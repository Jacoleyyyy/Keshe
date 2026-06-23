/**
 * @file    pid.h
 * @brief   PID 控制器模块
 * @note    增量式 + 位置式 PID，带积分分离和输出限幅
 */

#ifndef __PID_H
#define __PID_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* PID 模式 */
typedef enum {
    PID_MODE_POSITION = 0,   /* 位置式 PID */
    PID_MODE_DELTA    = 1,   /* 增量式 PID */
} PID_Mode_t;

/* PID 控制器结构体 */
typedef struct {
    /* 配置参数 */
    float kp;                /* 比例系数 */
    float ki;                /* 积分系数 */
    float kd;                /* 微分系数 */
    float integral_limit;    /* 积分限幅 */
    float output_limit;      /* 输出限幅 */
    float deadband;          /* 死区 */
    float dt;                /* 采样周期 (秒) */

    /* 状态变量 */
    float target;            /* 目标值 */
    float current;           /* 当前值 */
    float error;             /* 当前误差 */
    float error_prev;        /* 上次误差 */
    float error_sum;         /* 误差积分 */
    float deriv;             /* 微分项 */
    float output;            /* 输出值 */

    /* 选项 */
    PID_Mode_t mode;
    bool integral_separation; /* 积分分离 */
    bool enabled;
} PID_Controller_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化 PID 控制器
 * @param  pid   控制器指针
 * @param  kp    比例系数
 * @param  ki    积分系数
 * @param  kd    微分系数
 * @param  dt    采样周期 (秒)
 * @param  mode  模式 (位置式/增量式)
 */
void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd,
              float dt, PID_Mode_t mode);

/**
 * @brief  设置 PID 目标值
 */
void PID_SetTarget(PID_Controller_t *pid, float target);

/**
 * @brief  设置输出限幅
 */
void PID_SetOutputLimit(PID_Controller_t *pid, float limit);

/**
 * @brief  设置积分限幅和分离
 */
void PID_SetIntegralParam(PID_Controller_t *pid, float limit, bool separation);

/**
 * @brief  设置死区
 */
void PID_SetDeadband(PID_Controller_t *pid, float deadband);

/**
 * @brief  复位 PID 状态
 */
void PID_Reset(PID_Controller_t *pid);

/**
 * @brief  启停 PID
 */
void PID_Enable(PID_Controller_t *pid, bool enable);

/**
 * @brief  执行一次 PID 计算
 * @param  pid      控制器指针
 * @param  current  当前采样值
 * @return 控制输出值
 */
float PID_Update(PID_Controller_t *pid, float current);

/**
 * @brief  获取当前输出
 */
static inline float PID_GetOutput(const PID_Controller_t *pid)
{
    return pid->output;
}

/**
 * @brief  获取当前误差
 */
static inline float PID_GetError(const PID_Controller_t *pid)
{
    return pid->error;
}

/**
 * @brief  检查是否在目标附近 (死区内)
 */
static inline bool PID_InDeadband(const PID_Controller_t *pid)
{
    return (fabsf(pid->error) <= pid->deadband);
}

#endif /* __PID_H */
