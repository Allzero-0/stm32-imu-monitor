/**
 * @file    mpu6050.h
 * @brief   MPU6050 六轴传感器驱动（加速度 + 陀螺仪 + 片内温度）
 *
 * 量程与灵敏度（与 MPU6050_Init 中的配置对应）：
 *   加速度：±8g  -> 4096 LSB/g
 *   陀螺仪：±2000°/s -> 16.4 LSB/(°/s)
 *   温度  ：T(°C) = raw / 340.0 + 36.53
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/** 7 位从机地址（AD0 接地时为 0x68，接 VCC 时为 0x69） */
#define MPU6050_ADDR        0x68U

/* 寄存器映射（常用部分） */
#define MPU6050_REG_SMPLRT_DIV   0x19U
#define MPU6050_REG_CONFIG       0x1AU
#define MPU6050_REG_GYRO_CFG     0x1BU
#define MPU6050_REG_ACCEL_CFG    0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_TEMP_OUT_H   0x41U
#define MPU6050_REG_PWR_MGMT_1   0x6BU
#define MPU6050_REG_PWR_MGMT_2   0x6CU
#define MPU6050_REG_WHO_AM_I     0x75U

/** 灵敏度常量 */
#define MPU6050_ACCEL_SENS_8G    4096.0f
#define MPU6050_GYRO_SENS_2000   16.4f

/** @brief 一次采样得到的原始值（含片内温度） */
typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    int16_t temp_raw;
} mpu6050_raw_t;

/** @brief 零偏校准数据（静止时测得的固定偏差） */
typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint8_t valid;
} mpu6050_cal_t;

uint8_t MPU6050_Init(void);
uint8_t MPU6050_Check(void);
uint8_t MPU6050_ReadRaw(mpu6050_raw_t *out);
uint8_t MPU6050_Calibrate(mpu6050_cal_t *cal, uint16_t samples);
void    MPU6050_ApplyCal(mpu6050_raw_t *raw, const mpu6050_cal_t *cal);
float   MPU6050_TemperatureCelsius(int16_t temp_raw);
void    MPU6050_SetSampleRateDiv(uint8_t div);

#endif /* MPU6050_H */
