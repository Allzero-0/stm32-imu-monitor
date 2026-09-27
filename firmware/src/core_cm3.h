/**
 * @file    core_cm3.h
 * @brief   精简版 Cortex-M3 内核外设定义（NVIC / SCB / SysTick）
 *
 * 说明：本项目不依赖 ST 官方 CMSIS 库，内核相关寄存器与访问函数在此文件中
 *      手写实现，目的是让整个工程"零外部依赖、可离线编译"，同时逼自己
 *      搞清楚每一个寄存器到底在干什么（而不是调 HAL 库黑盒）。
 */
#ifndef CORE_CM3_H
#define CORE_CM3_H

#include <stdint.h>

/* ---------------------------------------------------------------- 基本类型 */
#define __IO   volatile
#define __O    volatile
#define __I    volatile const

/* ------------------------------------------------- 内核私有外设总线(PPB) */

/** @brief 系统控制块 SCB */
typedef struct {
    __I  uint32_t CPUID;    /*!< 0x00 CPU ID 基址寄存器 */
    __IO uint32_t ICSR;     /*!< 0x04 中断控制与状态寄存器 */
    __IO uint32_t VTOR;     /*!< 0x08 向量表偏移寄存器 */
    __IO uint32_t AIRCR;    /*!< 0x0C 应用中断与复位控制寄存器 */
    __IO uint32_t SCR;      /*!< 0x10 系统控制寄存器 */
    __IO uint32_t CCR;      /*!< 0x14 配置与控制寄存器 */
    __IO uint8_t  SHPR[12]; /*!< 0x18 系统异常优先级寄存器 */
    __IO uint32_t SHCSR;    /*!< 0x24 系统异常控制与状态寄存器 */
    __IO uint32_t CFSR;     /*!< 0x28 可配置故障状态寄存器 */
    __IO uint32_t HFSR;     /*!< 0x2C 硬故障状态寄存器 */
    __IO uint32_t DFSR;     /*!< 0x30 调试故障状态寄存器 */
    __IO uint32_t MMFAR;    /*!< 0x34 存储管理故障地址寄存器 */
    __IO uint32_t BFAR;     /*!< 0x38 总线故障地址寄存器 */
    __IO uint32_t AFSR;     /*!< 0x3C 辅助故障状态寄存器 */
} SCB_Type;

/** @brief 嵌套向量中断控制器 NVIC */
typedef struct {
    __IO uint32_t ISER[8];   /*!< 0x100 中断使能寄存器 */
         uint32_t RES0[24];
    __IO uint32_t ICER[8];   /*!< 0x180 中断清除使能寄存器 */
         uint32_t RES1[24];
    __IO uint32_t ISPR[8];   /*!< 0x200 中断挂起设置寄存器 */
         uint32_t RES2[24];
    __IO uint32_t ICPR[8];   /*!< 0x280 中断挂起清除寄存器 */
         uint32_t RES3[24];
    __IO uint32_t IABR[8];   /*!< 0x300 中断活跃位寄存器 */
         uint32_t RES4[56];
    __IO uint8_t  IP[240];   /*!< 0x400 中断优先级寄存器（每中断 1 字节） */
         uint32_t RES5[644];
    __O  uint32_t STIR;      /*!< 0xF00 软件触发中断寄存器 */
} NVIC_Type;

/** @brief 系统滴答定时器 SysTick */
typedef struct {
    __IO uint32_t CTRL;   /*!< 0x10 控制与状态寄存器 */
    __IO uint32_t LOAD;   /*!< 0x14 重装载值寄存器 */
    __IO uint32_t VAL;    /*!< 0x18 当前值寄存器 */
    __I  uint32_t CALIB;  /*!< 0x1C 校准值寄存器 */
} SysTick_Type;

#define SCS_BASE        (0xE000E000UL)
#define SysTick_BASE    (SCS_BASE + 0x0010UL)
#define NVIC_BASE       (SCS_BASE + 0x0100UL)
#define SCB_BASE        (SCS_BASE + 0x0D00UL)

#define SCB             ((SCB_Type *)SCB_BASE)
#define NVIC            ((NVIC_Type *)NVIC_BASE)
#define SysTick         ((SysTick_Type *)SysTick_BASE)

/* SysTick CTRL 位定义 */
#define SysTick_CTRL_ENABLE_Pos     0U
#define SysTick_CTRL_ENABLE_Msk     (1UL << SysTick_CTRL_ENABLE_Pos)
#define SysTick_CTRL_TICKINT_Pos    1U
#define SysTick_CTRL_TICKINT_Msk    (1UL << SysTick_CTRL_TICKINT_Pos)
#define SysTick_CTRL_CLKSOURCE_Pos  2U
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << SysTick_CTRL_CLKSOURCE_Pos)
#define SysTick_CTRL_COUNTFLAG_Pos  16U
#define SysTick_CTRL_COUNTFLAG_Msk  (1UL << SysTick_CTRL_COUNTFLAG_Pos)

/* SCB->AIRCR 优先级分组 */
#define SCB_AIRCR_VECTKEY_Pos   16U
#define SCB_AIRCR_VECTKEY_Msk   (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)
#define SCB_AIRCR_VECTKEY       (0x5FAUL << SCB_AIRCR_VECTKEY_Pos)
#define SCB_AIRCR_PRIGROUP_Pos  8U
#define SCB_AIRCR_PRIGROUP_Msk  (7UL << SCB_AIRCR_PRIGROUP_Pos)

/* ------------------------------------------------------------ 中断号定义 */
typedef enum {
    /* Cortex-M3 系统异常 */
    NonMaskableInt_IRQn   = -14,
    MemoryManagement_IRQn = -12,
    BusFault_IRQn         = -11,
    UsageFault_IRQn       = -10,
    SVCall_IRQn           = -5,
    DebugMonitor_IRQn     = -4,
    PendSV_IRQn           = -2,
    SysTick_IRQn          = -1,
    /* STM32F103 外部中断 */
    WWDG_IRQn             = 0,
    PVD_IRQn              = 1,
    TAMPER_IRQn           = 2,
    RTC_IRQn              = 3,
    FLASH_IRQn            = 4,
    RCC_IRQn              = 5,
    EXTI0_IRQn            = 6,
    EXTI1_IRQn            = 7,
    EXTI2_IRQn            = 8,
    EXTI3_IRQn            = 9,
    EXTI4_IRQn            = 10,
    DMA1_Channel1_IRQn    = 11,
    DMA1_Channel2_IRQn    = 12,
    DMA1_Channel3_IRQn    = 13,
    DMA1_Channel4_IRQn    = 14,
    DMA1_Channel5_IRQn    = 15,
    DMA1_Channel6_IRQn    = 16,
    DMA1_Channel7_IRQn    = 17,
    ADC1_2_IRQn           = 18,
    USB_HP_IRQn           = 19,
    USB_LP_IRQn           = 20,
    CAN_RX1_IRQn          = 21,
    CAN_SCE_IRQn          = 22,
    EXTI9_5_IRQn          = 23,
    TIM1_BRK_IRQn         = 24,
    TIM1_UP_IRQn          = 25,
    TIM1_TRG_COM_IRQn     = 26,
    TIM1_CC_IRQn          = 27,
    TIM2_IRQn             = 28,
    TIM3_IRQn             = 29,
    TIM4_IRQn             = 30,
    I2C1_EV_IRQn          = 31,
    I2C1_ER_IRQn          = 32,
    I2C2_EV_IRQn          = 33,
    I2C2_ER_IRQn          = 34,
    SPI1_IRQn             = 35,
    SPI2_IRQn             = 36,
    USART1_IRQn           = 37,
    USART2_IRQn           = 38,
    USART3_IRQn           = 39,
    EXTI15_10_IRQn        = 40,
    RTCAlarm_IRQn         = 41,
    USBWakeUp_IRQn        = 42
} IRQn_Type;

/* -------------------------------------------------------- 内核访问内联函数 */

/** 使能全局中断 */
__attribute__((always_inline)) static inline void __enable_irq(void)
{
    __asm volatile ("cpsie i" : : : "memory");
}

/** 关闭全局中断 */
__attribute__((always_inline)) static inline void __disable_irq(void)
{
    __asm volatile ("cpsid i" : : : "memory");
}

/** 数据同步屏障 */
__attribute__((always_inline)) static inline void __DSB(void)
{
    __asm volatile ("dsb 0xF" ::: "memory");
}

/**
 * @brief  设置中断优先级
 * @param  IRQn      中断号（负数表示系统异常）
 * @param  priority  4 位抢占优先级（本项目使用 4 位抢占、0 位子优先级）
 */
__attribute__((always_inline)) static inline void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
    if (IRQn < 0) {
        SCB->SHPR[((uint32_t)IRQn & 0xFUL) - 4UL] = (uint8_t)((priority << 4) & 0xFUL);
    } else {
        NVIC->IP[(uint32_t)IRQn] = (uint8_t)((priority << 4) & 0xFUL);
    }
}

/** 使能指定中断 */
__attribute__((always_inline)) static inline void NVIC_EnableIRQ(IRQn_Type IRQn)
{
    NVIC->ISER[((uint32_t)IRQn) >> 5UL] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
}

/** 禁用指定中断 */
__attribute__((always_inline)) static inline void NVIC_DisableIRQ(IRQn_Type IRQn)
{
    NVIC->ICER[((uint32_t)IRQn) >> 5UL] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
}

/** 设置优先级分组：4 位抢占优先级 / 0 位响应优先级 */
__attribute__((always_inline)) static inline void NVIC_SetPriorityGrouping(uint32_t group)
{
    uint32_t reg = SCB->AIRCR;
    reg &= ~((uint32_t)(SCB_AIRCR_VECTKEY_Msk | SCB_AIRCR_PRIGROUP_Msk));
    reg |= (SCB_AIRCR_VECTKEY | ((group << SCB_AIRCR_PRIGROUP_Pos) & SCB_AIRCR_PRIGROUP_Msk));
    SCB->AIRCR = reg;
    __DSB();
}

#endif /* CORE_CM3_H */
