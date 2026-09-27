/**
 * @file    tim.c
 * @brief   TIM2 定时节拍实现
 */
#include "tim.h"
#include "stm32f103xb.h"
#include "core_cm3.h"
#include "system.h"

static volatile uint8_t  s_tick_flag = 0U;
static volatile uint32_t s_overrun   = 0U;
static volatile uint32_t s_tick_count = 0U;

/**
 * @brief  初始化 TIM2 为指定频率的更新中断
 *
 * 定时器时钟说明：APB1 预分频为 2 时，定时器时钟 = PCLK1 × 2 = SYSCLK。
 * 计数频率 = TIM2CLK / (PSC+1)，溢出频率 = 计数频率 / (ARR+1)。
 */
void TIM2_Init(uint32_t hz)
{
    /* APB1 预分频不为 1，因此 TIM2 时钟 = PCLK1 × 2 = SYSCLK */
    uint32_t tim_clk = SystemCoreClock;
    uint32_t cnt_hz;
    uint32_t psc;
    uint32_t arr;

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* 先分频到 1MHz 计数频率（对 72MHz 和 64MHz 主频都成立） */
    psc = (tim_clk / 1000000UL);
    if (psc == 0UL) {
        psc = 1UL;
    }
    psc -= 1UL;
    if (psc > 0xFFFFUL) {
        psc = 0xFFFFUL;
    }
    cnt_hz = tim_clk / (psc + 1UL);

    if (hz == 0U) {
        hz = 1U;
    }
    arr = (cnt_hz / hz);
    if (arr > 0UL) {
        arr -= 1UL;
    }

    TIM2->PSC = psc;
    TIM2->ARR = arr;
    TIM2->CNT = 0U;
    TIM2->CR1 = TIM_CR1_ARPE;       /* 自动重装载预装载使能 */
    TIM2->DIER = TIM_DIER_UIE;      /* 更新中断 */
    TIM2->SR = 0U;                  /* 清标志 */

    NVIC_SetPriority(TIM2_IRQn, 1U);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;       /* 启动计数 */
}

/**
 * @brief  动态修改节拍频率
 */
void TIM2_SetPeriod(uint32_t hz)
{
    uint32_t arr = (hz == 0U) ? 999U : ((1000000UL / hz) - 1U);
    TIM2->ARR = arr;
    TIM2->CNT = 0U;
}

/**
 * @brief  取走一次节拍标志（取后自动清除）
 * @retval 1 有待处理的节拍；0 无
 */
uint8_t TIM2_TakeFlag(void)
{
    uint8_t f;
    __disable_irq();
    f = s_tick_flag;
    s_tick_flag = 0U;
    __enable_irq();
    return f;
}

/**
 * @brief  获取主循环未及时处理而被合并掉的节拍数
 */
uint32_t TIM2_GetOverrun(void)
{
    return s_overrun;
}

/**
 * @brief  TIM2 更新中断：仅置标志，尽量短
 */
void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0U) {
        TIM2->SR &= ~TIM_SR_UIF;      /* 必须手动清中断标志 */
        s_tick_count++;
        if (s_tick_flag != 0U) {
            s_overrun++;              /* 上一拍还没被处理，说明主循环跟不上 */
        }
        s_tick_flag = 1U;
    }
}
