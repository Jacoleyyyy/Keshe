/**
 * @file    motor.c
 * @brief   直流电机驱动 (GMR版: 双通道PWM + 编码器delta读数)
 *
 * 每个电机用2路PWM通道控制正反转:
 *   CH1输出 → 正转, CH2=0
 *   CH2输出 → 反转, CH1=0
 *
 * 编码器读取WHEELTEC方式: 读CNT后清零, 获取有符号16位增量
 */

#include "motor.h"
#include <string.h>
#include <math.h>

/* PID参数 (GMR编码器, 500线×4×60=120000脉冲/转) */
#define MOTOR_KP            0.06f
#define MOTOR_KI            0.015f
#define MOTOR_KD            0.008f
#define MOTOR_DT            0.001f
#define MOTOR_OUT_LIMIT     1.0f
#define MOTOR_I_LIMIT       30.0f
#define MOTOR_DEADBAND      0.5f

Motor_t g_motors[MOTOR_NUM];

void Motor_Init(MotorID_t id,
                TIM_HandleTypeDef *htim_ch1, uint32_t ch1,
                TIM_HandleTypeDef *htim_ch2, uint32_t ch2,
                TIM_HandleTypeDef *htim_enc)
{
    Motor_t *m = &g_motors[id];
    memset(m, 0, sizeof(Motor_t));

    m->id = id;
    m->htim_ch1 = htim_ch1;
    m->ch1_channel = ch1;
    m->htim_ch2 = htim_ch2;
    m->ch2_channel = ch2;
    m->htim_enc = htim_enc;

    /* 初始化PID */
    PID_Init(&m->pid, MOTOR_KP, MOTOR_KI, MOTOR_KD, MOTOR_DT, PID_MODE_POSITION);
    PID_SetOutputLimit(&m->pid, MOTOR_OUT_LIMIT);
    PID_SetIntegralParam(&m->pid, MOTOR_I_LIMIT, true);
    PID_SetDeadband(&m->pid, MOTOR_DEADBAND);

    /* 启动PWM (占空比0) */
    if (htim_ch1) HAL_TIM_PWM_Start(htim_ch1, ch1);
    if (htim_ch2) HAL_TIM_PWM_Start(htim_ch2, ch2);
    __HAL_TIM_SET_COMPARE(htim_ch1, ch1, 0);
    __HAL_TIM_SET_COMPARE(htim_ch2, ch2, 0);

    /* 启动编码器 */
    if (htim_enc) {
        __HAL_TIM_SET_COUNTER(htim_enc, 0);
        HAL_TIM_Encoder_Start(htim_enc, TIM_CHANNEL_ALL);
    }
}

void Motor_InitAll(void)
{
    extern TIM_HandleTypeDef htim1, htim9, htim10, htim11;
    extern TIM_HandleTypeDef htim2_enc, htim3_enc, htim4_enc, htim5_enc;

    /* 电机A: TIM10_CH1 + TIM11_CH1, 编码器D=TIM5 */
    Motor_Init(MOTOR_A, &htim10, TIM_CHANNEL_1, &htim11, TIM_CHANNEL_1, &htim5_enc);
    /* 电机B: TIM9_CH1 + TIM9_CH2, 编码器C=TIM4 */
    Motor_Init(MOTOR_B, &htim9, TIM_CHANNEL_1, &htim9, TIM_CHANNEL_2, &htim4_enc);
    /* 电机C: TIM1_CH2 + TIM1_CH1, 编码器A=TIM2 */
    Motor_Init(MOTOR_C, &htim1, TIM_CHANNEL_2, &htim1, TIM_CHANNEL_1, &htim2_enc);
    /* 电机D: TIM1_CH4 + TIM1_CH3, 编码器B=TIM3 */
    Motor_Init(MOTOR_D, &htim1, TIM_CHANNEL_4, &htim1, TIM_CHANNEL_3, &htim3_enc);
}

/**
 * @brief  设置PWM占空比 (正=CH1输出, 负=CH2输出)
 */
void Motor_SetDuty(MotorID_t id, float duty)
{
    Motor_t *m = &g_motors[id];
    if (!m->htim_ch1) return;

    /* 限幅 */
    if (duty > 1.0f) duty = 1.0f;
    if (duty < -1.0f) duty = -1.0f;

    m->duty_cycle = duty;
    uint32_t pulse = (uint32_t)(fabsf(duty) * MOTOR_PWM_MAX);

    if (duty >= 0) {
        __HAL_TIM_SET_COMPARE(m->htim_ch1, m->ch1_channel, pulse);
        __HAL_TIM_SET_COMPARE(m->htim_ch2, m->ch2_channel, 0);
    } else {
        __HAL_TIM_SET_COMPARE(m->htim_ch1, m->ch1_channel, 0);
        __HAL_TIM_SET_COMPARE(m->htim_ch2, m->ch2_channel, pulse);
    }
}

void Motor_SetTargetRPM(MotorID_t id, float rpm)
{
    Motor_t *m = &g_motors[id];
    if (rpm > MOTOR_MAX_RPM) rpm = MOTOR_MAX_RPM;
    if (rpm < -MOTOR_MAX_RPM) rpm = -MOTOR_MAX_RPM;
    m->target_rpm = rpm;
    PID_SetTarget(&m->pid, rpm);
}

void Motor_Stop(MotorID_t id)
{
    Motor_SetTargetRPM(id, 0);
    Motor_SetDuty(id, 0);
}

void Motor_EmergencyStop(void)
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        g_motors[i].target_rpm = 0;
        PID_Reset(&g_motors[i].pid);
        Motor_SetDuty((MotorID_t)i, 0);
    }
}

/**
 * @brief  读取编码器增量 (WHEELTEC方式: 读后清零)
 * @return 本次周期内的编码器脉冲增量 (有符号16位)
 */
int16_t Motor_GetEncoderDelta(MotorID_t id)
{
    Motor_t *m = &g_motors[id];
    if (!m->htim_enc) return 0;

    int16_t delta = (int16_t)(__HAL_TIM_GET_COUNTER(m->htim_enc));
    __HAL_TIM_SET_COUNTER(m->htim_enc, 0);
    m->enc_delta = delta;
    return delta;
}

float Motor_GetCurrentRPM(MotorID_t id)
{
    return g_motors[id].current_rpm;
}

void Motor_ResetEncoder(MotorID_t id)
{
    Motor_t *m = &g_motors[id];
    if (m->htim_enc) __HAL_TIM_SET_COUNTER(m->htim_enc, 0);
    m->enc_delta = 0;
    m->encoder_prev = 0;
}

/**
 * @brief  PID速度更新 (每1ms调用)
 *
 * RPM计算: delta脉冲 / 120000脉冲每转 * 60000ms/min = delta / 2
 * (120000 / 60000 = 2, 所以 RPM = delta * 0.5)
 */
void Motor_PIDUpdate(void)
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        Motor_t *m = &g_motors[i];
        if (!m->htim_ch1) continue;

        int16_t delta = Motor_GetEncoderDelta((MotorID_t)i);
        /* RPM = delta / 120000 * 60000 = delta / 2 */
        m->current_rpm = (float)delta * 0.5f;

        float err = m->target_rpm - m->current_rpm;
        float duty = PID_Update(&m->pid, m->current_rpm);

        if (fabsf(m->target_rpm) < 0.5f) {
            Motor_SetDuty((MotorID_t)i, 0);
        } else {
            Motor_SetDuty((MotorID_t)i, duty);
        }
    }
}
