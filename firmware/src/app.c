/**
 * @file    app.c
 * @brief   应用主逻辑
 *
 * 主循环采用"中断置标志 + 主循环干活"的结构：
 *   TIM2 每 5ms 产生一次节拍（200Hz），中断里只置标志；
 *   主循环取到标志后做 I2C 读传感器 -> 姿态解算 -> 读 ADC -> 组帧发送。
 * 这样中断服务函数极短，不会阻塞 SysTick 和串口接收。
 */
#include "app.h"
#include "system.h"
#include "usart.h"
#include "soft_i2c.h"
#include "mpu6050.h"
#include "adc_dma.h"
#include "tim.h"
#include "imu.h"
#include "protocol.h"
#include "stm32f103xb.h"

/* ------------------------------------------------------------- 内部状态 */
static uint8_t              s_tx_buf[FRAME_MAX_SIZE];
static uint8_t              s_status_buf[FRAME_MAX_SIZE];
static frame_data_payload_t s_payload;
static uint16_t             s_seq = 0U;
static uint32_t             s_sample_count = 0U;
static uint16_t             s_i2c_err = 0U;
static uint16_t             s_i2c_err_cont = 0U;
static uint8_t              s_upload_on = 1U;
static uint8_t              s_mpu_ok = 0U;
static mpu6050_cal_t        s_cal;
static imu_angle_t          s_angle;

/* --------------------------------------------------------- 小工具函数 */

/** 板载 LED（PC13，低电平点亮）翻转 */
static void LED_Toggle(void)
{
    static uint8_t state = 0U;
    if (state == 0U) {
        GPIOC->BRR = (1UL << 13U);
        state = 1U;
    } else {
        GPIOC->BSRR = (1UL << 13U);
        state = 0U;
    }
}

/** 发送一条文本日志帧（上位机终端窗口会显示） */
static void log_msg(const char *msg)
{
    uint16_t len = 0U;
    while (msg[len] != '\0') {
        len++;
    }
    len = Protocol_Pack(FRAME_TYPE_LOG, (const uint8_t *)msg, (uint8_t)len,
                        s_seq++, millis(), s_status_buf);
    (void)USART1_SendDMA(s_status_buf, len);
}

/** 小端写入工具 */
static void put_u16(uint8_t *b, uint16_t p, uint16_t v)
{
    b[p]     = (uint8_t)(v & 0xFFU);
    b[p + 1U] = (uint8_t)((v >> 8) & 0xFFU);
}
static void put_i16(uint8_t *b, uint16_t p, int16_t v)
{
    put_u16(b, p, (uint16_t)v);
}
static void put_u32(uint8_t *b, uint16_t p, uint32_t v)
{
    b[p]     = (uint8_t)(v & 0xFFU);
    b[p + 1U] = (uint8_t)((v >> 8) & 0xFFU);
    b[p + 2U] = (uint8_t)((v >> 16) & 0xFFU);
    b[p + 3U] = (uint8_t)((v >> 24) & 0xFFU);
}

/**
 * @brief  构造并发送状态帧
 */
static void send_status(void)
{
    uint8_t  p[32];
    uint16_t pos = 0U;
    uint8_t  i;
    uint16_t len;

    put_u32(p, pos, millis());              pos += 4U;
    put_u16(p, pos, USART1_GetDropped());   pos += 2U;
    put_u16(p, pos, (uint16_t)TIM2_GetOverrun()); pos += 2U;
    put_u16(p, pos, s_i2c_err);             pos += 2U;
    p[pos++] = s_cal.valid;
    for (i = 0U; i < 3U; i++) {
        put_i16(p, pos, (i == 0U) ? s_cal.gx : ((i == 1U) ? s_cal.gy : s_cal.gz));
        pos += 2U;
    }
    for (i = 0U; i < 3U; i++) {
        put_i16(p, pos, (i == 0U) ? s_cal.ax : ((i == 1U) ? s_cal.ay : s_cal.az));
        pos += 2U;
    }
    put_u32(p, pos, SystemCoreClock);       pos += 4U;
    p[pos++] = s_upload_on;
    p[pos++] = s_mpu_ok;

    len = Protocol_Pack(FRAME_TYPE_STATUS, p, (uint8_t)pos, s_seq++, millis(), s_status_buf);
    (void)USART1_SendDMA(s_status_buf, len);
}

/**
 * @brief  处理上位机下发的命令
 */
static void handle_command(uint8_t cmd)
{
    switch (cmd) {
        case CMD_CALIBRATE:
            log_msg("calibrating... keep the board still");
            if (MPU6050_Calibrate(&s_cal, 256U) == 0U) {
                log_msg("calibration done");
            } else {
                log_msg("calibration failed");
            }
            IMU_Reset();
            break;

        case CMD_STATUS:
            send_status();
            break;

        case CMD_TOGGLE:
            s_upload_on = (uint8_t)(s_upload_on ^ 1U);
            log_msg(s_upload_on ? "upload ON" : "upload OFF");
            break;

        case CMD_RESET:
            s_i2c_err = 0U;
            s_sample_count = 0U;
            IMU_Reset();
            log_msg("stats reset");
            break;

        default:
            break;
    }
}

/**
 * @brief  执行一次采样并更新 payload
 */
static void do_sample(void)
{
    mpu6050_raw_t raw;
    float dt = 1.0f / (float)APP_SAMPLE_HZ;
    uint8_t i;

    /* 1. 突发读 14 字节 */
    if (MPU6050_ReadRaw(&raw) != 0U) {
        s_i2c_err++;
        s_i2c_err_cont++;
        if (s_i2c_err_cont >= 50U) {
            /* 连续失败：总线可能挂了，恢复后重新初始化传感器 */
            s_i2c_err_cont = 0U;
            SoftI2C_BusRecovery();
            if (MPU6050_Init() == 0U) {
                s_mpu_ok = 1U;
                log_msg("MPU6050 re-init OK");
            } else {
                s_mpu_ok = 0U;
                log_msg("MPU6050 re-init FAILED");
            }
        }
        return;
    }
    s_i2c_err_cont = 0U;

    /* 2. 扣零偏 + 姿态解算 */
    MPU6050_ApplyCal(&raw, &s_cal);
    IMU_Update(&raw, dt, &s_angle);

    /* 3. 填充数据帧 payload */
    for (i = 0U; i < 3U; i++) {
        s_payload.accel[i] = (i == 0U) ? raw.ax : ((i == 1U) ? raw.ay : raw.az);
        s_payload.gyro[i]  = (i == 0U) ? raw.gx : ((i == 1U) ? raw.gy : raw.gz);
        s_payload.angle_x100[i] = (int16_t)(
            ((i == 0U) ? s_angle.roll : ((i == 1U) ? s_angle.pitch : s_angle.yaw)) * 100.0f);
    }
    s_payload.mpu_temp_x10 = (int16_t)(MPU6050_TemperatureCelsius(raw.temp_raw) * 10.0f);
    s_payload.adc[0] = ADC_GetRaw(ADC_IDX_EXT0);
    s_payload.adc[1] = ADC_GetRaw(ADC_IDX_EXT1);
    s_payload.adc[2] = ADC_GetRaw(ADC_IDX_CHIP_TEMP);
    s_payload.chip_temp_x10 = ADC_GetChipTempX10();
}

/**
 * @brief  应用主循环
 */
void APP_Run(void)
{
    uint32_t last_status_ms = 0U;
    uint32_t last_led_ms = 0U;
    int c;

    /* ---------------- 外设初始化 ---------------- */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xFUL << 20U);
    GPIOC->CRH |= (0x1UL << 20U);        /* PC13 通用开漏输出 10MHz */
    GPIOC->BSRR = (1UL << 13U);          /* 初始熄灭 */

    USART1_Init(115200U);
    USART1_SendString("\r\n[boot] STM32F103 IMU monitor\r\n");

    SoftI2C_Init();
    ADC1_DMA_Init();
    TIM2_Init(APP_SAMPLE_HZ);
    IMU_Init();

    s_cal.valid = 0U;
    s_cal.ax = s_cal.ay = s_cal.az = 0;
    s_cal.gx = s_cal.gy = s_cal.gz = 0;

    /* ---------------- 传感器初始化 ---------------- */
    if (MPU6050_Init() == 0U) {
        s_mpu_ok = 1U;
        USART1_SendString("[ok] MPU6050 detected\r\n");
        log_msg("MPU6050 init OK");
        /* 上电自动零偏校准：板子必须静止放置 */
        log_msg("auto calibration... keep still");
        (void)MPU6050_Calibrate(&s_cal, 256U);
    } else {
        s_mpu_ok = 0U;
        USART1_SendString("[err] MPU6050 not found, running in ADC-only mode\r\n");
        log_msg("MPU6050 NOT found - ADC only mode");
    }

    last_status_ms = millis();
    last_led_ms = millis();

    /* ---------------- 主循环 ---------------- */
    while (1) {
        /* 1. 采样节拍 */
        if (TIM2_TakeFlag() != 0U) {
            do_sample();
            s_sample_count++;

            /* 2. 按分频上传：采样 200Hz，上传 100Hz */
            if ((s_upload_on != 0U) && ((s_sample_count % APP_UPLOAD_DIV) == 0U)) {
                if (USART1_TxBusy() == 0U) {
                    uint16_t len = Protocol_PackData(&s_payload, s_seq++, millis(), s_tx_buf);
                    (void)USART1_SendDMA(s_tx_buf, len);
                }
            }
        }

        /* 3. 处理上位机命令 */
        while ((c = USART1_RxGetByte()) >= 0) {
            handle_command(Protocol_DecodeCommand((uint8_t)c));
        }

        /* 4. 每 1 秒上报一次运行状态 */
        if ((millis() - last_status_ms) >= 1000U) {
            last_status_ms = millis();
            if (USART1_TxBusy() == 0U) {
                send_status();
            }
        }

        /* 5. 心跳灯 */
        if ((millis() - last_led_ms) >= 250U) {
            last_led_ms = millis();
            LED_Toggle();
        }
    }
}
