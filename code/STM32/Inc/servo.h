/**
 * @file    servo.h
 * @brief   舵机控制 (GMR版: TIM8, PC6-PC9, 100Hz)
 */

#ifndef __SERVO_H
#define __SERVO_H

#include "pin_config.h"

typedef enum {
    SERVO_BASE = 0, SERVO_SHOULDER = 1, SERVO_ELBOW = 2, SERVO_GRIPPER = 3,
    SERVO_NUM
} ServoID_t;

typedef enum {
    ARM_POSE_IDLE, ARM_POSE_SCAN, ARM_POSE_PRE_PICK, ARM_POSE_PICK,
    ARM_POSE_CARRY, ARM_POSE_PLACE, ARM_POSE_STACK, ARM_POSE_NUM
} ArmPose_t;

void Servo_Init(TIM_HandleTypeDef *htim);
void Servo_SetAngle(ServoID_t id, uint16_t angle);
void Servo_SetPulse(ServoID_t id, uint16_t pulse_us);
uint16_t Servo_GetAngle(ServoID_t id);
void Servo_GripperSet(bool open);
void Servo_SetArmPose(ArmPose_t pose);
void Servo_SmoothMove(ServoID_t id, uint16_t angle, uint16_t step_delay_ms);

#endif
