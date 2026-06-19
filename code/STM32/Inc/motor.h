/**
 * @file    motor.h
 * @brief   直流电机控制模块 (带编码器 + PID 调速)
 */

#ifndef __MOTOR_H
#define __MOTOR_H

#include "pin_config.h"
#include "pid.h"

/* 电机编号 */
typedef enum {
    MOTOR_FRONT_LEFT  = 0,   /* 左前 */
    MOTOR_FRONT_RIGHT = 1,   /* 右前 */
    MOTOR_REAR_LEFT   = 2,   /* 左后 */
    MOTOR_REAR_RIGHT  = 3,   /* 右后 */
    MOTOR_NUM
} MotorID_t;

/* 电机方向 */
typedef enum {
    MOTOR_DIR_FORWARD  = 0,
    MOTOR_DIR_BACKWARD = 1,
} MotorDir_t;

/* 电机状态 */
typedef struct {
    MotorID_t id;
    MotorDir_t direction;
    float target_rpm;         /* 目标转速 (RPM) */
    float current_rpm;        /* 当前转速 (RPM) */
    int32_t encoder_count;    /* 编码器计数 */
    int32_t encoder_prev;     /* 上一次编码器计数 */
    float duty_cycle;         /* PWM 占空比 (0.0~1.0) */
    PID_Controller_t pid;     /* 速度PID控制器 */
    TIM_HandleTypeDef *htim_pwm;
    uint32_t pwm_channel;
    TIM_HandleTypeDef *htim_enc;
    bool initialized;
} Motor_t;

/* 电机全局句柄 */
extern Motor_t g_motors[MOTOR_NUM];

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  初始化所有4个电机
 */
void Motor_InitAll(void);

/**
 * @brief  初始化单个电机
 */
void Motor_Init(MotorID_t id,
                TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel,
                TIM_HandleTypeDef *htim_enc,
                GPIO_TypeDef *dir_port, uint16_t dir_pin);

/**
 * @brief  设置电机目标转速 (RPM)
 */
void Motor_SetTargetRPM(MotorID_t id, float rpm);

/**
 * @brief  设置电机 PWM 占空比 (0.0~1.0), 无PID控制
 */
void Motor_SetDuty(MotorID_t id, float duty);

/**
 * @brief  设置电机方向
 */
void Motor_SetDirection(MotorID_t id, MotorDir_t dir);

/**
 * @brief  急停 (立即停止所有电机)
 */
void Motor_EmergencyStop(void);

/**
 * @brief  停止某个电机
 */
void Motor_Stop(MotorID_t id);

/**
 * @brief  读取编码器计数值
 */
int32_t Motor_GetEncoderCount(MotorID_t id);

/**
 * @brief  获取当前转速 (RPM)
 */
float Motor_GetCurrentRPM(MotorID_t id);

/**
 * @brief  PID 调速更新 (由定时器中断调用, 如1kHz)
 * @note   此函数应在定时器中断或高优先级任务中周期调用
 */
void Motor_PIDUpdate(void);

/**
 * @brief  获取电机里程 (转数)
 */
float Motor_GetRevolutions(MotorID_t id);

/**
 * @brief  复位编码器计数
 */
void Motor_ResetEncoder(MotorID_t id);

/**
 * @brief  检查是否到达目标转速
 */
bool Motor_IsSpeedReached(MotorID_t id, float tolerance_rpm);

#endif /* __MOTOR_H */
