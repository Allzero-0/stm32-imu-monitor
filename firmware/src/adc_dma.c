/**
 * @file    adc_dma.c
 * @brief   ADC1 多通道扫描 + DMA 循环模式实现
 */
#include "adc_dma.h"
#include "stm32f103xb.h"
#include "system.h"

/* DMA 循环搬运的目标缓冲区（半字对齐，与 ADC 数据寄存器同宽） */
static volatile uint16_t s_adc_buf[ADC_CHANNEL_COUNT] __attribute__((aligned(4)));

/**
 * @brief  初始化 ADC1 + DMA1_Channel1
 *
 * 步骤：开时钟 -> PA0/PA1 模拟输入 -> DMA 配置 -> ADC 校准 ->
 *       设置扫描序列与采样时间 -> 打开连续转换与 DMA -> 软件启动
 */
void ADC1_DMA_Init(void)
{
    uint32_t i;

    /* 1. 时钟：GPIOA、ADC1、DMA1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
    RCC->AHBENR  |= RCC_AHBENR_DMA1EN;

    /* ADC 时钟 = PCLK2 / 6 （必须 <= 14MHz） */
    RCC->CFGR &= ~(3UL << RCC_CFGR_ADCPRE_Pos);
    RCC->CFGR |= (2UL << RCC_CFGR_ADCPRE_Pos);

    /* 2. PA0 / PA1 配置为模拟输入（CRL 中 CNF=00 MODE=00） */
    GPIOA->CRL &= ~((0xFUL << 0U) | (0xFUL << 4U));

    /* 3. DMA1_Channel1：外设 16 位、内存 16 位、循环模式、内存递增 */
    DMA1_Channel1->CCR = 0UL;
    DMA1_Channel1->CCR = (1UL << DMA_CCR_CIRC_Pos) |       /* 循环模式 */
                         (1UL << DMA_CCR_MINC_Pos) |       /* 内存地址递增 */
                         (1UL << DMA_CCR_PSIZE_Pos) |      /* 外设 16 位 */
                         (1UL << DMA_CCR_MSIZE_Pos);       /* 内存 16 位 */
    DMA1_Channel1->CPAR = (uint32_t)&(ADC1->DR);
    DMA1_Channel1->CMAR = (uint32_t)s_adc_buf;
    DMA1_Channel1->CNDTR = ADC_CHANNEL_COUNT;

    /* 4. ADC 上电并校准（F1 的 ADC 使用前必须校准，否则误差可达几十 LSB） */
    ADC1->CR2 |= ADC_CR2_ADON;             /* 退出掉电 */
    delay_us(10U);
    ADC1->CR2 |= ADC_CR2_RSTCAL;           /* 复位校准寄存器 */
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) != 0U) { }
    ADC1->CR2 |= ADC_CR2_CAL;              /* 开始校准 */
    while ((ADC1->CR2 & ADC_CR2_CAL) != 0U) { }

    /* 5. 通道配置
     *    采样时间统一取 239.5 个 ADC 周期（约 20us @12MHz），
     *    内部温度传感器要求 >= 17.1us，必须满足否则读数偏小。 */
    ADC1->SMPR2 |= (7UL << 0U) | (7UL << 3U);      /* 通道 0、1 */
    ADC1->SMPR1 |= (7UL << 18U);                   /* 通道 16（位于 SMPR1） */

    /* 6. 规则序列：SQ1 = CH0, SQ2 = CH1, SQ3 = CH16 */
    ADC1->SQR3 = (0UL << 0U) | (1UL << 5U) | (16UL << 10U);
    ADC1->SQR2 = 0UL;
    ADC1->SQR1 = ((ADC_CHANNEL_COUNT - 1U) << 20U);   /* L = 转换通道数 - 1 */

    /* 7. 扫描模式 + 连续转换 + DMA + 内部温度通道使能 */
    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->CR2 |= ADC_CR2_CONT | ADC_CR2_DMA | ADC_CR2_TSVREFE;
    ADC1->CR2 &= ~(ADC_CR2_ALIGN);                    /* 右对齐 */

    /* 8. 启动 DMA 后再启动转换 */
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    delay_us(10U);

    /* 首次软件启动，之后 CONT=1 会自动连续转换 */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /* 等待第一组数据填充完成 */
    for (i = 0U; i < 100000UL; i++) {
        if ((DMA1->ISR & (1UL << 1U)) != 0U) {   /* TCIF1 */
            break;
        }
    }
    DMA1->IFCR = (0xFUL << 0U);
}

/**
 * @brief  读取指定通道的最新原始值（12 位，0~4095）
 */
uint16_t ADC_GetRaw(uint8_t idx)
{
    if (idx >= ADC_CHANNEL_COUNT) {
        return 0U;
    }
    return (uint16_t)s_adc_buf[idx];
}

/**
 * @brief  换算为毫伏（按 VDDA = 3.3V 计算）
 */
uint16_t ADC_GetMilliVolts(uint8_t idx)
{
    uint32_t mv = ((uint32_t)ADC_GetRaw(idx) * 3300UL) / 4095UL;
    return (uint16_t)mv;
}

/**
 * @brief  换算芯片内部温度（单位：0.1°C）
 *
 * 公式（RM0008）：T = (V25 - Vsense) / Avg_Slope + 25
 *   V25       = 1.43 V（25°C 时的传感器输出）
 *   Avg_Slope = 4.3 mV/°C
 * 注意：内部温度传感器精度约 ±1.5°C，只适合测芯片温升趋势，
 *       不能当环境温度计 —— 这是数据手册明确写的，面试时别说错。
 */
int16_t ADC_GetChipTempX10(void)
{
    int32_t vsense_mv = (int32_t)ADC_GetMilliVolts(ADC_IDX_CHIP_TEMP);
    int32_t t10;

    t10 = ((1430 - vsense_mv) * 10) / 43 + 250;    /* (mV)/4.3mV 换算成 0.1°C */
    return (int16_t)t10;
}
