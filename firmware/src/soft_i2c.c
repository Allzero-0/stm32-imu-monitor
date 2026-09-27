/**
 * @file    soft_i2c.c
 * @brief   GPIO 模拟 I2C 主机实现（PB6=SCL, PB7=SDA）
 */
#include "soft_i2c.h"
#include "stm32f103xb.h"
#include "system.h"

/* ------------------------------------------------------------- 引脚宏定义 */
#define I2C_SCL_PIN    6U
#define I2C_SDA_PIN    7U

/* 半周期延时（us）：2us -> 约 250kHz（标准模式 100kHz 上限内留余量） */
#define I2C_HALF_DELAY_US   2U

/* 时钟延展等待超时次数 */
#define I2C_TIMEOUT         5000U

/* SCL/SDA 置高：开漏模式下写 ODR=1 即释放总线，由上拉电阻拉高 */
#define SCL_H()  (GPIOB->BSRR = (1UL << I2C_SCL_PIN))
#define SCL_L()  (GPIOB->BRR  = (1UL << I2C_SCL_PIN))
#define SDA_H()  (GPIOB->BSRR = (1UL << I2C_SDA_PIN))
#define SDA_L()  (GPIOB->BRR  = (1UL << I2C_SDA_PIN))
#define SDA_READ()  ((GPIOB->IDR >> I2C_SDA_PIN) & 0x1UL)

/**
 * @brief  SCL 拉高并等待其真正变为高电平（兼容从机时钟延展）
 * @retval 0 正常；1 超时（总线异常）
 */
static uint8_t SCL_High_WithTimeout(void)
{
    uint32_t timeout = I2C_TIMEOUT;

    SCL_H();
    while (((GPIOB->IDR >> I2C_SCL_PIN) & 0x1UL) == 0UL) {
        if (timeout-- == 0U) {
            return 1U;   /* 从机死死拉住 SCL */
        }
    }
    delay_us(I2C_HALF_DELAY_US);
    return 0U;
}

/**
 * @brief  初始化模拟 I2C 引脚：开漏输出 50MHz，总线置空闲（均为高）
 */
void SoftI2C_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    /* PB6/PB7 位于 CRL（pin0~7），每引脚 4 bit */
    GPIOB->CRL &= ~((0xFUL << (4U * I2C_SCL_PIN)) | (0xFUL << (4U * I2C_SDA_PIN)));
    /* CNF=01(通用开漏) MODE=11(50MHz) -> 0x7 */
    GPIOB->CRL |= ((0x7UL << (4U * I2C_SCL_PIN)) | (0x7UL << (4U * I2C_SDA_PIN)));

    /* 释放总线：两根线都拉高 */
    GPIOB->BSRR = (1UL << I2C_SCL_PIN) | (1UL << I2C_SDA_PIN);
    delay_us(20U);
}

/**
 * @brief  产生起始条件：SCL 高电平期间 SDA 由高变低
 */
void SoftI2C_Start(void)
{
    SDA_H();
    SCL_H();
    delay_us(I2C_HALF_DELAY_US);
    SDA_L();
    delay_us(I2C_HALF_DELAY_US);
    SCL_L();
    delay_us(I2C_HALF_DELAY_US);
}

/**
 * @brief  产生停止条件：SCL 高电平期间 SDA 由低变高
 */
void SoftI2C_Stop(void)
{
    SDA_L();
    delay_us(I2C_HALF_DELAY_US);
    SCL_H();
    delay_us(I2C_HALF_DELAY_US);
    SDA_H();
    delay_us(I2C_HALF_DELAY_US);
}

/**
 * @brief  写一个字节并读取从机应答
 * @retval 0 收到 ACK；1 未收到 ACK（NACK）
 */
uint8_t SoftI2C_WriteByte(uint8_t byte)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0U; i < 8U; i++) {
        if ((byte & 0x80U) != 0U) {
            SDA_H();
        } else {
            SDA_L();
        }
        byte <<= 1;
        delay_us(I2C_HALF_DELAY_US);
        (void)SCL_High_WithTimeout();          /* SCL 上升沿锁存数据 */
        SCL_L();
        delay_us(I2C_HALF_DELAY_US);
    }

    /* 第 9 个时钟：释放 SDA 读取 ACK */
    SDA_H();
    delay_us(I2C_HALF_DELAY_US);
    (void)SCL_High_WithTimeout();
    ack = (SDA_READ() != 0U) ? 1U : 0U;        /* 从机拉低即为 ACK */
    SCL_L();
    delay_us(I2C_HALF_DELAY_US);
    return ack;
}

/**
 * @brief  读一个字节
 * @param  ack 主机是否应答（1 = 继续读，0 = 读完后 NACK）
 */
uint8_t SoftI2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0U;

    SDA_H();                                   /* 释放 SDA，交给从机驱动 */
    for (i = 0U; i < 8U; i++) {
        delay_us(I2C_HALF_DELAY_US);
        (void)SCL_High_WithTimeout();
        byte <<= 1;
        if (SDA_READ() != 0U) {
            byte |= 0x01U;
        }
        SCL_L();
        delay_us(I2C_HALF_DELAY_US);
    }

    /* 主机应答位 */
    if (ack != 0U) {
        SDA_L();
    } else {
        SDA_H();
    }
    delay_us(I2C_HALF_DELAY_US);
    (void)SCL_High_WithTimeout();
    SCL_L();
    SDA_H();
    delay_us(I2C_HALF_DELAY_US);
    return byte;
}

/**
 * @brief  总线恢复：发送 9 个时钟脉冲，再给一个 STOP
 *
 * 用于从机把 SDA 卡在低电平导致总线死锁的情况（例如 MPU6050 读出半个字节时
 * 被复位）。这是硬件 I2C 外设很难处理、软件 I2C 很容易处理的典型场景。
 */
void SoftI2C_BusRecovery(void)
{
    uint8_t i;

    SDA_H();
    for (i = 0U; i < 9U; i++) {
        SCL_L();
        delay_us(I2C_HALF_DELAY_US);
        SCL_H();
        delay_us(I2C_HALF_DELAY_US);
    }
    /* 在 SCL 高时产生 STOP：SDA 由低变高 */
    SDA_L();
    delay_us(I2C_HALF_DELAY_US);
    SCL_H();
    delay_us(I2C_HALF_DELAY_US);
    SDA_H();
    delay_us(I2C_HALF_DELAY_US);
}

/**
 * @brief  写单个寄存器
 * @retval 0 成功；非 0 失败
 */
uint8_t SoftI2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t data)
{
    uint8_t ret;

    SoftI2C_Start();
    ret = SoftI2C_WriteByte((uint8_t)(dev_addr << 1U));   /* 写方向 */
    if (ret == 0U) {
        ret = SoftI2C_WriteByte(reg);
    }
    if (ret == 0U) {
        ret = SoftI2C_WriteByte(data);
    }
    SoftI2C_Stop();
    return ret;
}

/**
 * @brief  从指定寄存器开始连续读取多个字节
 * @retval 0 成功；非 0 失败
 */
uint8_t SoftI2C_ReadRegs(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t ret;
    uint16_t i;

    /* 1. 写寄存器地址（伪写） */
    SoftI2C_Start();
    ret = SoftI2C_WriteByte((uint8_t)(dev_addr << 1U));
    if (ret == 0U) {
        ret = SoftI2C_WriteByte(reg);
    }
    if (ret != 0U) {
        SoftI2C_Stop();
        return 1U;
    }

    /* 2. 重复起始 + 读方向 */
    SoftI2C_Start();
    ret = SoftI2C_WriteByte((uint8_t)((dev_addr << 1U) | 0x01U));
    if (ret != 0U) {
        SoftI2C_Stop();
        return 2U;
    }

    for (i = 0U; i < len; i++) {
        buf[i] = SoftI2C_ReadByte((uint8_t)((i + 1U) < len ? 1U : 0U));
    }
    SoftI2C_Stop();
    return 0U;
}

/**
 * @brief  探测设备是否存在（发送地址看是否有 ACK）
 * @retval 0 存在；非 0 无应答
 */
uint8_t SoftI2C_Probe(uint8_t dev_addr)
{
    uint8_t ret;

    SoftI2C_Start();
    ret = SoftI2C_WriteByte((uint8_t)(dev_addr << 1U));
    SoftI2C_Stop();
    return ret;
}
