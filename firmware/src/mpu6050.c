/**
 * @file    mpu6050.c
 * @brief   MPU6050 驱动实现
 */
#include "mpu6050.h"
#include "soft_i2c.h"
#include "system.h"

/**
 * @brief  校验 WHO_AM_I，确认芯片在线
 * @retval 0 正常（读到 0x68）；非 0 通信失败
 */
uint8_t MPU6050_Check(void)
{
    uint8_t id = 0U;

    if (SoftI2C_ReadRegs(MPU6050_ADDR, MPU6050_REG_WHO_AM_I, &id, 1U) != 0U) {
        return 1U;
    }
    return (id == 0x68U) ? 0U : 2U;
}

/**
 * @brief  初始化 MPU6050
 *
 * 流程：上电延时 -> 探测 -> 软件复位 -> 退出睡眠并选 PLL 时钟 ->
 *       配置低通滤波 -> 设置采样率 -> 设置量程
 *
 * @retval 0 成功；1 I2C 无应答；2 ID 不对
 */
uint8_t MPU6050_Init(void)
{
    delay_ms(100U);   /* 等待芯片上电稳定 */

    if (SoftI2C_Probe(MPU6050_ADDR) != 0U) {
        SoftI2C_BusRecovery();                 /* 先尝试恢复总线再探一次 */
        if (SoftI2C_Probe(MPU6050_ADDR) != 0U) {
            return 1U;
        }
    }

    if (MPU6050_Check() != 0U) {
        return 2U;
    }

    /* 复位：PWR_MGMT_1 bit7 = DEVICE_RESET */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1, 0x80U);
    delay_ms(100U);

    /* 唤醒：清除 SLEEP，时钟选 PLL with X-axis gyro（最稳定的时钟源） */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1, 0x01U);
    delay_ms(10U);

    /* 关闭所有轴的待机 */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_PWR_MGMT_2, 0x00U);

    /* DLPF_CFG = 3：加速度带宽 44Hz、陀螺仪带宽 42Hz，滤掉高频机械抖动 */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_CONFIG, 0x03U);

    /* 采样分频：陀螺输出 1kHz（DLPF 使能时），分频 5 -> 200Hz */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_SMPLRT_DIV, 0x04U);

    /* 陀螺仪 ±2000°/s（FS_SEL=3） */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_GYRO_CFG, 0x18U);

    /* 加速度 ±8g（AFS_SEL=2） */
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_ACCEL_CFG, 0x10U);

    delay_ms(10U);
    return 0U;
}

/**
 * @brief  设置采样率分频：SampleRate = 1kHz / (1 + div)
 */
void MPU6050_SetSampleRateDiv(uint8_t div)
{
    (void)SoftI2C_WriteReg(MPU6050_ADDR, MPU6050_REG_SMPLRT_DIV, div);
}

/**
 * @brief  一次性读取 14 字节：加速度(6) + 温度(2) + 陀螺仪(6)
 *
 * 使用连续突发读（burst read）而不是分次单寄存器读，保证同一时刻的数据
 * 快照，避免轴间数据不同步导致姿态解算出现毛刺。
 *
 * @retval 0 成功
 */
uint8_t MPU6050_ReadRaw(mpu6050_raw_t *out)
{
    uint8_t buf[14];

    if (SoftI2C_ReadRegs(MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H, buf, 14U) != 0U) {
        return 1U;
    }

    out->ax = (int16_t)((buf[0] << 8) | buf[1]);
    out->ay = (int16_t)((buf[2] << 8) | buf[3]);
    out->az = (int16_t)((buf[4] << 8) | buf[5]);
    out->temp_raw = (int16_t)((buf[6] << 8) | buf[7]);
    out->gx = (int16_t)((buf[8] << 8) | buf[9]);
    out->gy = (int16_t)((buf[10] << 8) | buf[11]);
    out->gz = (int16_t)((buf[12] << 8) | buf[13]);
    return 0U;
}

/**
 * @brief  零偏校准：静止放置，连续采样求平均
 *
 * 陀螺仪的零偏会随温度和使用时间漂移，是姿态解算最主要的误差源。
 * 这里在启动时做一次静态校准；加速度取 X/Y 的均值作为零偏，
 * Z 轴则扣除 1g（4096 LSB @±8g）后作为零偏。
 *
 * @param  cal    校准结果输出
 * @param  samples 采样次数（建议 >= 128）
 * @retval 0 成功
 */
uint8_t MPU6050_Calibrate(mpu6050_cal_t *cal, uint16_t samples)
{
    uint32_t sum_ax = 0U, sum_ay = 0U, sum_az = 0U;
    uint32_t sum_gx = 0U, sum_gy = 0U, sum_gz = 0U;
    uint16_t i, ok = 0U;
    mpu6050_raw_t raw;

    if (samples == 0U) {
        return 1U;
    }

    for (i = 0U; i < samples; i++) {
        if (MPU6050_ReadRaw(&raw) == 0U) {
            /* 用无符号累加再取平均，避免溢出；读数可能为负需转 int32 */
            sum_ax += (uint32_t)(int32_t)raw.ax;
            sum_ay += (uint32_t)(int32_t)raw.ay;
            sum_az += (uint32_t)(int32_t)raw.az;
            sum_gx += (uint32_t)(int32_t)raw.gx;
            sum_gy += (uint32_t)(int32_t)raw.gy;
            sum_gz += (uint32_t)(int32_t)raw.gz;
            ok++;
        }
        delay_ms(2U);
    }

    if (ok == 0U) {
        return 2U;
    }

    cal->ax = (int16_t)((int32_t)(sum_ax / ok));
    cal->ay = (int16_t)((int32_t)(sum_ay / ok));
    cal->az = (int16_t)((int32_t)(sum_az / ok) - 4096);   /* 扣除重力 1g */
    cal->gx = (int16_t)((int32_t)(sum_gx / ok));
    cal->gy = (int16_t)((int32_t)(sum_gy / ok));
    cal->gz = (int16_t)((int32_t)(sum_gz / ok));
    cal->valid = 1U;
    return 0U;
}

/**
 * @brief  对原始值扣除零偏（原地修改）
 */
void MPU6050_ApplyCal(mpu6050_raw_t *raw, const mpu6050_cal_t *cal)
{
    if ((cal == 0) || (cal->valid == 0U)) {
        return;
    }
    raw->ax -= cal->ax;
    raw->ay -= cal->ay;
    raw->az -= cal->az;
    raw->gx -= cal->gx;
    raw->gy -= cal->gy;
    raw->gz -= cal->gz;
}

/**
 * @brief  片内温度换算：T = raw/340 + 36.53 （摄氏度）
 */
float MPU6050_TemperatureCelsius(int16_t temp_raw)
{
    return ((float)temp_raw / 340.0f) + 36.53f;
}
