/**
 * @file    chassis.h
 * @brief   麦轮底盘运动控制 (GMR版: 75mm轮/60:1/93+85mm)
 */

#ifndef __CHASSIS_H
#define __CHASSIS_H

#include "pin_config.h"
#include "protocol.h"
#include "motor.h"
#include "sensor.h"

typedef enum {
    CHASSIS_MOVE_FORWARD, CHASSIS_MOVE_BACKWARD,
    CHASSIS_MOVE_LEFT, CHASSIS_MOVE_RIGHT,
    CHASSIS_ROTATE_CW, CHASSIS_ROTATE_CCW,
    CHASSIS_STOP, CHASSIS_CUSTOM,
} ChassisMotion_t;

typedef struct {
    float vx, vy, wz;
} VelocityVector_t;

typedef struct {
    float x_mm, y_mm, yaw_deg;
} Pose2D_t;

void Chassis_Init(void);
void Chassis_InverseKinematics(float vx, float vy, float wz, float rpm[4]);
void Chassis_SetVelocity(float vx, float vy, float wz);
void Chassis_SetMotion(ChassisMotion_t motion, float speed);
void Chassis_Stop(void);
void Chassis_EmergencyStop(void);
bool Chassis_NavigateTo(float target_x, float target_y, float target_yaw, float speed);
bool Chassis_IsAtTarget(float target_x, float target_y, float tol_mm);
Pose2D_t Chassis_GetPose(void);
void Chassis_UpdateOdometry(void);
void Chassis_ResetOdometry(void);
void Chassis_LaneFollow(const GraySensor_t *gray, float forward_speed);
void Chassis_ApplyMotorRPM(const float rpm[4]);

#endif
