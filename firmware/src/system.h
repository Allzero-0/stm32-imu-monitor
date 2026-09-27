/**
 * @file    system.h
 * @brief   系统时钟与时基接口
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

/** 当前系统核心时钟频率（Hz），SystemInit() 后有效 */
extern uint32_t SystemCoreClock;

void     SystemInit(void);
uint32_t SysTick_Config_1ms(void);
uint32_t millis(void);
void     delay_us(uint32_t us);
void     delay_ms(uint32_t ms);

#endif /* SYSTEM_H */
