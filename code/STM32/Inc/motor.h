/**
 * @file    motor.h
 * @brief   直流电机控制 (GMR版: 双通道PWM, 无方向IO)
 */

#ifndef __MOTOR_H
#define __MOTOR_H

#include "pin_config.h"
#include "pid.h"

/* 电机编号 - 对应WHEELTEC命名 */
typedef enum {
    MOTOR_A = 0,
    MOTOR_B = 1,
    MOTOR_C = 2,
    MOTOR_D = 3,
    MOTOR_NUM
} MotorID_t;

/* 单个电机结构 */
typedef struct {
    MotorID_t id;
    float target_rpm;
    float current_rpm;
    int32_t encoder_count;
    int32_t encoder_prev;
    float duty_cycle;
    PID_Controller_t pid;

    /* 双通道PWM */
    TIM_HandleTypeDef *htim_ch1, *htim_ch2;
    uint32_t ch1_channel, ch2_channel;

    /* 编码器 */
    TIM_HandleTypeDef *htim_enc;
    int16_t enc_delta;      /* 本次读数增量 (有符号16位) */
} Motor_t;

extern Motor_t g_motors[MOTOR_NUM];

void Motor_InitAll(void);
void Motor_Init(MotorID_t id,
                TIM_HandleTypeDef *htim_ch1, uint32_t ch1,
                TIM_HandleTypeDef *htim_ch2, uint32_t ch2,
                TIM_HandleTypeDef *htim_enc);
void Motor_SetTargetRPM(MotorID_t id, float rpm);
void Motor_SetDuty(MotorID_t id, float duty);
void Motor_Stop(MotorID_t id);
void Motor_EmergencyStop(void);
int16_t Motor_GetEncoderDelta(MotorID_t id);
float Motor_GetCurrentRPM(MotorID_t id);
void Motor_PIDUpdate(void);
void Motor_ResetEncoder(MotorID_t id);

#endif
