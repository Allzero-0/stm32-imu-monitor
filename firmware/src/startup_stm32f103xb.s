/**
 * @file    startup_stm32f103xb.s
 * @brief   STM32F103xB 启动文件（GNU as 语法）
 *
 * 职责：
 *   1. 建立中断向量表
 *   2. 初始化 .data 段（从 FLASH 拷贝到 RAM）、清零 .bss 段
 *   3. 跳转到 SystemInit 配置时钟，再进入 main
 *
 * 注意：本工程未使用标准库初始化流程（--nostartfiles 之外的默认流程被保留
 *      但 main 返回后进入死循环），因此不依赖 newlib 的 _start。
 */
    .syntax unified
    .cpu cortex-m3
    .thumb

/* 栈顶（由链接脚本提供） */
    .word   _estack

    .section .isr_vector, "a"
    .global g_pfnVectors
    .type   g_pfnVectors, %object
    .size   g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word   _estack                 /* 0  - 栈顶 */
    .word   Reset_Handler           /* 1  - 复位 */
    .word   NMI_Handler             /* 2  */
    .word   HardFault_Handler       /* 3  */
    .word   MemManage_Handler       /* 4  */
    .word   BusFault_Handler        /* 5  */
    .word   UsageFault_Handler      /* 6  */
    .word   0                       /* 7  */
    .word   0                       /* 8  */
    .word   0                       /* 9  */
    .word   0                       /* 10 */
    .word   SVC_Handler             /* 11 */
    .word   DebugMon_Handler        /* 12 */
    .word   0                       /* 13 */
    .word   PendSV_Handler          /* 14 */
    .word   SysTick_Handler         /* 15 */
    /* 外部中断 */
    .word   WWDG_IRQHandler         /* 0  */
    .word   PVD_IRQHandler          /* 1  */
    .word   TAMPER_IRQHandler       /* 2  */
    .word   RTC_IRQHandler          /* 3  */
    .word   FLASH_IRQHandler        /* 4  */
    .word   RCC_IRQHandler          /* 5  */
    .word   EXTI0_IRQHandler        /* 6  */
    .word   EXTI1_IRQHandler        /* 7  */
    .word   EXTI2_IRQHandler        /* 8  */
    .word   EXTI3_IRQHandler        /* 9  */
    .word   EXTI4_IRQHandler        /* 10 */
    .word   DMA1_Channel1_IRQHandler /* 11 */
    .word   DMA1_Channel2_IRQHandler /* 12 */
    .word   DMA1_Channel3_IRQHandler /* 13 */
    .word   DMA1_Channel4_IRQHandler /* 14 */
    .word   DMA1_Channel5_IRQHandler /* 15 */
    .word   DMA1_Channel6_IRQHandler /* 16 */
    .word   DMA1_Channel7_IRQHandler /* 17 */
    .word   ADC1_2_IRQHandler       /* 18 */
    .word   USB_HP_CAN_TX_IRQHandler /* 19 */
    .word   USB_LP_CAN_RX0_IRQHandler /* 20 */
    .word   CAN_RX1_IRQHandler      /* 21 */
    .word   CAN_SCE_IRQHandler      /* 22 */
    .word   EXTI9_5_IRQHandler      /* 23 */
    .word   TIM1_BRK_IRQHandler     /* 24 */
    .word   TIM1_UP_IRQHandler      /* 25 */
    .word   TIM1_TRG_COM_IRQHandler /* 26 */
    .word   TIM1_CC_IRQHandler      /* 27 */
    .word   TIM2_IRQHandler         /* 28 */
    .word   TIM3_IRQHandler         /* 29 */
    .word   TIM4_IRQHandler         /* 30 */
    .word   I2C1_EV_IRQHandler      /* 31 */
    .word   I2C1_ER_IRQHandler      /* 32 */
    .word   I2C2_EV_IRQHandler      /* 33 */
    .word   I2C2_ER_IRQHandler      /* 34 */
    .word   SPI1_IRQHandler         /* 35 */
    .word   SPI2_IRQHandler         /* 36 */
    .word   USART1_IRQHandler       /* 37 */
    .word   USART2_IRQHandler       /* 38 */
    .word   USART3_IRQHandler       /* 39 */
    .word   EXTI15_10_IRQHandler    /* 40 */
    .word   RTCAlarm_IRQHandler     /* 41 */
    .word   USBWakeUp_IRQHandler    /* 42 */

/* ------------------------------------------------------------ 复位处理 */
    .section .text.Reset_Handler
    .weak   Reset_Handler
    .type   Reset_Handler, %function
Reset_Handler:
    /* 拷贝 .data 段：FLASH(_sidata) -> RAM(_sdata .. _edata) */
    ldr   r0, =_sdata
    ldr   r1, =_edata
    ldr   r2, =_sidata
    movs  r3, #0
    b     LoopCopyDataInit

CopyDataInit:
    ldr   r4, [r2, r3]
    str   r4, [r0, r3]
    adds  r3, r3, #4

LoopCopyDataInit:
    adds  r4, r0, r3
    cmp   r4, r1
    bcc   CopyDataInit

    /* 清零 .bss 段 */
    ldr   r2, =_sbss
    ldr   r4, =_ebss
    movs  r3, #0
    b     LoopFillZerobss

FillZerobss:
    str   r3, [r2]
    adds  r2, r2, #4

LoopFillZerobss:
    cmp   r2, r4
    bcc   FillZerobss

    /* 配置时钟，进入 main */
    bl    SystemInit
    bl    main

LoopForever:
    b     LoopForever
    .size Reset_Handler, .-Reset_Handler

/* --------------------------------------------------- 默认异常处理函数 */
    .section .text.Default_Handler, "ax", %progbits
Default_Handler:
Infinite_Loop:
    b     Infinite_Loop
    .size Default_Handler, .-Default_Handler

/* 所有未实现的中断统一弱定义到 Default_Handler */
    .macro def_irq_handler handler
    .weak \handler
    .set  \handler, Default_Handler
    .endm

    def_irq_handler NMI_Handler
    def_irq_handler HardFault_Handler
    def_irq_handler MemManage_Handler
    def_irq_handler BusFault_Handler
    def_irq_handler UsageFault_Handler
    def_irq_handler SVC_Handler
    def_irq_handler DebugMon_Handler
    def_irq_handler PendSV_Handler
    def_irq_handler SysTick_Handler
    def_irq_handler WWDG_IRQHandler
    def_irq_handler PVD_IRQHandler
    def_irq_handler TAMPER_IRQHandler
    def_irq_handler RTC_IRQHandler
    def_irq_handler FLASH_IRQHandler
    def_irq_handler RCC_IRQHandler
    def_irq_handler EXTI0_IRQHandler
    def_irq_handler EXTI1_IRQHandler
    def_irq_handler EXTI2_IRQHandler
    def_irq_handler EXTI3_IRQHandler
    def_irq_handler EXTI4_IRQHandler
    def_irq_handler DMA1_Channel1_IRQHandler
    def_irq_handler DMA1_Channel2_IRQHandler
    def_irq_handler DMA1_Channel3_IRQHandler
    def_irq_handler DMA1_Channel4_IRQHandler
    def_irq_handler DMA1_Channel5_IRQHandler
    def_irq_handler DMA1_Channel6_IRQHandler
    def_irq_handler DMA1_Channel7_IRQHandler
    def_irq_handler ADC1_2_IRQHandler
    def_irq_handler USB_HP_CAN_TX_IRQHandler
    def_irq_handler USB_LP_CAN_RX0_IRQHandler
    def_irq_handler CAN_RX1_IRQHandler
    def_irq_handler CAN_SCE_IRQHandler
    def_irq_handler EXTI9_5_IRQHandler
    def_irq_handler TIM1_BRK_IRQHandler
    def_irq_handler TIM1_UP_IRQHandler
    def_irq_handler TIM1_TRG_COM_IRQHandler
    def_irq_handler TIM1_CC_IRQHandler
    def_irq_handler TIM2_IRQHandler
    def_irq_handler TIM3_IRQHandler
    def_irq_handler TIM4_IRQHandler
    def_irq_handler I2C1_EV_IRQHandler
    def_irq_handler I2C1_ER_IRQHandler
    def_irq_handler I2C2_EV_IRQHandler
    def_irq_handler I2C2_ER_IRQHandler
    def_irq_handler SPI1_IRQHandler
    def_irq_handler SPI2_IRQHandler
    def_irq_handler USART1_IRQHandler
    def_irq_handler USART2_IRQHandler
    def_irq_handler USART3_IRQHandler
    def_irq_handler EXTI15_10_IRQHandler
    def_irq_handler RTCAlarm_IRQHandler
    def_irq_handler USBWakeUp_IRQHandler

    .end
