/**
 * @file    chassis.c
 * @brief   麦轮底盘运动控制实现
 * @note    逆运动学 + 里程计 + 车道跟随 + 点对点导航
 */

#include "chassis.h"
#include <math.h>
#include <string.h>

/* 底盘全局状态 */
static ChassisState_t g_chassis;

/* PI 常数 */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* 将 Lx+Ly 预计算 */
#define CHASSIS_L_SUM  (CHASSIS_LX_MM + CHASSIS_LY_MM)

/* ============================================================
 * 初始化
 * ============================================================ */
void Chassis_Init(void)
{
    memset(&g_chassis, 0, sizeof(g_chassis));
    g_chassis.pose.x_mm = 0.0f;
    g_chassis.pose.y_mm = 0.0f;
    g_chassis.pose.yaw_deg = 0.0f;
}

/* ============================================================
 * 麦轮逆向运动学
 *
 * 麦轮速度关系:
 *   ω_FL = (1/R) * ( vy + vx + L_sum * ωz )
 *   ω_FR = (1/R) * ( vy - vx - L_sum * ωz )
 *   ω_RL = (1/R) * ( vy - vx + L_sum * ωz )
 *   ω_RR = (1/R) * ( vy + vx - L_sum * ωz )
 *
 * 注: 不同麦轮布置符号可能不同，需要根据实际测试调整
 * ============================================================ */
void Chassis_InverseKinematics(float vx, float vy, float wz, float rpm[4])
{
    /* ωz 转换: deg/s → rad/s */
    float wz_rad = wz * M_PI / 180.0f;

    /* 线速度 → 角速度 (rad/s) */
    float wheel_radius_m = CHASSIS_WHEEL_RADIUS_MM / 1000.0f;

    /* 各轮线速度 (mm/s) → 角速度 (rad/s) → RPM */
    float v_fl = vy + vx + CHASSIS_L_SUM * wz_rad;
    float v_fr = vy - vx - CHASSIS_L_SUM * wz_rad;
    float v_rl = vy - vx + CHASSIS_L_SUM * wz_rad;
    float v_rr = vy + vx - CHASSIS_L_SUM * wz_rad;

    /* 转 RPM: RPM = v(mm/s) / (2*PI*R) * 60 */
    float factor = 60.0f / (2.0f * M_PI * CHASSIS_WHEEL_RADIUS_MM);

    rpm[MOTOR_FRONT_LEFT]  = v_fl * factor;
    rpm[MOTOR_FRONT_RIGHT] = v_fr * factor;
    rpm[MOTOR_REAR_LEFT]   = v_rl * factor;
    rpm[MOTOR_REAR_RIGHT]  = v_rr * factor;
}

/* ============================================================
 * 设置速度向量
 * ============================================================ */
void Chassis_SetVelocity(float vx, float vy, float wz)
{
    g_chassis.vel.vx = vx;
    g_chassis.vel.vy = vy;
    g_chassis.vel.wz = wz;

    Chassis_InverseKinematics(vx, vy, wz, g_chassis.motor_rpm);

    /* 限幅 */
    for (int i = 0; i < 4; i++) {
        float rpm = g_chassis.motor_rpm[i];
        if (rpm > MAX_LINEAR_SPEED_MM_S / (2.0f * M_PI * CHASSIS_WHEEL_RADIUS_MM) * 60.0f) {
            rpm = MAX_LINEAR_SPEED_MM_S / (2.0f * M_PI * CHASSIS_WHEEL_RADIUS_MM) * 60.0f;
        } else if (rpm < -MAX_LINEAR_SPEED_MM_S / (2.0f * M_PI * CHASSIS_WHEEL_RADIUS_MM) * 60.0f) {
            rpm = -MAX_LINEAR_SPEED_MM_S / (2.0f * M_PI * CHASSIS_WHEEL_RADIUS_MM) * 60.0f;
        }
        g_chassis.motor_rpm[i] = rpm;
    }

    Chassis_ApplyMotorRPM(g_chassis.motor_rpm);
    g_chassis.moving = (fabsf(vx) > 0.5f || fabsf(vy) > 0.5f || fabsf(wz) > 0.1f);
}

/* ============================================================
 * 设置运动模式
 * ============================================================ */
void Chassis_SetMotion(ChassisMotion_t motion, float speed)
{
    switch (motion) {
        case CHASSIS_MOVE_FORWARD:
            Chassis_SetVelocity(0, speed, 0);
            break;
        case CHASSIS_MOVE_BACKWARD:
            Chassis_SetVelocity(0, -speed, 0);
            break;
        case CHASSIS_MOVE_LEFT:
            Chassis_SetVelocity(-speed, 0, 0);
            break;
        case CHASSIS_MOVE_RIGHT:
            Chassis_SetVelocity(speed, 0, 0);
            break;
        case CHASSIS_ROTATE_CW:
            Chassis_SetVelocity(0, 0, -speed);  /* 角速度用 deg/s */
            break;
        case CHASSIS_ROTATE_CCW:
            Chassis_SetVelocity(0, 0, speed);
            break;
        case CHASSIS_MOVE_DIAG_FL:
            Chassis_SetVelocity(-speed * 0.707f, speed * 0.707f, 0);
            break;
        case CHASSIS_MOVE_DIAG_FR:
            Chassis_SetVelocity(speed * 0.707f, speed * 0.707f, 0);
            break;
        case CHASSIS_MOVE_DIAG_BL:
            Chassis_SetVelocity(-speed * 0.707f, -speed * 0.707f, 0);
            break;
        case CHASSIS_MOVE_DIAG_BR:
            Chassis_SetVelocity(speed * 0.707f, -speed * 0.707f, 0);
            break;
        case CHASSIS_STOP:
        default:
            Chassis_Stop();
            break;
    }
}

/* ============================================================
 * 停止
 * ============================================================ */
void Chassis_Stop(void)
{
    Chassis_SetVelocity(0, 0, 0);
    g_chassis.moving = false;
}

void Chassis_EmergencyStop(void)
{
    Chassis_Stop();
    Motor_EmergencyStop();
}

/* ============================================================
 * 应用电机转速
 * ============================================================ */
void Chassis_ApplyMotorRPM(const float rpm[4])
{
    for (int i = 0; i < 4; i++) {
        Motor_SetTargetRPM((MotorID_t)i, rpm[i]);
    }
}

/* ============================================================
 * 里程计更新
 *
 * 从4个编码器推算底盘里程计
 * 麦轮正向运动学 (简化):
 *   vx = (R/4) * ( ω_FL - ω_FR - ω_RL + ω_RR )
 *   vy = (R/4) * ( ω_FL + ω_FR + ω_RL + ω_RR )
 *   ωz = (R/(4*L_sum)) * ( ω_FL - ω_FR + ω_RL - ω_RR )
 * ============================================================ */
void Chassis_UpdateOdometry(void)
{
    float rpm[4];
    for (int i = 0; i < 4; i++) {
        rpm[i] = Motor_GetCurrentRPM((MotorID_t)i);
    }

    /* RPM → rad/s */
    float w[4];
    for (int i = 0; i < 4; i++) {
        w[i] = rpm[i] * 2.0f * M_PI / 60.0f;
    }

    float R = CHASSIS_WHEEL_RADIUS_MM;
    float vx = (R / 4.0f) * ( w[0] - w[1] - w[2] + w[3] );
    float vy = (R / 4.0f) * ( w[0] + w[1] + w[2] + w[3] );
    float wz = (R / (4.0f * CHASSIS_L_SUM)) * ( w[0] - w[1] + w[2] - w[3] );

    /* 积分 dt = PID 更新周期 (0.001s) */
    float dt = 0.001f;

    /* 将底盘坐标系速度转换到全局坐标系 */
    float yaw_rad = g_chassis.pose.yaw_deg * M_PI / 180.0f;
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);

    /* 旋转: [vx_global, vy_global] = R(yaw) * [vx, vy] */
    g_chassis.pose.x_mm += (vx * cos_yaw - vy * sin_yaw) * dt;
    g_chassis.pose.y_mm += (vx * sin_yaw + vy * cos_yaw) * dt;
    g_chassis.pose.yaw_deg += wz * dt * 180.0f / M_PI;

    /* 角度归一化 */
    while (g_chassis.pose.yaw_deg > 180.0f) g_chassis.pose.yaw_deg -= 360.0f;
    while (g_chassis.pose.yaw_deg < -180.0f) g_chassis.pose.yaw_deg += 360.0f;
}

/* ============================================================
 * 获取位姿
 * ============================================================ */
Pose2D_t Chassis_GetPose(void)
{
    return g_chassis.pose;
}

/* ============================================================
 * 复位里程计
 * ============================================================ */
void Chassis_ResetOdometry(void)
{
    g_chassis.pose.x_mm = 0.0f;
    g_chassis.pose.y_mm = 0.0f;
    g_chassis.pose.yaw_deg = 0.0f;
}

/* ============================================================
 * 点对点导航 (简化版，基于比例控制)
 *
 * 使用纯比例控制引导机器人向目标点移动。
 * 在真实实现中应使用更复杂的路径规划（A*等）。
 * ============================================================ */
bool Chassis_NavigateTo(float target_x, float target_y, float target_yaw, float speed)
{
    float dx = target_x - g_chassis.pose.x_mm;
    float dy = target_y - g_chassis.pose.y_mm;
    float dist = sqrtf(dx * dx + dy * dy);

    /* 距离检查 */
    if (dist < POSITION_TOLERANCE_MM) {
        /* 到达目标位置，调整朝向 */
        float dyaw = target_yaw - g_chassis.pose.yaw_deg;

        /* 角度归一化到 [-180, 180] */
        while (dyaw > 180.0f) dyaw -= 360.0f;
        while (dyaw < -180.0f) dyaw += 360.0f;

        if (fabsf(dyaw) < ANGLE_TOLERANCE_DEG) {
            Chassis_Stop();
            return true; /* 已到达 */
        }

        /* 原地旋转调整朝向 */
        float rot_speed = (dyaw > 0) ? 30.0f : -30.0f;
        rot_speed = fminf(fabsf(dyaw), 30.0f) * ((dyaw > 0) ? 1.0f : -1.0f);
        Chassis_SetVelocity(0, 0, rot_speed);
        return false;
    }

    /* 计算目标方向角 */
    float target_angle = atan2f(dy, dx) * 180.0f / M_PI;
    float angle_error = target_angle - g_chassis.pose.yaw_deg;

    /* 角度归一化 */
    while (angle_error > 180.0f) angle_error -= 360.0f;
    while (angle_error < -180.0f) angle_error += 360.0f;

    /* 比例控制: 运动方向由朝向误差决定 */
    if (fabsf(angle_error) > 15.0f) {
        /* 先原地旋转到大致方向 */
        float rot_speed = (angle_error > 0) ? 45.0f : -45.0f;
        Chassis_SetVelocity(0, 0, rot_speed);
    } else {
        /* 向目标行驶 */
        float effective_speed = (dist < 200.0f) ? fminf(speed, APPROACH_SPEED_MM_S) : speed;
        /* 用角度误差修正横向分量 */
        float vx_adj = sinf(angle_error * M_PI / 180.0f) * effective_speed * 0.3f;
        float vy_adj = effective_speed;
        float wz_adj = angle_error * 2.0f;  /* 角度修正 */
        Chassis_SetVelocity(vx_adj, vy_adj, wz_adj);
    }

    return false;
}

/* ============================================================
 * 车道跟随 (基于灰度传感器)
 * ============================================================ */
void Chassis_LaneFollow(const GraySensor_t *gray, float forward_speed)
{
    if (!gray) {
        Chassis_Stop();
        return;
    }

    int8_t offset = Sensor_GetLaneOffset(gray);

    /* PID 风格的横向修正 */
    float kp_lane = 3.0f;  /* 横向修正系数 */
    float vx_correction = (float)offset * kp_lane;
    float wz_correction = (float)offset * 1.5f;

    /* 限幅 */
    if (vx_correction > 100.0f) vx_correction = 100.0f;
    if (vx_correction < -100.0f) vx_correction = -100.0f;
    if (wz_correction > 30.0f) wz_correction = 30.0f;
    if (wz_correction < -30.0f) wz_correction = -30.0f;

    /* 如果掉线 (>80%偏移)，减速并往回找 */
    if (!gray->on_line || offset < -80 || offset > 80) {
        forward_speed *= 0.3f;
    }

    Chassis_SetVelocity(-vx_correction, forward_speed, -wz_correction);
}

/* ============================================================
 * 沿车道行驶距离
 * ============================================================ */
bool Chassis_MoveAlongLane(float distance_mm, float speed)
{
    static float start_x = 0, start_y = 0;
    static bool initialized = false;

    if (!initialized) {
        start_x = g_chassis.pose.x_mm;
        start_y = g_chassis.pose.y_mm;
        initialized = true;
    }

    float dx = g_chassis.pose.x_mm - start_x;
    float dy = g_chassis.pose.y_mm - start_y;
    float traveled = sqrtf(dx * dx + dy * dy);

    if (traveled >= fabsf(distance_mm)) {
        Chassis_Stop();
        initialized = false;
        return true;
    }

    /* 继续行驶 */
    Chassis_SetVelocity(0, (distance_mm > 0) ? speed : -speed, 0);
    return false;
}

/* ============================================================
 * 到达检查
 * ============================================================ */
bool Chassis_IsAtTarget(float target_x, float target_y, float tol_mm)
{
    float dx = target_x - g_chassis.pose.x_mm;
    float dy = target_y - g_chassis.pose.y_mm;
    return (sqrtf(dx * dx + dy * dy) < tol_mm);
}
