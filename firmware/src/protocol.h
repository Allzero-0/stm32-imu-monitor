/**
 * @file    protocol.h
 * @brief   下位机 <-> 上位机 串口通信帧协议
 *
 * 帧格式（小端序，多字节字段低字节在前）：
 *  ┌──────┬──────┬──────┬──────┬────────┬──────────┬─────────┬──────┐
 *  │ 0xAA │ 0x55 │ TYPE │ LEN  │ SEQ(2) │ TICK(4)  │ PAYLOAD │ CRC8 │
 *  └──────┴──────┴──────┴──────┴────────┴──────────┴─────────┴──────┘
 *    帧头1   帧头2   类型   长度   帧序号   时间戳ms   数据     校验
 *
 * 为什么不用直接 printf 打字符串？
 *   二进制帧的带宽占用只有 ASCII 方案的 1/3 左右，解析开销小，且带 CRC
 *   能发现串口误码；ASCII 方案在 100Hz 上传时会把串口带宽吃光。
 *
 * CRC8：多项式 0x07（x^8 + x^2 + x + 1），初值 0x00。
 *        覆盖 TYPE 到 PAYLOAD 结束，长度 = 8 + LEN 字节（不含 2 字节帧头）。
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* ------------------------------------------------------------- 帧常量 */
#define FRAME_HEAD0          0xAAU
#define FRAME_HEAD1          0x55U

#define FRAME_TYPE_DATA      0x01U   /* IMU + ADC 数据帧 */
#define FRAME_TYPE_STATUS    0x02U   /* 运行状态帧 */
#define FRAME_TYPE_LOG       0x03U   /* 文本日志帧 */

#define FRAME_HDR_SIZE       10U     /* 帧头(2) + TYPE(1) + LEN(1) + SEQ(2) + TICK(4) */
#define FRAME_CRC_SIZE       1U
#define FRAME_MAX_PAYLOAD    64U
#define FRAME_MAX_SIZE       (FRAME_HDR_SIZE + FRAME_MAX_PAYLOAD + FRAME_CRC_SIZE)

/** 数据帧 payload 长度：6+6+2+6+6+2 = 28 字节 */
#define FRAME_DATA_PAYLOAD   28U

/* ------------------------------------------------------------- 数据帧 */
/** @brief 数据帧 payload 布局（与上位机 Python 端严格对应） */
typedef struct {
    int16_t accel[3];        /* 加速度原始值（已扣零偏），LSB @±8g */
    int16_t gyro[3];         /* 陀螺仪原始值（已扣零偏），LSB @±2000°/s */
    int16_t mpu_temp_x10;    /* MPU6050 片内温度 ×10 (0.1°C) */
    int16_t angle_x100[3];   /* 姿态角 roll/pitch/yaw ×100 (度) */
    uint16_t adc[3];         /* ADC 原始值：外部0 / 外部1 / 内部温度 */
    int16_t chip_temp_x10;   /* STM32 内部温度 ×10 (0.1°C) */
} frame_data_payload_t;

/* ------------------------------------------------------------- 命令字 */
#define CMD_NONE        0U
#define CMD_CALIBRATE   'C'   /* 重新零偏校准 */
#define CMD_STATUS      'V'   /* 立即上报状态帧 */
#define CMD_TOGGLE      'S'   /* 暂停 / 继续上传 */
#define CMD_RESET       'R'   /* 复位统计计数 */

/* --------------------------------------------------------------- API */
uint8_t CRC8_Calc(const uint8_t *data, uint16_t len);

uint16_t Protocol_Pack(uint8_t type, const uint8_t *payload, uint8_t len,
                       uint16_t seq, uint32_t tick, uint8_t *out);

uint16_t Protocol_PackData(const frame_data_payload_t *p,
                           uint16_t seq, uint32_t tick, uint8_t *out);

uint8_t Protocol_DecodeCommand(uint8_t c);

#endif /* PROTOCOL_H */
