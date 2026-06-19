/**
 * @file    servo.h
 * @brief   舵机控制模块 (机械臂4自由度 + 软性手爪)
 */

#ifndef __SERVO_H
#define __SERVO_H

#include "pin_config.h"

/* 舵机编号 */
typedef enum {
    SERVO_BASE     = 0,  /* 腰部旋转舵机 */
    SERVO_SHOULDER = 1,  /* 大臂舵机 */
    SERVO_ELBOW    = 2,  /* 小臂舵机 */
    SERVO_GRIPPER  = 3,  /* 手爪舵机 */
    SERVO_NUM
} ServoID_t;

/* 机械臂预设姿态 */
typedef enum {
    ARM_POSE_IDLE,        /* 空闲/行驶姿态 */
    ARM_POSE_SCAN,        /* 扫描二维码姿态 */
    ARM_POSE_PRE_PICK,    /* 预抓取姿态 */
    ARM_POSE_PICK,        /* 抓取姿态 */
    ARM_POSE_CARRY,       /* 运输姿态 */
    ARM_POSE_PLACE,       /* 放置姿态 */
    ARM_POSE_STACK,       /* 码垛姿态 (堆叠) */
    ARM_POSE_NUM
} ArmPose_t;

/* 舵机配置 */
typedef struct {
    ServoID_t id;
    uint16_t angle_min;
    uint16_t angle_max;
    uint16_t pulse_min;      /* 最小角度对应脉冲 (典型500us) */
    uint16_t pulse_max;      /* 最大角度对应脉冲 (典型2500us) */
    uint16_t current_angle;
    float current_pulse;     /* 当前脉宽 */
    bool initialized;
} Servo_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  舵机模块初始化
 */
void Servo_Init(TIM_HandleTypeDef *htim);

/**
 * @brief  设置舵机角度 (0-180度)
 */
void Servo_SetAngle(ServoID_t id, uint16_t angle);

/**
 * @brief  设置舵机脉宽 (微秒)
 */
void Servo_SetPulse(ServoID_t id, uint16_t pulse_us);

/**
 * @brief  获取当前角度
 */
uint16_t Servo_GetAngle(ServoID_t id);

/**
 * @brief  设置手爪开合
 * @param  open  true=张开, false=闭合
 */
void Servo_GripperSet(bool open);

/**
 * @brief  设置机械臂到预设姿态
 * @param  pose  预设姿态枚举
 */
void Servo_SetArmPose(ArmPose_t pose);

/**
 * @brief  缓慢移动舵机到目标角度 (带插值)
 * @param  id     舵机编号
 * @param  angle  目标角度
 * @param  step_delay_ms 每步延时 (ms), 越小越快
 */
void Servo_SmoothMove(ServoID_t id, uint16_t angle, uint16_t step_delay_ms);

#endif /* __SERVO_H */
