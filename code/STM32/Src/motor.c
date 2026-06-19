/**
 * @file    motor.c
 * @brief   直流电机控制实现 (PWM + 编码器 + PID)
 */

#include "motor.h"
#include <string.h>
#include <math.h>

/* 全局电机数组 */
Motor_t g_motors[MOTOR_NUM];

/* PID 参数 (可调) */
#define MOTOR_KP                0.08f
#define MOTOR_KI                0.02f
#define MOTOR_KD                0.01f
#define MOTOR_PID_DT            0.001f  /* 1ms */
#define MOTOR_PID_LIMIT         50.0f
#define MOTOR_OUTPUT_LIMIT      1.0f
#define MOTOR_DEADBAND          1.0f    /* 1 RPM */

/* 最大转速 */
#define MOTOR_MAX_RPM           300.0f

/**
 * @brief  初始化单个电机
 */
void Motor_Init(MotorID_t id,
                TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel,
                TIM_HandleTypeDef *htim_enc,
                GPIO_TypeDef *dir_port, uint16_t dir_pin)
{
    Motor_t *m = &g_motors[id];
    memset(m, 0, sizeof(Motor_t));

    m->id = id;
    m->htim_pwm = htim_pwm;
    m->pwm_channel = pwm_channel;
    m->htim_enc = htim_enc;
    m->direction = MOTOR_DIR_FORWARD;
    m->initialized = true;

    /* 初始化速度 PID */
    PID_Init(&m->pid, MOTOR_KP, MOTOR_KI, MOTOR_KD,
             MOTOR_PID_DT, PID_MODE_POSITION);
    PID_SetOutputLimit(&m->pid, MOTOR_OUTPUT_LIMIT);
    PID_SetIntegralParam(&m->pid, MOTOR_PID_LIMIT, true);
    PID_SetDeadband(&m->pid, MOTOR_DEADBAND);

    /* 启动编码器 */
    if (htim_enc) {
        HAL_TIM_Encoder_Start(htim_enc, TIM_CHANNEL_ALL);
    }

    /* 启动 PWM */
    if (htim_pwm) {
        HAL_TIM_PWM_Start(htim_pwm, pwm_channel);
    }

    /* 方向引脚 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = dir_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(dir_port, &gpio);
}

/* 电机引脚配置表 */
typedef struct {
    TIM_HandleTypeDef *htim_pwm;
    uint32_t pwm_channel;
    TIM_HandleTypeDef *htim_enc;
    GPIO_TypeDef *dir_port;
    uint16_t dir_pin;
} MotorPinConfig_t;

/**
 * @brief  初始化所有电机 (使用 pin_config.h 定义)
 */
void Motor_InitAll(void)
{
    /* 外部需在调用前初始化好定时器句柄 */
    extern TIM_HandleTypeDef htim_motor_pwm;
    extern TIM_HandleTypeDef htim_m1_enc, htim_m2_enc, htim_m3_enc, htim_m4_enc;

    const MotorPinConfig_t configs[MOTOR_NUM] = {
        [MOTOR_FRONT_LEFT]  = { &htim_motor_pwm, M1_PWM_CHANNEL, &htim_m1_enc, M1_DIR_PORT, M1_DIR_PIN },
        [MOTOR_FRONT_RIGHT] = { &htim_motor_pwm, M2_PWM_CHANNEL, &htim_m2_enc, M2_DIR_PORT, M2_DIR_PIN },
        [MOTOR_REAR_LEFT]   = { &htim_motor_pwm, M3_PWM_CHANNEL, &htim_m3_enc, M3_DIR_PORT, M3_DIR_PIN },
        [MOTOR_REAR_RIGHT]  = { &htim_motor_pwm, M4_PWM_CHANNEL, &htim_m4_enc, M4_DIR_PORT, M4_DIR_PIN },
    };

    for (int i = 0; i < MOTOR_NUM; i++) {
        Motor_Init((MotorID_t)i,
                   configs[i].htim_pwm, configs[i].pwm_channel,
                   configs[i].htim_enc,
                   configs[i].dir_port, configs[i].dir_pin);
    }
}

/**
 * @brief  设置电机方向
 */
void Motor_SetDirection(MotorID_t id, MotorDir_t dir)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized) return;
    m->direction = dir;
}

/**
 * @brief  设置 PWM 占空比
 */
void Motor_SetDuty(MotorID_t id, float duty)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized) return;

    /* 限幅 */
    if (duty > 1.0f) duty = 1.0f;
    if (duty < 0.0f) duty = 0.0f;

    /* 根据方向取反 */
    if (duty < 0) {
        m->direction = MOTOR_DIR_BACKWARD;
        duty = -duty;
    }

    m->duty_cycle = duty;

    uint32_t pulse = (uint32_t)(duty * MOTOR_PWM_MAX);
    __HAL_TIM_SET_COMPARE(m->htim_pwm, m->pwm_channel, pulse);

    /* 方向输出 */
    if (m->direction == MOTOR_DIR_FORWARD) {
        switch (id) {
            case MOTOR_FRONT_LEFT:  HAL_GPIO_WritePin(M1_DIR_PORT, M1_DIR_PIN, GPIO_PIN_RESET); break;
            case MOTOR_FRONT_RIGHT: HAL_GPIO_WritePin(M2_DIR_PORT, M2_DIR_PIN, GPIO_PIN_RESET); break;
            case MOTOR_REAR_LEFT:   HAL_GPIO_WritePin(M3_DIR_PORT, M3_DIR_PIN, GPIO_PIN_RESET); break;
            case MOTOR_REAR_RIGHT:  HAL_GPIO_WritePin(M4_DIR_PORT, M4_DIR_PIN, GPIO_PIN_RESET); break;
            default: break;
        }
    } else {
        switch (id) {
            case MOTOR_FRONT_LEFT:  HAL_GPIO_WritePin(M1_DIR_PORT, M1_DIR_PIN, GPIO_PIN_SET); break;
            case MOTOR_FRONT_RIGHT: HAL_GPIO_WritePin(M2_DIR_PORT, M2_DIR_PIN, GPIO_PIN_SET); break;
            case MOTOR_REAR_LEFT:   HAL_GPIO_WritePin(M3_DIR_PORT, M3_DIR_PIN, GPIO_PIN_SET); break;
            case MOTOR_REAR_RIGHT:  HAL_GPIO_WritePin(M4_DIR_PORT, M4_DIR_PIN, GPIO_PIN_SET); break;
            default: break;
        }
    }
}

/**
 * @brief  设置目标转速
 */
void Motor_SetTargetRPM(MotorID_t id, float rpm)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized) return;

    /* 限幅 */
    if (rpm > MOTOR_MAX_RPM) rpm = MOTOR_MAX_RPM;
    if (rpm < -MOTOR_MAX_RPM) rpm = -MOTOR_MAX_RPM;

    m->target_rpm = rpm;
    PID_SetTarget(&m->pid, rpm);

    /* 若目标转速为0，停止 */
    if (fabsf(rpm) < 0.1f) {
        Motor_SetDuty(id, 0.0f);
    }
}

/**
 * @brief  停止单个电机
 */
void Motor_Stop(MotorID_t id)
{
    Motor_SetTargetRPM(id, 0.0f);
    Motor_SetDuty(id, 0.0f);
}

/**
 * @brief  急停所有电机
 */
void Motor_EmergencyStop(void)
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        g_motors[i].target_rpm = 0.0f;
        PID_Reset(&g_motors[i].pid);
        __HAL_TIM_SET_COMPARE(g_motors[i].htim_pwm, g_motors[i].pwm_channel, 0);
    }
}

/**
 * @brief  读取编码器计数值
 */
int32_t Motor_GetEncoderCount(MotorID_t id)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized || !m->htim_enc) return 0;

    int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(m->htim_enc);
    return count;
}

/**
 * @brief  获取当前转速 (RPM)
 */
float Motor_GetCurrentRPM(MotorID_t id)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized) return 0.0f;
    return m->current_rpm;
}

/**
 * @brief  获取电机里程 (转数)
 */
float Motor_GetRevolutions(MotorID_t id)
{
    return (float)Motor_GetEncoderCount(id) / (float)PULSE_PER_REV;
}

/**
 * @brief  复位编码器
 */
void Motor_ResetEncoder(MotorID_t id)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized || !m->htim_enc) return;
    __HAL_TIM_SET_COUNTER(m->htim_enc, 0);
    m->encoder_count = 0;
    m->encoder_prev = 0;
}

/**
 * @brief  检查是否到达目标转速
 */
bool Motor_IsSpeedReached(MotorID_t id, float tolerance_rpm)
{
    Motor_t *m = &g_motors[id];
    if (!m->initialized) return true;
    return (fabsf(m->current_rpm - m->target_rpm) <= tolerance_rpm);
}

/**
 * @brief  PID 调速更新 (周期调用, 例如 1kHz)
 */
void Motor_PIDUpdate(void)
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        Motor_t *m = &g_motors[i];
        if (!m->initialized) continue;

        /* 计算当前转速 (RPM) */
        int32_t enc_current = Motor_GetEncoderCount((MotorID_t)i);
        int32_t enc_diff = enc_current - m->encoder_prev;
        m->encoder_prev = enc_current;
        m->encoder_count = enc_current;

        /* 转速 = Δ脉冲 / PPR / 减速比 / dt * 60 */
        /* enc_diff 是 1ms 内的脉冲差 → RPM = enc_diff/(PULSE_PER_REV)*60*1000 */
        m->current_rpm = (float)enc_diff / (float)PULSE_PER_REV * 60000.0f;

        /* 处理方向: 根据目标转速符号确定方向 */
        if (m->target_rpm < 0) {
            Motor_SetDirection((MotorID_t)i, MOTOR_DIR_BACKWARD);
            PID_SetTarget(&m->pid, -m->target_rpm);
        } else {
            Motor_SetDirection((MotorID_t)i, MOTOR_DIR_FORWARD);
            PID_SetTarget(&m->pid, m->target_rpm);
        }

        /* PID 计算 */
        float duty = PID_Update(&m->pid, fabsf(m->current_rpm));

        /* 应用 PWM */
        if (fabsf(m->target_rpm) < 0.5f) {
            Motor_SetDuty((MotorID_t)i, 0.0f);
        } else {
            Motor_SetDuty((MotorID_t)i, duty);
        }
    }
}
