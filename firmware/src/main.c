/**
 * @file    main.c
 * @brief   程序入口
 *
 * 注意：SystemInit() 已由启动文件的 Reset_Handler 调用，
 *       此处只需配置 SysTick 时基并进入应用循环。
 */
#include "system.h"
#include "app.h"

int main(void)
{
    /* 1ms 系统节拍，供 millis()/delay_ms() 使用 */
    (void)SysTick_Config_1ms();

    /* 进入主循环（不返回） */
    APP_Run();

    return 0;
}
