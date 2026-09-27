/**
 * @file    soft_i2c.h
 * @brief   GPIO 模拟 I2C 主机驱动
 *
 * 为什么不用硬件 I2C1？
 *   STM32F1 的硬件 I2C 在总线被异常拉低后容易进入 BUSY 死锁，需要复位外设
 *   甚至断电才能恢复，工程中口碑较差。这里用 GPIO 模拟，并额外实现
 *   "总线恢复（9 个时钟脉冲 + STOP）"，任何异常状态下都能自愈。
 *   代价：CPU 占用略高，但本项目 200Hz 采样下完全够用。
 *
 * 引脚：PB6 = SCL，PB7 = SDA（开漏输出 + 外部上拉）
 */
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdint.h>

void     SoftI2C_Init(void);
void     SoftI2C_Start(void);
void     SoftI2C_Stop(void);
uint8_t  SoftI2C_WriteByte(uint8_t byte);   /* 返回 0 表示收到 ACK */
uint8_t  SoftI2C_ReadByte(uint8_t ack);
void     SoftI2C_BusRecovery(void);

/* 面向设备的便捷接口 */
uint8_t  SoftI2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t data);
uint8_t  SoftI2C_ReadRegs(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint16_t len);
uint8_t  SoftI2C_Probe(uint8_t dev_addr);

#endif /* SOFT_I2C_H */
