/**
 * @file    tim.h
 * @brief   TIM2 定时采样节拍
 *
 * 设计：只在中断里置标志，真正的采样/计算/发送放到主循环。
 * 这是一个刻意的选择 —— 中断服务函数要尽可能短，否则会影响 SysTick 时基
 * 和其他中断的响应。若主循环一次没处理完，节拍会被合并并计数 overrun。
 */
#ifndef TIM_H
#define TIM_H

#include <stdint.h>

/**
 * @brief  初始化 TIM2
 * @param  hz 期望的节拍频率（本项目默认 200Hz）
 */
void     TIM2_Init(uint32_t hz);
uint8_t  TIM2_TakeFlag(void);
uint32_t TIM2_GetOverrun(void);
void     TIM2_SetPeriod(uint32_t hz);

#endif /* TIM_H */
