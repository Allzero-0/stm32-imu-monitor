/**
 * @file    stm32f103xb.h
 * @brief   STM32F103xB 系列外设寄存器映射（手写，非 ST 官方库）
 *
 * 覆盖本项目用到的外设：RCC / GPIO / AFIO / EXTI / USART / ADC / DMA / TIM
 * / CRC / FLASH / PWR / BKP / DBGMCU。
 * 参考文档：RM0008 Reference Manual (STM32F103xx)
 */
#ifndef STM32F103XB_H
#define STM32F103XB_H

#include <stdint.h>

#define __IO volatile
#define __O  volatile
#define __I  volatile const

/* ------------------------------------------------------------- 存储器基址 */
#define FLASH_BASE      (0x08000000UL)  /*!< FLASH 基址 */
#define SRAM_BASE       (0x20000000UL)  /*!< SRAM 基址 */
#define PERIPH_BASE     (0x40000000UL)  /*!< 外设基址 */

#define APB1PERIPH_BASE (PERIPH_BASE)
#define APB2PERIPH_BASE (PERIPH_BASE + 0x00010000UL)
#define AHBPERIPH_BASE  (PERIPH_BASE + 0x00020000UL)

/* APB2 外设 */
#define AFIO_BASE       (APB2PERIPH_BASE + 0x0000UL)
#define EXTI_BASE       (APB2PERIPH_BASE + 0x0400UL)
#define GPIOA_BASE      (APB2PERIPH_BASE + 0x0800UL)
#define GPIOB_BASE      (APB2PERIPH_BASE + 0x0C00UL)
#define GPIOC_BASE      (APB2PERIPH_BASE + 0x1000UL)
#define GPIOD_BASE      (APB2PERIPH_BASE + 0x1400UL)
#define GPIOE_BASE      (APB2PERIPH_BASE + 0x1800UL)
#define ADC1_BASE       (APB2PERIPH_BASE + 0x2400UL)
#define ADC2_BASE       (APB2PERIPH_BASE + 0x2800UL)
#define USART1_BASE     (APB2PERIPH_BASE + 0x3800UL)
#define SPI1_BASE       (APB2PERIPH_BASE + 0x3000UL)
#define TIM1_BASE       (APB2PERIPH_BASE + 0x2C00UL)

/* APB1 外设 */
#define TIM2_BASE       (APB1PERIPH_BASE + 0x0000UL)
#define TIM3_BASE       (APB1PERIPH_BASE + 0x0400UL)
#define TIM4_BASE       (APB1PERIPH_BASE + 0x0800UL)
#define RTC_BASE        (APB1PERIPH_BASE + 0x2800UL)
#define WWDG_BASE       (APB1PERIPH_BASE + 0x2C00UL)
#define IWDG_BASE       (APB1PERIPH_BASE + 0x3000UL)
#define USART2_BASE     (APB1PERIPH_BASE + 0x4400UL)
#define USART3_BASE     (APB1PERIPH_BASE + 0x4800UL)
#define I2C1_BASE       (APB1PERIPH_BASE + 0x5400UL)
#define I2C2_BASE       (APB1PERIPH_BASE + 0x5800UL)
#define CAN1_BASE       (APB1PERIPH_BASE + 0x6400UL)
#define BKP_BASE        (APB1PERIPH_BASE + 0x6C00UL)
#define PWR_BASE        (APB1PERIPH_BASE + 0x7000UL)

/* AHB 外设 */
#define SDIO_BASE       (AHBPERIPH_BASE + 0x0400UL)
#define DMA1_BASE       (AHBPERIPH_BASE + 0x0000UL)
#define DMA2_BASE       (AHBPERIPH_BASE + 0x0400UL)
#define RCC_BASE        (AHBPERIPH_BASE + 0x1000UL)
#define CRC_BASE        (AHBPERIPH_BASE + 0x3000UL)
#define FLASH_R_BASE    (AHBPERIPH_BASE + 0x2000UL)
#define DBGMCU_BASE     (0xE0042000UL)

/* ----------------------------------------------------------------- 结构体 */

/** @brief 复位与时钟控制 RCC */
typedef struct {
    __IO uint32_t CR;
    __IO uint32_t CFGR;
    __IO uint32_t CIR;
    __IO uint32_t APB2RSTR;
    __IO uint32_t APB1RSTR;
    __IO uint32_t AHBENR;
    __IO uint32_t APB2ENR;
    __IO uint32_t APB1ENR;
    __IO uint32_t BDCR;
    __IO uint32_t CSR;
    __IO uint32_t AHBRSTR;
    __IO uint32_t CFGR2;
} RCC_TypeDef;

/** @brief 通用 IO 口 GPIO */
typedef struct {
    __IO uint32_t CRL;   /*!< 端口配置低寄存器 (pin0~7) */
    __IO uint32_t CRH;   /*!< 端口配置高寄存器 (pin8~15) */
    __IO uint32_t IDR;   /*!< 输入数据寄存器 */
    __IO uint32_t ODR;   /*!< 输出数据寄存器 */
    __IO uint32_t BSRR;  /*!< 位设置/清除寄存器 */
    __IO uint32_t BRR;   /*!< 位清除寄存器 */
    __IO uint32_t LCKR;  /*!< 配置锁定寄存器 */
} GPIO_TypeDef;

/** @brief 复用功能 IO AFIO */
typedef struct {
    __IO uint32_t EVCR;
    __IO uint32_t MAPR;
    __IO uint32_t EXTICR[4];
    __IO uint32_t MAPR2;
} AFIO_TypeDef;

/** @brief 外部中断 EXTI */
typedef struct {
    __IO uint32_t IMR;
    __IO uint32_t EMR;
    __IO uint32_t RTSR;
    __IO uint32_t FTSR;
    __IO uint32_t SWIER;
    __IO uint32_t PR;
} EXTI_TypeDef;

/** @brief 通用同步异步收发器 USART */
typedef struct {
    __IO uint32_t SR;
    __IO uint32_t DR;
    __IO uint32_t BRR;
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t CR3;
    __IO uint32_t GTPR;
} USART_TypeDef;

/** @brief 模数转换器 ADC */
typedef struct {
    __IO uint32_t SR;
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t SMPR1;
    __IO uint32_t SMPR2;
    __IO uint32_t JOFR1;
    __IO uint32_t JOFR2;
    __IO uint32_t JOFR3;
    __IO uint32_t JOFR4;
    __IO uint32_t HTR;
    __IO uint32_t LTR;
    __IO uint32_t SQR1;
    __IO uint32_t SQR2;
    __IO uint32_t SQR3;
    __IO uint32_t JSQR;
    __IO uint32_t JDR1;
    __IO uint32_t JDR2;
    __IO uint32_t JDR3;
    __IO uint32_t JDR4;
    __IO uint32_t DR;
} ADC_TypeDef;

/** @brief DMA 通道 */
typedef struct {
    __IO uint32_t CCR;
    __IO uint32_t CNDTR;
    __IO uint32_t CPAR;
    __IO uint32_t CMAR;
} DMA_Channel_TypeDef;

/** @brief DMA 控制器公共寄存器 */
typedef struct {
    __IO uint32_t ISR;
    __IO uint32_t IFCR;
} DMA_TypeDef;

/** @brief 通用定时器 TIM2/3/4 */
typedef struct {
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t SMCR;
    __IO uint32_t DIER;
    __IO uint32_t SR;
    __IO uint32_t EGR;
    __IO uint32_t CCMR1;
    __IO uint32_t CCMR2;
    __IO uint32_t CCER;
    __IO uint32_t CNT;
    __IO uint32_t PSC;
    __IO uint32_t ARR;
    __IO uint32_t RESERVED1;
    __IO uint32_t CCR1;
    __IO uint32_t CCR2;
    __IO uint32_t CCR3;
    __IO uint32_t CCR4;
    __IO uint32_t RESERVED2;
    __IO uint32_t DCR;
    __IO uint32_t DMAR;
} TIM_TypeDef;

/** @brief FLASH 接口 */
typedef struct {
    __IO uint32_t ACR;
    __IO uint32_t KEYR;
    __IO uint32_t OPTKEYR;
    __IO uint32_t SR;
    __IO uint32_t CR;
    __IO uint32_t AR;
    __IO uint32_t RESERVED;
    __IO uint32_t OBR;
    __IO uint32_t WRPR;
} FLASH_TypeDef;

/** @brief CRC 计算单元 */
typedef struct {
    __IO uint32_t DR;
    __IO uint8_t  IDR;
    __IO uint8_t  RESERVED0;
    __IO uint16_t RESERVED1;
    __IO uint32_t CR;
} CRC_TypeDef;

/** @brief 电源控制 PWR */
typedef struct {
    __IO uint32_t CR;
    __IO uint32_t CSR;
} PWR_TypeDef;

/* ------------------------------------------------------------- 外设指针宏 */
#define RCC     ((RCC_TypeDef *)RCC_BASE)
#define GPIOA   ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB   ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC   ((GPIO_TypeDef *)GPIOC_BASE)
#define GPIOD   ((GPIO_TypeDef *)GPIOD_BASE)
#define GPIOE   ((GPIO_TypeDef *)GPIOE_BASE)
#define AFIO    ((AFIO_TypeDef *)AFIO_BASE)
#define EXTI    ((EXTI_TypeDef *)EXTI_BASE)
#define USART1  ((USART_TypeDef *)USART1_BASE)
#define USART2  ((USART_TypeDef *)USART2_BASE)
#define ADC1    ((ADC_TypeDef *)ADC1_BASE)
#define ADC2    ((ADC_TypeDef *)ADC2_BASE)
#define TIM1    ((TIM_TypeDef *)TIM1_BASE)
#define TIM2    ((TIM_TypeDef *)TIM2_BASE)
#define TIM3    ((TIM_TypeDef *)TIM3_BASE)
#define TIM4    ((TIM_TypeDef *)TIM4_BASE)
#define FLASH   ((FLASH_TypeDef *)FLASH_R_BASE)
#define CRC     ((CRC_TypeDef *)CRC_BASE)
#define PWR     ((PWR_TypeDef *)PWR_BASE)
#define DBGMCU  ((__IO uint32_t *)DBGMCU_BASE)

#define DMA1    ((DMA_TypeDef *)DMA1_BASE)
#define DMA2    ((DMA_TypeDef *)DMA2_BASE)

/* DMA 通道：通道寄存器从偏移 0x08 开始，每个通道占 0x14 字节 */
#define DMA1_Channel1 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x08UL))
#define DMA1_Channel2 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x1CUL))
#define DMA1_Channel3 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x30UL))
#define DMA1_Channel4 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x44UL))
#define DMA1_Channel5 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x58UL))
#define DMA1_Channel6 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x6CUL))
#define DMA1_Channel7 ((DMA_Channel_TypeDef *)(DMA1_BASE + 0x80UL))

/* ------------------------------------------------------------ RCC 位定义 */
#define RCC_CR_HSION_Pos     0U
#define RCC_CR_HSION_Msk     (1UL << 0U)
#define RCC_CR_HSIRDY_Pos    1U
#define RCC_CR_HSIRDY_Msk    (1UL << 1U)
#define RCC_CR_HSEON_Pos     16U
#define RCC_CR_HSEON_Msk     (1UL << 16U)
#define RCC_CR_HSERDY_Pos    17U
#define RCC_CR_HSERDY_Msk    (1UL << 17U)
#define RCC_CR_HSEBYP_Pos    18U
#define RCC_CR_HSEBYP_Msk    (1UL << 18U)
#define RCC_CR_CSSON_Pos     19U
#define RCC_CR_CSSON_Msk     (1UL << 19U)
#define RCC_CR_PLLON_Pos     24U
#define RCC_CR_PLLON_Msk     (1UL << 24U)
#define RCC_CR_PLLRDY_Pos    25U
#define RCC_CR_PLLRDY_Msk    (1UL << 25U)

#define RCC_CFGR_SW_Pos      0U
#define RCC_CFGR_SW_Msk      (3UL << 0U)
#define RCC_CFGR_SWS_Pos     2U
#define RCC_CFGR_SWS_Msk     (3UL << 2U)
#define RCC_CFGR_HPRE_Pos    4U
#define RCC_CFGR_PPRE1_Pos   8U
#define RCC_CFGR_PPRE2_Pos   11U
#define RCC_CFGR_ADCPRE_Pos  14U
#define RCC_CFGR_PLLSRC_Pos  16U
#define RCC_CFGR_PLLXTPRE_Pos 17U
#define RCC_CFGR_PLLMUL_Pos  18U
#define RCC_CFGR_USBPRE_Pos  22U
#define RCC_CFGR_MCO_Pos     24U

/* APB2ENR 外设时钟使能位 */
#define RCC_APB2ENR_AFIOEN   (1UL << 0U)
#define RCC_APB2ENR_IOPAEN   (1UL << 2U)
#define RCC_APB2ENR_IOPBEN   (1UL << 3U)
#define RCC_APB2ENR_IOPCEN   (1UL << 4U)
#define RCC_APB2ENR_IOPDEN   (1UL << 5U)
#define RCC_APB2ENR_ADC1EN   (1UL << 9U)
#define RCC_APB2ENR_ADC2EN   (1UL << 10U)
#define RCC_APB2ENR_TIM1EN   (1UL << 11U)
#define RCC_APB2ENR_SPI1EN   (1UL << 12U)
#define RCC_APB2ENR_USART1EN (1UL << 14U)

/* APB1ENR 外设时钟使能位 */
#define RCC_APB1ENR_TIM2EN   (1UL << 0U)
#define RCC_APB1ENR_TIM3EN   (1UL << 1U)
#define RCC_APB1ENR_TIM4EN   (1UL << 2U)
#define RCC_APB1ENR_WWDGEN   (1UL << 11U)
#define RCC_APB1ENR_USART2EN (1UL << 17U)
#define RCC_APB1ENR_USART3EN (1UL << 18U)
#define RCC_APB1ENR_I2C1EN   (1UL << 21U)
#define RCC_APB1ENR_I2C2EN   (1UL << 22U)
#define RCC_APB1ENR_CAN1EN   (1UL << 25U)
#define RCC_APB1ENR_BKPEN    (1UL << 27U)
#define RCC_APB1ENR_PWREN    (1UL << 28U)

/* AHBENR 外设时钟使能位 */
#define RCC_AHBENR_DMA1EN    (1UL << 0U)
#define RCC_AHBENR_DMA2EN    (1UL << 1U)
#define RCC_AHBENR_SRAMEN    (1UL << 2U)
#define RCC_AHBENR_FLITFEN   (1UL << 4U)
#define RCC_AHBENR_CRCEN     (1UL << 6U)

/* ----------------------------------------------------------- GPIO 位定义 */
/* CRL/CRH 每引脚 4 bit：CNF[1:0] + MODE[1:0] */
#define GPIO_MODE_INPUT      0x0U
#define GPIO_MODE_OUTPUT_10M 0x1U
#define GPIO_MODE_OUTPUT_2M  0x2U
#define GPIO_MODE_OUTPUT_50M 0x3U

#define GPIO_CNF_ANALOG      0x0U
#define GPIO_CNF_FLOATING    0x1U
#define GPIO_CNF_PULL        0x2U   /* 输入模式下为上下拉 */
#define GPIO_CNF_AF_PP       0x2U   /* 输出模式下为复用推挽 */
#define GPIO_CNF_AF_OD       0x3U   /* 输出模式下为复用开漏 */
#define GPIO_CNF_OUT_PP      0x0U   /* 通用推挽输出 */
#define GPIO_CNF_OUT_OD      0x1U   /* 通用开漏输出 */

/* --------------------------------------------------------- USART 位定义 */
#define USART_SR_TXE         (1UL << 7U)   /* 发送数据寄存器空 */
#define USART_SR_TC          (1UL << 6U)   /* 发送完成 */
#define USART_SR_RXNE        (1UL << 5U)   /* 接收数据寄存器非空 */
#define USART_SR_IDLE        (1UL << 4U)
#define USART_SR_ORE         (1UL << 3U)
#define USART_SR_NE          (1UL << 2U)
#define USART_SR_FE          (1UL << 1U)
#define USART_SR_PE          (1UL << 0U)

#define USART_CR1_UE         (1UL << 13U)
#define USART_CR1_M          (1UL << 12U)
#define USART_CR1_PCE        (1UL << 10U)
#define USART_CR1_TXEIE      (1UL << 7U)
#define USART_CR1_TCIE       (1UL << 6U)
#define USART_CR1_RXNEIE     (1UL << 5U)
#define USART_CR1_IDLEIE     (1UL << 4U)
#define USART_CR1_TE         (1UL << 3U)
#define USART_CR1_RE         (1UL << 2U)

#define USART_CR3_DMAT       (1UL << 7U)
#define USART_CR3_DMAR       (1UL << 6U)

/* ----------------------------------------------------------- ADC 位定义 */
#define ADC_SR_EOC           (1UL << 1U)   /* 规则通道转换结束 */
#define ADC_SR_STRT          (1UL << 4U)
#define ADC_CR1_SCAN         (1UL << 8U)
#define ADC_CR1_EOCIE        (1UL << 5U)
#define ADC_CR2_ADON         (1UL << 0U)
#define ADC_CR2_CONT         (1UL << 1U)
#define ADC_CR2_CAL          (1UL << 2U)
#define ADC_CR2_RSTCAL       (1UL << 3U)
#define ADC_CR2_DMA          (1UL << 8U)
#define ADC_CR2_ALIGN        (1UL << 11U)
#define ADC_CR2_EXTSEL_Pos   17U
#define ADC_CR2_EXTTRIG      (1UL << 20U)
#define ADC_CR2_SWSTART      (1UL << 22U)
#define ADC_CR2_TSVREFE      (1UL << 23U)

/* ----------------------------------------------------------- DMA 位定义 */
#define DMA_CCR_EN           (1UL << 0U)
#define DMA_CCR_TCIE_Pos     1U
#define DMA_CCR_TCIE         (1UL << 1U)
#define DMA_CCR_HTIE         (1UL << 2U)
#define DMA_CCR_TEIE         (1UL << 3U)
#define DMA_CCR_DIR_Pos      4U
#define DMA_CCR_DIR          (1UL << 4U)
#define DMA_CCR_CIRC_Pos     5U
#define DMA_CCR_CIRC         (1UL << 5U)
#define DMA_CCR_PINC         (1UL << 6U)
#define DMA_CCR_MINC_Pos     7U
#define DMA_CCR_MINC         (1UL << 7U)
#define DMA_CCR_PSIZE_Pos    8U
#define DMA_CCR_MSIZE_Pos    10U
#define DMA_CCR_PL_Pos       12U

#define DMA_IFCR_CGIF1       (1UL << 0U)
#define DMA_IFCR_CTCIF1      (1UL << 1U)
#define DMA_IFCR_CHTIF1      (1UL << 2U)
#define DMA_IFCR_CTEIF1      (1UL << 3U)

/* ----------------------------------------------------------- TIM 位定义 */
#define TIM_CR1_CEN          (1UL << 0U)
#define TIM_CR1_URS          (1UL << 2U)
#define TIM_CR1_ARPE         (1UL << 7U)
#define TIM_DIER_UIE         (1UL << 0U)
#define TIM_SR_UIF           (1UL << 0U)
#define TIM_EGR_UG           (1UL << 0U)

/* --------------------------------------------------------- FLASH 位定义 */
#define FLASH_ACR_LATENCY_Pos 0U
#define FLASH_ACR_PRFTBE     (1UL << 4U)

#endif /* STM32F103XB_H */
