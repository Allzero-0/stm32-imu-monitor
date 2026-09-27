/**
 * @file    imu.c
 * @brief   姿态解算实现
 *
 * 原理说明（面试常问）：
 *  - 加速度计：静态时测的是重力方向，可算出长期准确的 roll/pitch，
 *    但运动时受线性加速度干扰，噪声大、响应慢。
 *  - 陀螺仪：对角速度积分得角度，动态响应快、短期准，但零偏会随时间累积
 *    导致漂移（即使静止也在慢慢转）。
 *  - 互补滤波：angle = a*(上一刻角度 + 角速度*dt) + (1-a)*加速度算出的角度。
 *    用高通特性保留陀螺仪的动态性能，用低通特性吸收加速度计的低频真值。
 *    这里 a = 0.98（对应截止频率约 dt 相关，200Hz 下时间常数约 0.5s）。
 *  - yaw（航向）没有磁力计做绝对参考，只能纯积分，因此必然漂移，这是
 *    六轴方案的固有缺陷，不是 bug。
 */
#include "imu.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979f
#endif

#define RAD_TO_DEG  (180.0f / PI)

/** 互补滤波系数：越大越信任陀螺仪 */
#define ALPHA_GYRO  0.98f

static imu_angle_t s_angle = {0.0f, 0.0f, 0.0f};

void IMU_Init(void)
{
    s_angle.roll = 0.0f;
    s_angle.pitch = 0.0f;
    s_angle.yaw = 0.0f;
}

void IMU_Reset(void)
{
    IMU_Init();
}

/**
 * @brief  仅用加速度计算倾角
 */
void IMU_AccelAngles(const mpu6050_raw_t *raw, imu_angle_t *out)
{
    float ax = (float)raw->ax;
    float ay = (float)raw->ay;
    float az = (float)raw->az;

    out->roll  = atan2f(ay, az) * RAD_TO_DEG;
    out->pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
    out->yaw   = 0.0f;
}

/**
 * @brief  互补滤波更新
 */
void IMU_Update(const mpu6050_raw_t *raw, float dt, imu_angle_t *out)
{
    imu_angle_t acc_angle;
    float gx_dps, gy_dps, gz_dps;

    /* 1. 陀螺仪原始值换算为角速度（°/s） */
    gx_dps = (float)raw->gx / MPU6050_GYRO_SENS_2000;
    gy_dps = (float)raw->gy / MPU6050_GYRO_SENS_2000;
    gz_dps = (float)raw->gz / MPU6050_GYRO_SENS_2000;

    /* 2. 陀螺仪积分（预测） */
    s_angle.roll  += gx_dps * dt;
    s_angle.pitch += gy_dps * dt;
    s_angle.yaw   += gz_dps * dt;

    /* 3. 加速度计提供静态参考（校正） */
    IMU_AccelAngles(raw, &acc_angle);

    /* 4. 互补融合 */
    s_angle.roll  = ALPHA_GYRO * s_angle.roll  + (1.0f - ALPHA_GYRO) * acc_angle.roll;
    s_angle.pitch = ALPHA_GYRO * s_angle.pitch + (1.0f - ALPHA_GYRO) * acc_angle.pitch;
    /* yaw 无加速度参考，只能保持积分结果 */

    /* 5. yaw 限制在 -180 ~ +180，避免长时间跑飞后数值无意义 */
    if (s_angle.yaw > 180.0f) {
        s_angle.yaw -= 360.0f;
    } else if (s_angle.yaw < -180.0f) {
        s_angle.yaw += 360.0f;
    }

    out->roll  = s_angle.roll;
    out->pitch = s_angle.pitch;
    out->yaw   = s_angle.yaw;
}
