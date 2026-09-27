/**
 * @file    usart.h
 * @brief   USART1 驱动：DMA 发送 + 中断接收环形缓冲
 *
 * 设计要点：
 *   - 发送走 DMA1_Channel4，CPU 只负责填缓冲和启动，不阻塞主循环
 *   - 发送为非阻塞：上帧未发完时直接丢帧并计数（流量控制），
 *     避免低速串口把高速采样循环拖垮
 *   - 接收为中断 + 环形缓冲区（1KB），供上位机下发命令使用
 */
#ifndef USART_H
#define USART_H

#include <stdint.h>

/** 接收环形缓冲区大小（字节） */
#define USART_RX_BUF_SIZE   256U

void     USART1_Init(uint32_t baud);
void     USART1_SendString(const char *str);
int      USART1_SendDMA(const uint8_t *data, uint16_t len);
uint8_t  USART1_TxBusy(void);
uint16_t USART1_GetDropped(void);

uint16_t USART1_RxAvailable(void);
int      USART1_RxGetByte(void);

#endif /* USART_H */
