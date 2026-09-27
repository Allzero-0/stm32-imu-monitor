/**
 * @file    usart.c
 * @brief   USART1 驱动实现（PA9-TX / PA10-RX）
 */
#include "usart.h"
#include "stm32f103xb.h"
#include "core_cm3.h"
#include "system.h"

/* ------------------------------------------------------------ 内部状态 */
static volatile uint8_t  s_rx_buf[USART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0U;   /* 中断写入位置 */
static volatile uint16_t s_rx_tail = 0U;   /* 主循环读出位置 */
static volatile uint16_t s_dropped = 0U;   /* 因发送忙被丢弃的帧数 */
static volatile uint8_t  s_tx_busy = 0U;

/**
 * @brief  初始化 USART1
 * @param  baud 波特率（推荐 115200 或 230400）
 */
void USART1_Init(uint32_t baud)
{
    uint32_t pclk2 = SystemCoreClock;   /* APB2 不分频，PCLK2 = SYSCLK */
    uint32_t usartdiv;

    /* 1. 开时钟：GPIOA / USART1 / AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_AFIOEN;

    /* 2. PA9 复用推挽输出 50MHz，PA10 浮空/上拉输入 */
    GPIOA->CRH &= ~((0xFUL << 4U) | (0xFUL << 8U));        /* 清 PA9(bit4~7) PA10(bit8~11) */
    GPIOA->CRH |=  ((0xBUL << 4U));                        /* PA9: CNF=10 AF PP, MODE=11 50MHz */
    GPIOA->CRH |=  ((0x8UL << 8U));                        /* PA10: CNF=10 上/下拉输入 */
    GPIOA->BSRR = (1UL << 10U);                            /* PA10 上拉 */

    /* 3. 波特率：BRR = PCLK2 / baud（16 倍过采样） */
    usartdiv = (pclk2 + (baud / 2U)) / baud;
    USART1->BRR = (uint16_t)usartdiv;

    /* 4. 使能 USART、发送、接收、接收中断 */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    USART1->CR2 = 0UL;
    USART1->CR3 = USART_CR3_DMAT;      /* 发送使用 DMA */

    NVIC_SetPriority(USART1_IRQn, 2U);
    NVIC_EnableIRQ(USART1_IRQn);

    /* 5. 配置 DMA1_Channel4：内存 -> USART1->DR，8 位，内存地址递增 */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    DMA1_Channel4->CCR = 0UL;
    DMA1_Channel4->CCR = (1UL << DMA_CCR_DIR_Pos) |     /* 从内存读 */
                         (1UL << DMA_CCR_MINC_Pos) |    /* 内存地址递增 */
                         (1UL << DMA_CCR_TCIE_Pos) |    /* 传输完成中断 */
                         (1UL << DMA_CCR_PL_Pos);       /* 优先级 Medium */
    DMA1_Channel4->CPAR = (uint32_t)&(USART1->DR);

    NVIC_SetPriority(DMA1_Channel4_IRQn, 2U);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

/**
 * @brief  阻塞发送字符串（仅用于启动日志/调试，正式数据流不用它）
 */
void USART1_SendString(const char *str)
{
    while (*str != '\0') {
        while ((USART1->SR & USART_SR_TXE) == 0U) {
            /* 等待发送寄存器空 */
        }
        USART1->DR = (uint16_t)(*str);
        str++;
    }
}

/**
 * @brief  非阻塞 DMA 发送
 * @param  data 数据缓冲（调用后到发送完成前不要修改！）
 * @param  len  长度
 * @retval  0 已启动发送；-1 上一帧还没发完，本次丢弃
 */
int USART1_SendDMA(const uint8_t *data, uint16_t len)
{
    if (s_tx_busy != 0U) {
        s_dropped++;
        return -1;
    }
    if ((data == 0) || (len == 0U)) {
        return -1;
    }

    s_tx_busy = 1U;

    DMA1_Channel4->CCR &= ~DMA_CCR_EN;         /* 先关闭通道再配置 */
    DMA1->IFCR = (0xFUL << 12U);               /* 清除通道 4 的中断标志 */
    DMA1_Channel4->CNDTR = len;
    DMA1_Channel4->CMAR  = (uint32_t)data;
    DMA1_Channel4->CCR  |= DMA_CCR_EN;         /* 启动 */
    return 0;
}

/**
 * @brief  查询发送是否仍在进行
 */
uint8_t USART1_TxBusy(void)
{
    return s_tx_busy;
}

/**
 * @brief  获取被丢弃的帧数（用于评估串口带宽是否够用）
 */
uint16_t USART1_GetDropped(void)
{
    return s_dropped;
}

/**
 * @brief  接收缓冲区中可读字节数
 */
uint16_t USART1_RxAvailable(void)
{
    uint16_t head = s_rx_head;
    uint16_t tail = s_rx_tail;
    if (head >= tail) {
        return head - tail;
    }
    return (USART_RX_BUF_SIZE - tail + head);
}

/**
 * @brief  从接收缓冲区取一个字节
 * @retval >=0 数据；-1 缓冲区为空
 */
int USART1_RxGetByte(void)
{
    uint8_t b;

    if (USART1_RxAvailable() == 0U) {
        return -1;
    }
    b = s_rx_buf[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) % USART_RX_BUF_SIZE);
    return (int)b;
}

/* --------------------------------------------------------- 中断服务函数 */

/**
 * @brief  USART1 接收中断：写入环形缓冲
 */
void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    if ((sr & USART_SR_RXNE) != 0U) {
        uint8_t b = (uint8_t)(USART1->DR & 0xFFU);
        uint16_t next = (uint16_t)((s_rx_head + 1U) % USART_RX_BUF_SIZE);
        if (next != s_rx_tail) {          /* 缓冲未满才写入，满则丢弃新数据 */
            s_rx_buf[s_rx_head] = b;
            s_rx_head = next;
        }
    }

    if ((sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U) {
        /* 发生溢出/噪声/帧错误：读 DR 清标志 */
        (void)USART1->DR;
    }
}

/**
 * @brief  DMA1_Channel4 传输完成中断：USART1 发送结束
 */
void DMA1_Channel4_IRQHandler(void)
{
    if ((DMA1->ISR & (1UL << 13U)) != 0U) {     /* TCIF4 */
        DMA1->IFCR = (0xFUL << 12U);            /* 清通道 4 全部标志 */
        DMA1_Channel4->CCR &= ~DMA_CCR_EN;
        s_tx_busy = 0U;
    }
}
