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
#include <stdio.h>

/* === 运行时 PID 参数 (AI/MCP 可动态调整) === */
float g_pid_kp         = 0.06f;
float g_pid_ki         = 0.015f;
float g_pid_kd         = 0.008f;
float g_pid_out_limit  = 1.0f;
float g_pid_i_limit    = 30.0f;
float g_pid_deadband   = 0.5f;
#define MOTOR_DT        0.001f  /* 固定 1ms, 不可调 */

/* 阶跃测试状态 */
static volatile bool    g_step_test_running = false;
static volatile uint8_t g_step_motor = 0;
static volatile float   g_step_target = 0;
static volatile uint32_t g_step_ticks = 0;

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

    /* 初始化PID (使用运行时可变参数) */
    PID_Init(&m->pid, g_pid_kp, g_pid_ki, g_pid_kd, MOTOR_DT, PID_MODE_POSITION);
    PID_SetOutputLimit(&m->pid, g_pid_out_limit);
    PID_SetIntegralParam(&m->pid, g_pid_i_limit, true);
    PID_SetDeadband(&m->pid, g_pid_deadband);

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
        /* RPM = delta / 60000 * 60000 = delta (30:1减速比) */
        m->current_rpm = (float)delta;

        float err = m->target_rpm - m->current_rpm;
        float duty = PID_Update(&m->pid, m->current_rpm);

        if (fabsf(m->target_rpm) < 0.5f) {
            Motor_SetDuty((MotorID_t)i, 0);
        } else {
            Motor_SetDuty((MotorID_t)i, duty);
        }

        /* 阶跃测试: 输出 CSV 数据到串口 */
        if (g_step_test_running && (uint8_t)i == g_step_motor) {
            if (g_step_ticks > 0) {
                printf("STEP,%d,%lu,%.1f,%.1f,%.1f,%.3f\r\n",
                       g_step_motor, g_step_ticks,
                       m->target_rpm, m->current_rpm, err, duty);
                g_step_ticks--;
            }
            if (g_step_ticks == 0) {
                g_step_test_running = false;
                Motor_SetTargetRPM((MotorID_t)g_step_motor, 0.0f);
                printf("STEP,%d,END,0,0,0,0\r\n", g_step_motor);
            }
        }
    }
}

/**
 * @brief  将当前全局 PID 参数写入所有 4 个电机
 * @note   AI 修改 g_pid_kp/ki/kd 后调用, 无需重启
 */
void Motor_ApplyPIDGains(void)
{
    for (int i = 0; i < MOTOR_NUM; i++) {
        PID_Controller_t *p = &g_motors[i].pid;
        p->kp = g_pid_kp;
        p->ki = g_pid_ki;
        p->kd = g_pid_kd;
        p->output_limit = g_pid_out_limit;
        p->integral_limit = g_pid_i_limit;
        p->deadband = g_pid_deadband;
    }
    printf("PID:SET kp=%.4f ki=%.4f kd=%.4f\r\n",
           g_pid_kp, g_pid_ki, g_pid_kd);
}

/**
 * @brief  PID 阶跃响应测试
 *
 * 流程:
 *   1. 停止电机 500ms (归零)
 *   2. 设目标转速 = target_rpm
 *   3. 接下来 step_ms 毫秒内, 每个 PID 周期输出一行 CSV:
 *        STEP,<电机编号>,<剩余tick>,<目标RPM>,<实际RPM>,<误差>,<占空比>
 *   4. 测试结束自动停转
 *
 * AI 通过 serial_monitor_start 收集 CSV → 分析阶跃响应 →
 * 计算最佳 Kp/Ki/Kd → 通过 PID: 命令写入 → Motor_ApplyPIDGains()
 */
void Motor_RunStepTest(MotorID_t motor_id, float target_rpm, uint32_t step_ms)
{
    if (motor_id >= MOTOR_NUM) return;

    printf("STEP,%d,START,target=%.1f,duration=%lu\r\n",
           motor_id, target_rpm, step_ms);

    /* 归零 */
    Motor_SetTargetRPM(motor_id, 0.0f);
    HAL_Delay(300);

    /* 启动测试 */
    g_step_motor = (uint8_t)motor_id;
    g_step_target = target_rpm;
    g_step_ticks = step_ms;  /* 1 tick = 1 PID 周期 = 1ms */
    g_step_test_running = true;

    Motor_SetTargetRPM(motor_id, target_rpm);
}
