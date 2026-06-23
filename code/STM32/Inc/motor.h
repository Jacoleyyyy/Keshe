/**
 * @file    motor.h
 * @brief   直流电机控制 (GMR版: 双通道PWM, 无方向IO)
 */

#ifndef __MOTOR_H
#define __MOTOR_H

#include <stdbool.h>
#include "pin_config.h"
#include "protocol.h"
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

/* === 运行时 PID 参数 (AI 可通过串口/MCP动态调整) === */
extern float g_pid_kp;          /* 比例系数 */
extern float g_pid_ki;          /* 积分系数 */
extern float g_pid_kd;          /* 微分系数 */
extern float g_pid_out_limit;   /* 输出限幅 */
extern float g_pid_i_limit;     /* 积分限幅 */
extern float g_pid_deadband;    /* 死区 (RPM) */

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

/**
 * @brief  将当前 g_pid_kp/ki/kd 应用到所有4个电机
 * @note   AI 通过串口修改参数后调用此函数, 无需重新编译烧录
 */
void Motor_ApplyPIDGains(void);

/**
 * @brief  PID 阶跃测试: 设目标转速, 通过 printf 输出实时响应
 * @param  motor_id    电机编号 (MOTOR_A ~ MOTOR_D)
 * @param  target_rpm  目标转速 (RPM)
 * @param  step_ms     测试持续时长 (ms), 建议 200~500
 * @note   调用前需保证 printf 已重定向到串口
 *         输出格式: "STEP,<motor>,<tick>,<target>,<actual>,<error>,<duty>"
 *         AI 收集后分析阶跃响应 (上升时间/超调/稳态误差)
 */
void Motor_RunStepTest(MotorID_t motor_id, float target_rpm, uint32_t step_ms);

#endif
