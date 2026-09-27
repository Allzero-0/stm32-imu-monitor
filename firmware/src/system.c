/**
 * @file    system.c
 * @brief   系统时钟配置与时基
 *
 * 时钟树（HSE 8MHz 可用时）：
 *   HSE 8MHz --[PLL x9]--> SYSCLK 72MHz
 *     AHB  = 72MHz (HCLK)
 *     APB1 = 36MHz (PCLK1，<=36MHz 上限)
 *     APB2 = 72MHz (PCLK2)
 *     ADC  = PCLK2 / 6 = 12MHz (<=14MHz 上限)
 *
 * 若外部晶振未起振（部分廉价最小系统板晶振虚焊/未焊接），自动回退到
 * 内部 HSI：HSI/2 x16 = 64MHz，保证程序在任何板子上都能跑起来而不是"白屏"。
 * 这一点在面试里讲很加分：知道硬件有差异，软件要能容错。
 */
#include "system.h"
#include "stm32f103xb.h"
#include "core_cm3.h"

/* 系统核心时钟频率（Hz），由 SystemInit 更新 */
uint32_t SystemCoreClock = 8000000U;

/* 毫秒计数（SysTick 中断累加） */
static volatile uint32_t s_tick_ms = 0U;

/* 微秒级延时用的每循环周期数（粗标定，够用即可） */
static uint32_t s_us_loop = 0U;

/**
 * @brief  尝试启动 HSE
 * @retval 1 成功，0 超时失败
 */
static uint8_t HSE_Start(void)
{
    uint32_t timeout = 0x00080000UL;   /* 约几十毫秒，足够晶振起振 */

    RCC->CR |= RCC_CR_HSEON_Msk;
    while (((RCC->CR & RCC_CR_HSERDY_Msk) == 0U) && (timeout-- > 0U)) {
        __asm volatile ("nop");
    }
    return (RCC->CR & RCC_CR_HSERDY_Msk) ? 1U : 0U;
}

/**
 * @brief  配置系统时钟，并使能 Flash 预取、设置等待周期
 * @note   由启动文件 Reset_Handler 调用，早于 main
 */
void SystemInit(void)
{
    uint32_t use_hse = 0U;

    /* 1. 复位时钟配置为默认状态：HSI 打开，PLL 关闭 */
    RCC->CR |= RCC_CR_HSION_Msk;
    RCC->CFGR = 0x00000000UL;
    RCC->CR &= ~(RCC_CR_PLLON_Msk | RCC_CR_CSSON_Msk | RCC_CR_HSEON_Msk | RCC_CR_HSEBYP_Msk);
    RCC->CIR = 0x00000000UL;   /* 关闭所有时钟中断 */

    /* 2. 打开 Flash 预取缓冲 + 2 个等待周期（48MHz < SYSCLK <= 72MHz） */
    FLASH->ACR |= FLASH_ACR_PRFTBE | (2UL << FLASH_ACR_LATENCY_Pos);

    /* 3. 尝试外部晶振 */
    use_hse = HSE_Start();

    if (use_hse) {
        /* HSE 不分频直接进 PLL，9 倍频 -> 72MHz */
        RCC->CFGR &= ~(0xFUL << RCC_CFGR_PLLMUL_Pos);
        RCC->CFGR |= ((9UL - 2UL) << RCC_CFGR_PLLMUL_Pos);  /* PLLMUL 编码 = 倍频数 - 2 */
        RCC->CFGR |= (1UL << RCC_CFGR_PLLSRC_Pos);           /* PLLSRC = HSE */
        RCC->CFGR &= ~(1UL << RCC_CFGR_PLLXTPRE_Pos);        /* HSE 不分频 */
        SystemCoreClock = 72000000U;
    } else {
        /* HSE 不可用：关闭 HSE，用 HSI/2 进 PLL，16 倍频 -> 64MHz */
        RCC->CR &= ~RCC_CR_HSEON_Msk;
        RCC->CFGR &= ~((0xFUL << RCC_CFGR_PLLMUL_Pos) | (1UL << RCC_CFGR_PLLSRC_Pos));
        RCC->CFGR |= ((16UL - 2UL) << RCC_CFGR_PLLMUL_Pos);  /* PLLSRC=0 表示 HSI/2 */
        SystemCoreClock = 64000000U;
    }

    /* 4. 总线分频：AHB 不分频，APB1 /2，APB2 不分频，ADC /6，USB /1.5 */
    RCC->CFGR &= ~((0xFUL << RCC_CFGR_HPRE_Pos) | (7UL << RCC_CFGR_PPRE1_Pos) |
                   (7UL << RCC_CFGR_PPRE2_Pos) | (3UL << RCC_CFGR_ADCPRE_Pos));
    RCC->CFGR |= (4UL << RCC_CFGR_PPRE1_Pos);    /* PPRE1 = 100 -> HCLK/2 */
    RCC->CFGR |= (2UL << RCC_CFGR_ADCPRE_Pos);   /* ADCPRE = 10 -> PCLK2/6 */
    RCC->CFGR |= (0UL << RCC_CFGR_USBPRE_Pos);   /* USB 预分频 1.5 -> 48MHz */

    /* 5. 启动 PLL 并等待锁定 */
    RCC->CR |= RCC_CR_PLLON_Msk;
    while ((RCC->CR & RCC_CR_PLLRDY_Msk) == 0U) {
        __asm volatile ("nop");
    }

    /* 6. 切换系统时钟源到 PLL，等待切换完成 */
    RCC->CFGR &= ~(3UL << RCC_CFGR_SW_Pos);
    RCC->CFGR |= (2UL << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR >> RCC_CFGR_SWS_Pos) & 0x3UL) != 0x2UL) {
        __asm volatile ("nop");
    }

    /* 7. 中断优先级分组：4 位抢占优先级，0 位子优先级 */
    NVIC_SetPriorityGrouping(0x3UL);

    /* 8. 微秒延时粗标定：约 1us 需要的循环次数，按每循环约 4 个时钟周期估算 */
    s_us_loop = SystemCoreClock / 4000000U;
    if (s_us_loop == 0U) {
        s_us_loop = 1U;
    }
}

/**
 * @brief  配置 SysTick 为 1ms 中断
 * @retval 0 成功，1 失败（重装载值超出 24 位）
 */
uint32_t SysTick_Config_1ms(void)
{
    uint32_t reload = SystemCoreClock / 1000U;

    if (reload > 0x00FFFFFFUL) {
        return 1U;
    }

    SysTick->LOAD = reload - 1U;
    SysTick->VAL  = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |   /* 内核时钟 */
                    SysTick_CTRL_TICKINT_Msk   |   /* 开启中断 */
                    SysTick_CTRL_ENABLE_Msk;       /* 使能计数 */
    NVIC_SetPriority(SysTick_IRQn, 0U);
    return 0U;
}

/**
 * @brief  获取上电后经过的毫秒数
 */
uint32_t millis(void)
{
    return s_tick_ms;
}

/**
 * @brief  微秒级阻塞延时（基于循环，中断会打断它，仅用于短延时）
 */
void delay_us(uint32_t us)
{
    volatile uint32_t n = us * s_us_loop;
    while (n-- > 0U) {
        __asm volatile ("nop");
    }
}

/**
 * @brief  毫秒级阻塞延时
 */
void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        __asm volatile ("nop");
    }
}

/**
 * @brief  SysTick 中断服务函数：1ms 节拍
 */
void SysTick_Handler(void)
{
    s_tick_ms++;
}
