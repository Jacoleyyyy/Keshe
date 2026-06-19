/**
 * @file    chassis.h
 * @brief   麦轮底盘运动控制模块
 * @note    实现麦轮逆运动学、路径规划、车道保持
 */

#ifndef __CHASSIS_H
#define __CHASSIS_H

#include "pin_config.h"
#include "protocol.h"
#include "motor.h"
#include "sensor.h"

/* ============================================================
 * 底盘几何参数
 * ============================================================ */
#define CHASSIS_LX_MM           110.0f   /* 轮距X半长 (前后轮距的一半) */
#define CHASSIS_LY_MM           105.0f   /* 轮距Y半长 (左右轮距的一半) */
#define CHASSIS_WHEEL_RADIUS_MM WHEEL_RADIUS_MM

/* ============================================================
 * 底盘运动指令
 * ============================================================ */
typedef enum {
    CHASSIS_MOVE_FORWARD,       /* 前进 (沿Y轴正方向) */
    CHASSIS_MOVE_BACKWARD,      /* 后退 (沿Y轴负方向) */
    CHASSIS_MOVE_LEFT,          /* 左移 (沿X轴负方向) */
    CHASSIS_MOVE_RIGHT,         /* 右移 (沿X轴正方向) */
    CHASSIS_ROTATE_CW,          /* 顺时针旋转 */
    CHASSIS_ROTATE_CCW,         /* 逆时针旋转 */
    CHASSIS_MOVE_DIAG_FL,       /* 左前斜移 */
    CHASSIS_MOVE_DIAG_FR,       /* 右前斜移 */
    CHASSIS_MOVE_DIAG_BL,       /* 左后斜移 */
    CHASSIS_MOVE_DIAG_BR,       /* 右后斜移 */
    CHASSIS_STOP,               /* 停止 */
    CHASSIS_CUSTOM,             /* 自定义速度向量 */
} ChassisMotion_t;

/* 速度向量 (底盘坐标系) */
typedef struct {
    float vx;       /* 横向速度 (mm/s, 右为正) */
    float vy;       /* 纵向速度 (mm/s, 前为正) */
    float wz;       /* 角速度 (deg/s, 逆时针为正) */
} VelocityVector_t;

/* 位置姿态 (全局坐标系) */
typedef struct {
    float x_mm;     /* X坐标 (mm) */
    float y_mm;     /* Y坐标 (mm) */
    float yaw_deg;  /* 偏航角 (deg) */
} Pose2D_t;

/* 底盘状态 */
typedef struct {
    Pose2D_t pose;              /* 当前位置姿态 */
    VelocityVector_t vel;       /* 当前速度 */
    float motor_rpm[4];         /* 4轮目标转速 */
    bool moving;                /* 是否正在运动 */
    bool error;                 /* 是否错误 */
} ChassisState_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  底盘模块初始化
 */
void Chassis_Init(void);

/**
 * @brief  麦轮逆向运动学: 速度向量 → 各轮转速(RPM)
 * @param  vx    横向速度 (mm/s)
 * @param  vy    纵向速度 (mm/s)
 * @param  wz    角速度 (deg/s)
 * @param  rpm   输出: 4轮目标转速 [FL, FR, RL, RR]
 */
void Chassis_InverseKinematics(float vx, float vy, float wz, float rpm[4]);

/**
 * @brief  设置底盘速度向量
 */
void Chassis_SetVelocity(float vx, float vy, float wz);

/**
 * @brief  设置底盘运动模式 (简单指令)
 * @param  motion  运动模式
 * @param  speed   速度 (mm/s 或 deg/s)
 */
void Chassis_SetMotion(ChassisMotion_t motion, float speed);

/**
 * @brief  停止底盘
 */
void Chassis_Stop(void);

/**
 * @brief  紧急停止
 */
void Chassis_EmergencyStop(void);

/**
 * @brief  导航到目标点 (阻塞/异步)
 * @param  target_x    目标X坐标 (mm)
 * @param  target_y    目标Y坐标 (mm)
 * @param  target_yaw  目标朝向 (deg)
 * @param  speed       运动速度 (mm/s)
 * @return true=已到达, false=运动中
 */
bool Chassis_NavigateTo(float target_x, float target_y, float target_yaw, float speed);

/**
 * @brief  检查是否到达目标
 */
bool Chassis_IsAtTarget(float target_x, float target_y, float tol_mm);

/**
 * @brief  获取当前位姿
 */
Pose2D_t Chassis_GetPose(void);

/**
 * @brief  更新里程计 (由编码器中断/任务周期调用)
 */
void Chassis_UpdateOdometry(void);

/**
 * @brief  复位里程计
 */
void Chassis_ResetOdometry(void);

/**
 * @brief  车道跟随控制 (基于灰度传感器)
 * @param  gray       灰度传感器数据
 * @param  forward_speed 前进速度
 */
void Chassis_LaneFollow(const GraySensor_t *gray, float forward_speed);

/**
 * @brief  沿车道行驶指定距离, 保持居中
 * @param  distance_mm  行驶距离 (mm, 正=前, 负=后)
 * @param  speed        速度 (mm/s)
 * @return true=已到达
 */
bool Chassis_MoveAlongLane(float distance_mm, float speed);

/**
 * @brief  设置目标转速到4个电机 (由 Chassis 控制循环调用)
 */
void Chassis_ApplyMotorRPM(const float rpm[4]);

#endif /* __CHASSIS_H */
