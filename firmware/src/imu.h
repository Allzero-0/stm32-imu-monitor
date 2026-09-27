/**
 * @file    imu.h
 * @brief   姿态解算：加速度/陀螺仪互补滤波
 */
#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include "mpu6050.h"

/** @brief 姿态角（单位：度） */
typedef struct {
    float roll;    /* 横滚，绕 X 轴 */
    float pitch;   /* 俯仰，绕 Y 轴 */
    float yaw;     /* 航向，绕 Z 轴（无磁力计，会缓慢漂移） */
} imu_angle_t;

/**
 * @brief  初始化姿态解算（清角度）
 */
void IMU_Init(void);

/**
 * @brief  用一次采样更新姿态角
 * @param  raw  已扣零偏的原始采样值
 * @param  dt   采样间隔（秒）
 * @param  out  输出姿态角
 */
void IMU_Update(const mpu6050_raw_t *raw, float dt, imu_angle_t *out);

/**
 * @brief  清零姿态角（保留内部状态）
 */
void IMU_Reset(void);

/**
 * @brief  由加速度计算静态倾角（不受陀螺零偏影响，但动态抖动大）
 */
void IMU_AccelAngles(const mpu6050_raw_t *raw, imu_angle_t *out);

#endif /* IMU_H */
