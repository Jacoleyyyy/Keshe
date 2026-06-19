/**
 * @file    pid.c
 * @brief   PID 控制器实现
 */

#include <math.h>
#include <string.h>
#include "pid.h"

/* ============================================================
 * PID 初始化
 * ============================================================ */
void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd,
              float dt, PID_Mode_t mode)
{
    memset(pid, 0, sizeof(PID_Controller_t));

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;
    pid->mode = mode;

    /* 默认参数 */
    pid->integral_limit = 1000.0f;
    pid->output_limit = 1.0f;
    pid->deadband = 0.0f;
    pid->integral_separation = true;
    pid->enabled = true;
}

/* ============================================================
 * 设置目标
 * ============================================================ */
void PID_SetTarget(PID_Controller_t *pid, float target)
{
    pid->target = target;
}

/* ============================================================
 * 设置输出限幅
 * ============================================================ */
void PID_SetOutputLimit(PID_Controller_t *pid, float limit)
{
    pid->output_limit = limit;
}

/* ============================================================
 * 设置积分参数
 * ============================================================ */
void PID_SetIntegralParam(PID_Controller_t *pid, float limit, bool separation)
{
    pid->integral_limit = limit;
    pid->integral_separation = separation;
}

/* ============================================================
 * 设置死区
 * ============================================================ */
void PID_SetDeadband(PID_Controller_t *pid, float deadband)
{
    pid->deadband = deadband;
}

/* ============================================================
 * 复位
 * ============================================================ */
void PID_Reset(PID_Controller_t *pid)
{
    pid->error = 0.0f;
    pid->error_prev = 0.0f;
    pid->error_sum = 0.0f;
    pid->deriv = 0.0f;
    pid->output = 0.0f;
    pid->current = 0.0f;
}

/* ============================================================
 * 启停
 * ============================================================ */
void PID_Enable(PID_Controller_t *pid, bool enable)
{
    if (!enable) {
        PID_Reset(pid);
    }
    pid->enabled = enable;
}

/* ============================================================
 * PID 核心计算
 * ============================================================ */
float PID_Update(PID_Controller_t *pid, float current)
{
    if (!pid->enabled) {
        pid->current = current;
        return 0.0f;
    }

    pid->current = current;
    pid->error = pid->target - current;

    /* 死区检查 */
    if (fabsf(pid->error) <= pid->deadband) {
        pid->output = 0.0f;
        return 0.0f;
    }

    float p_term = pid->kp * pid->error;

    /* 积分分离: 误差过大时不积分 */
    if (!pid->integral_separation || fabsf(pid->error) < pid->integral_limit * 0.3f) {
        pid->error_sum += pid->error * pid->dt;
        /* 积分限幅 */
        if (pid->error_sum > pid->integral_limit)
            pid->error_sum = pid->integral_limit;
        else if (pid->error_sum < -pid->integral_limit)
            pid->error_sum = -pid->integral_limit;
    } else {
        /* 积分分离: 清零积分项，防止积分饱和 */
        pid->error_sum = 0.0f;
    }

    float i_term = pid->ki * pid->error_sum;

    /* 微分项 (低通滤波简单处理) */
    pid->deriv = (pid->error - pid->error_prev) / pid->dt;
    pid->error_prev = pid->error;
    float d_term = pid->kd * pid->deriv;

    /* 输出计算 */
    if (pid->mode == PID_MODE_POSITION) {
        pid->output = p_term + i_term + d_term;
    } else {
        /* 增量式 */
        pid->output += p_term + i_term + d_term;
    }

    /* 输出限幅 */
    if (pid->output > pid->output_limit)
        pid->output = pid->output_limit;
    else if (pid->output < -pid->output_limit)
        pid->output = -pid->output_limit;

    return pid->output;
}
