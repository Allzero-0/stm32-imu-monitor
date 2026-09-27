/**
 * @file    protocol.c
 * @brief   帧协议实现：打包 + CRC8 校验 + 命令解析
 */
#include "protocol.h"

/**
 * @brief  CRC8 计算（多项式 0x07，初值 0x00，无反射、无输出异或）
 *
 * 用查表太占 Flash，这里用移位实现，64 字节以内的数据量 CPU 开销可忽略。
 */
uint8_t CRC8_Calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00U;
    uint16_t i;
    uint8_t j;

    for (i = 0U; i < len; i++) {
        crc ^= data[i];
        for (j = 0U; j < 8U; j++) {
            if ((crc & 0x80U) != 0U) {
                crc = (uint8_t)((crc << 1) ^ 0x07U);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/**
 * @brief  打包一帧
 *
 * @param  type    帧类型
 * @param  payload 数据区
 * @param  len     数据区长度
 * @param  seq     帧序号
 * @param  tick    时间戳（毫秒）
 * @param  out     输出缓冲区（至少 FRAME_MAX_SIZE 字节）
 * @return 帧总长度（字节）
 */
uint16_t Protocol_Pack(uint8_t type, const uint8_t *payload, uint8_t len,
                       uint16_t seq, uint32_t tick, uint8_t *out)
{
    uint16_t idx = 0U;
    uint16_t i;

    out[idx++] = FRAME_HEAD0;
    out[idx++] = FRAME_HEAD1;
    out[idx++] = type;
    out[idx++] = len;

    out[idx++] = (uint8_t)(seq & 0xFFU);
    out[idx++] = (uint8_t)((seq >> 8) & 0xFFU);

    out[idx++] = (uint8_t)(tick & 0xFFU);
    out[idx++] = (uint8_t)((tick >> 8) & 0xFFU);
    out[idx++] = (uint8_t)((tick >> 16) & 0xFFU);
    out[idx++] = (uint8_t)((tick >> 24) & 0xFFU);

    if ((payload != 0) && (len > 0U)) {
        for (i = 0U; i < len; i++) {
            out[idx++] = payload[i];
        }
    }

    /* CRC 覆盖从 TYPE 到 PAYLOAD 结束的所有字节（不含 2 字节帧头）
     * 长度 = TYPE(1) + LEN(1) + SEQ(2) + TICK(4) + len = 8 + len */
    out[idx] = CRC8_Calc(&out[2], (uint16_t)(8U + len));
    idx++;

    return idx;
}

/**
 * @brief  小端写入 int16 到缓冲区
 */
static void put_i16(uint8_t *buf, uint16_t pos, int16_t v)
{
    uint16_t u = (uint16_t)v;
    buf[pos]     = (uint8_t)(u & 0xFFU);
    buf[pos + 1U] = (uint8_t)((u >> 8) & 0xFFU);
}

/**
 * @brief  打包 IMU + ADC 数据帧
 */
uint16_t Protocol_PackData(const frame_data_payload_t *p,
                           uint16_t seq, uint32_t tick, uint8_t *out)
{
    uint8_t payload[FRAME_DATA_PAYLOAD];
    uint16_t pos = 0U;
    uint8_t i;

    for (i = 0U; i < 3U; i++) {
        put_i16(payload, pos, p->accel[i]);
        pos += 2U;
    }
    for (i = 0U; i < 3U; i++) {
        put_i16(payload, pos, p->gyro[i]);
        pos += 2U;
    }
    put_i16(payload, pos, p->mpu_temp_x10);
    pos += 2U;
    for (i = 0U; i < 3U; i++) {
        put_i16(payload, pos, p->angle_x100[i]);
        pos += 2U;
    }
    for (i = 0U; i < 3U; i++) {
        payload[pos++] = (uint8_t)(p->adc[i] & 0xFFU);
        payload[pos++] = (uint8_t)((p->adc[i] >> 8) & 0xFFU);
    }
    put_i16(payload, pos, p->chip_temp_x10);
    pos += 2U;

    return Protocol_Pack(FRAME_TYPE_DATA, payload, (uint8_t)pos, seq, tick, out);
}

/**
 * @brief  解析上位机下发的单字节命令
 * @return 命令枚举，未知字符返回 CMD_NONE
 */
uint8_t Protocol_DecodeCommand(uint8_t c)
{
    switch (c) {
        case CMD_CALIBRATE:
            return CMD_CALIBRATE;
        case CMD_STATUS:
            return CMD_STATUS;
        case CMD_TOGGLE:
            return CMD_TOGGLE;
        case CMD_RESET:
            return CMD_RESET;
        default:
            return CMD_NONE;
    }
}
