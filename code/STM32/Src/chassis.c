/**
 * @file    chassis.c
 * @brief   麦轮底盘运动控制 (GMR版: 75mm轮径, 93+85mm轮轴距)
 *
 * 麦轮逆运动学 (与WHEELTEC公式符号一致):
 *   Motor_A (FL) = +Vy + Vx - Vz*(Axle + Wheel)
 *   Motor_B (FR) = -Vy + Vx - Vz*(Axle + Wheel)
 *   Motor_C (RL) = +Vy + Vx + Vz*(Axle + Wheel)
 *   Motor_D (RR) = -Vy + Vx + Vz*(Axle + Wheel)
 *
 * 正向运动学 (里程计):
 *   Vx = (R/4)*( +ωA - ωB + ωC - ωD )
 *   Vy = (R/4)*( +ωA - ωB + ωC - ωD ) 中的Vy分量
 *   注意: A/C同向, B/D反向
 */

#include "chassis.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

static Pose2D_t g_pose;

void Chassis_Init(void)
{
    memset(&g_pose, 0, sizeof(g_pose));
}

/**
 * @brief  逆运动学: 速度向量 → 4轮转速 (RPM)
 *
 * @note   符号约定与WHEELTEC保持一致:
 *         Vx: 右为正, Vy: 前为正, Vz: 逆时针为正
 */
void Chassis_InverseKinematics(float vx, float vy, float vz, float rpm[4])
{
    float wz_r = vz * M_PI / 180.0f;
    float L = CHASSIS_LX_MM + CHASSIS_LY_MM;  /* 93 + 85 = 178mm */

    /* 各轮线速度 (mm/s), 符号与WHEELTEC Drive_Motor一致 */
    float v_A = +vy + vx - wz_r * L;
    float v_B = -vy + vx - wz_r * L;
    float v_C = +vy + vx + wz_r * L;
    float v_D = -vy + vx + wz_r * L;

    /* mm/s → RPM: v / (PI * D) * 60 */
    float factor = 60.0f / (M_PI * WHEEL_DIAMETER_MM);

    rpm[MOTOR_A] = v_A * factor;
    rpm[MOTOR_B] = v_B * factor;
    rpm[MOTOR_C] = v_C * factor;
    rpm[MOTOR_D] = v_D * factor;
}

void Chassis_SetVelocity(float vx, float vy, float wz)
{
    float rpm[4];
    Chassis_InverseKinematics(vx, vy, wz, rpm);
    Chassis_ApplyMotorRPM(rpm);
}

void Chassis_ApplyMotorRPM(const float rpm[4])
{
    for (int i = 0; i < 4; i++)
        Motor_SetTargetRPM((MotorID_t)i, rpm[i]);
}

void Chassis_SetMotion(ChassisMotion_t motion, float speed)
{
    switch (motion) {
        case CHASSIS_MOVE_FORWARD:  Chassis_SetVelocity(0, speed, 0); break;
        case CHASSIS_MOVE_BACKWARD: Chassis_SetVelocity(0, -speed, 0); break;
        case CHASSIS_MOVE_LEFT:     Chassis_SetVelocity(-speed, 0, 0); break;
        case CHASSIS_MOVE_RIGHT:    Chassis_SetVelocity(speed, 0, 0); break;
        case CHASSIS_ROTATE_CW:     Chassis_SetVelocity(0, 0, -speed); break;
        case CHASSIS_ROTATE_CCW:    Chassis_SetVelocity(0, 0, speed); break;
        default:                    Chassis_Stop(); break;
    }
}

void Chassis_Stop(void)     { Chassis_SetVelocity(0, 0, 0); }
void Chassis_EmergencyStop(void) { Motor_EmergencyStop(); }

/**
 * @brief  里程计更新 (正向运动学)
 *
 * 从4轮转速反推底盘速度, 积分到位姿。
 * 编码器→RPM由Motor_PIDUpdate完成, 这里直接用 current_rpm。
 */
void Chassis_UpdateOdometry(void)
{
    float rpm[4], w[4];
    for (int i = 0; i < 4; i++)
        rpm[i] = Motor_GetCurrentRPM((MotorID_t)i);

    /* RPM → rad/s */
    for (int i = 0; i < 4; i++)
        w[i] = rpm[i] * 2.0f * M_PI / 60.0f;

    float R = WHEEL_DIAMETER_MM / 2.0f;
    float L = CHASSIS_LX_MM + CHASSIS_LY_MM;

    /* 正向: A=+Vy+Vx, B=-Vy+Vx, C=+Vy+Vx, D=-Vy+Vx (忽略Vz项简化) */
    float vx = (R / 4.0f) * ( w[MOTOR_A] - w[MOTOR_B] + w[MOTOR_C] - w[MOTOR_D] );
    float vy = (R / 4.0f) * ( w[MOTOR_A] + w[MOTOR_B] + w[MOTOR_C] + w[MOTOR_D] );
    /* wz ≈ 0, 由角度传感器辅助 */

    float dt = 0.001f;
    float yaw_rad = g_pose.yaw_deg * M_PI / 180.0f;
    float c = cosf(yaw_rad), s = sinf(yaw_rad);

    g_pose.x_mm += (vx * c - vy * s) * dt;
    g_pose.y_mm += (vx * s + vy * c) * dt;

    while (g_pose.yaw_deg > 180.0f) g_pose.yaw_deg -= 360.0f;
    while (g_pose.yaw_deg < -180.0f) g_pose.yaw_deg += 360.0f;
}

Pose2D_t Chassis_GetPose(void) { return g_pose; }

void Chassis_ResetOdometry(void)
{
    memset(&g_pose, 0, sizeof(g_pose));
}

/**
 * @brief  点对点导航 (比例控制)
 */
bool Chassis_NavigateTo(float tx, float ty, float tyaw, float speed)
{
    float dx = tx - g_pose.x_mm;
    float dy = ty - g_pose.y_mm;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < POSITION_TOLERANCE_MM) {
        float de = tyaw - g_pose.yaw_deg;
        while (de > 180.0f) de -= 360.0f;
        while (de < -180.0f) de += 360.0f;
        if (fabsf(de) < ANGLE_TOLERANCE_DEG) { Chassis_Stop(); return true; }
        Chassis_SetVelocity(0, 0, (de > 0) ? 30.0f : -30.0f);
        return false;
    }

    float target_angle = atan2f(dy, dx) * 180.0f / M_PI;
    float angle_err = target_angle - g_pose.yaw_deg;
    while (angle_err > 180.0f) angle_err -= 360.0f;
    while (angle_err < -180.0f) angle_err += 360.0f;

    if (fabsf(angle_err) > 15.0f) {
        Chassis_SetVelocity(0, 0, (angle_err > 0) ? 45.0f : -45.0f);
    } else {
        float v = (dist < 200.0f) ? fminf(speed, APPROACH_SPEED_MM_S) : speed;
        Chassis_SetVelocity(sinf(angle_err * M_PI / 180.0f) * v * 0.3f, v, angle_err * 2.0f);
    }
    return false;
}

bool Chassis_IsAtTarget(float tx, float ty, float tol)
{
    float dx = tx - g_pose.x_mm, dy = ty - g_pose.y_mm;
    return sqrtf(dx * dx + dy * dy) < tol;
}

/**
 * @brief  车道跟随
 */
void Chassis_LaneFollow(const GraySensor_t *gray, float fwd)
{
    if (!gray) { Chassis_Stop(); return; }
    int8_t off = Sensor_GetLaneOffset(gray);
    float kp = 3.0f;
    float vx = (float)off * kp;
    if (vx > 100.0f) vx = 100.0f; if (vx < -100.0f) vx = -100.0f;
    if (!gray->on_line || off < -80 || off > 80) fwd *= 0.3f;
    Chassis_SetVelocity(-vx, fwd, -(float)off * 1.5f);
}
