/**
 * @file    adc_dma.h
 * @brief   ADC1 多通道扫描 + DMA 循环搬运
 *
 * 通道安排（3 路规则通道）：
 *   [0] PA0 / ADC_CH0  —— 外部模拟信号（电位器、传感器输出）
 *   [1] PA1 / ADC_CH1  —— 外部模拟信号（光敏 / 电压监测）
 *   [2] CH16           —— 芯片内部温度传感器
 *
 * 工作方式：连续转换 + DMA 循环模式，硬件自动刷新缓冲区，CPU 随时读取
 * 最新值即可，不占用任何中断和 CPU 时间。
 */
#ifndef ADC_DMA_H
#define ADC_DMA_H

#include <stdint.h>

/** 规则通道数量 */
#define ADC_CHANNEL_COUNT   3U

/** 缓冲区索引 */
#define ADC_IDX_EXT0        0U
#define ADC_IDX_EXT1        1U
#define ADC_IDX_CHIP_TEMP   2U

void     ADC1_DMA_Init(void);
uint16_t ADC_GetRaw(uint8_t idx);
uint16_t ADC_GetMilliVolts(uint8_t idx);
int16_t  ADC_GetChipTempX10(void);

#endif /* ADC_DMA_H */
