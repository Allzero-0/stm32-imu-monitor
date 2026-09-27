/**
 * @file    app.h
 * @brief   应用层：采样调度、数据上传、命令处理
 */
#ifndef APP_H
#define APP_H

#include <stdint.h>

/** 采样频率（Hz），与 TIM2 节拍一致 */
#define APP_SAMPLE_HZ       200U

/** 每 N 个采样周期上传一帧（200/2 = 100Hz 上传） */
#define APP_UPLOAD_DIV      2U

/**
 * @brief  进入应用主循环（不返回）
 */
void APP_Run(void);

#endif /* APP_H */
