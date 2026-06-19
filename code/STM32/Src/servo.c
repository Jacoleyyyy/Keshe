/**
 * @file    servo.c
 * @brief   舵机控制实现 (机械臂4自由度)
 */

#include "servo.h"
#include <string.h>
#include <math.h>

/* 舵机全局数组 */
static Servo_t g_servos[SERVO_NUM];
static TIM_HandleTypeDef *g_htim_servo = NULL;

/* 预设姿态角度表 [SERVO_BASE, SERVO_SHOULDER, SERVO_ELBOW, SERVO_GRIPPER] */
static const uint16_t g_pose_angles[ARM_POSE_NUM][SERVO_NUM] = {
    /* 空闲/行驶姿态 - 机械臂收缩 */
    [ARM_POSE_IDLE]       = { 90, 20, 10,  5 },   /* 闭合, 收缩 */
    /* 扫描二维码姿态 - 相机朝前 */
    [ARM_POSE_SCAN]       = { 90, 60, 30,  5 },
    /* 预抓取姿态 - 接近物料 */
    [ARM_POSE_PRE_PICK]   = { 90, 80, 60, 60 },   /* 张开手爪 */
    /* 抓取姿态 - 闭合手爪 */
    [ARM_POSE_PICK]       = { 90, 85, 65,  5 },   /* 闭合手爪夹紧 */
    /* 运输姿态 - 收缩行驶 */
    [ARM_POSE_CARRY]      = { 90, 50, 40,  5 },
    /* 放置姿态 - 在粗加工区放置 */
    [ARM_POSE_PLACE]      = { 90, 80, 60, 30 },
    /* 码垛姿态 - 在暂存区堆叠 */
    [ARM_POSE_STACK]      = { 90, 90, 80, 30 },
};

/**
 * @brief  舵机模块初始化
 */
void Servo_Init(TIM_HandleTypeDef *htim)
{
    g_htim_servo = htim;

    /* 配置舵机参数 */
    for (int i = 0; i < SERVO_NUM; i++) {
        Servo_t *s = &g_servos[i];
        s->id = (ServoID_t)i;
        s->angle_min = 0;
        s->angle_max = 180;
        s->pulse_min = 500;     /* 0度 → 500us */
        s->pulse_max = 2500;    /* 180度 → 2500us */
        s->current_angle = 90;
        s->current_pulse = 1500; /* 90度 → 1500us */
        s->initialized = true;
    }

    /* 启动所有舵机 PWM 通道 */
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_4);

    /* 设置到空闲姿态 */
    Servo_SetArmPose(ARM_POSE_IDLE);
}

/**
 * @brief  角度转脉宽 (us)
 */
static uint16_t AngleToPulse(const Servo_t *s, uint16_t angle)
{
    uint16_t clamped = angle;
    if (clamped < s->angle_min) clamped = s->angle_min;
    if (clamped > s->angle_max) clamped = s->angle_max;

    /* 线性映射 */
    float ratio = (float)(clamped - s->angle_min) / (float)(s->angle_max - s->angle_min);
    return (uint16_t)(s->pulse_min + ratio * (s->pulse_max - s->pulse_min));
}

/**
 * @brief  脉宽转比较值 (TIM->CCR)
 */
static uint32_t PulseToCompare(uint16_t pulse_us)
{
    /* TIM1 时钟 = 168MHz / (SERVO_PRESCALER+1) = 168M/168 = 1MHz */
    /* 1us = 1 count, 所以 CCR = pulse_us */
    return (uint32_t)pulse_us;
}

/**
 * @brief  设置舵机脉宽
 */
void Servo_SetPulse(ServoID_t id, uint16_t pulse_us)
{
    if (id >= SERVO_NUM || !g_servos[id].initialized) return;

    g_servos[id].current_pulse = pulse_us;
    uint32_t ccr = PulseToCompare(pulse_us);

    switch (id) {
        case SERVO_BASE:     __HAL_TIM_SET_COMPARE(g_htim_servo, TIM_CHANNEL_1, ccr); break;
        case SERVO_SHOULDER: __HAL_TIM_SET_COMPARE(g_htim_servo, TIM_CHANNEL_2, ccr); break;
        case SERVO_ELBOW:    __HAL_TIM_SET_COMPARE(g_htim_servo, TIM_CHANNEL_3, ccr); break;
        case SERVO_GRIPPER:  __HAL_TIM_SET_COMPARE(g_htim_servo, TIM_CHANNEL_4, ccr); break;
        default: break;
    }
}

/**
 * @brief  设置舵机角度
 */
void Servo_SetAngle(ServoID_t id, uint16_t angle)
{
    if (id >= SERVO_NUM || !g_servos[id].initialized) return;

    uint16_t clamped = angle;
    if (clamped < g_servos[id].angle_min) clamped = g_servos[id].angle_min;
    if (clamped > g_servos[id].angle_max) clamped = g_servos[id].angle_max;

    g_servos[id].current_angle = clamped;
    uint16_t pulse = AngleToPulse(&g_servos[id], clamped);
    Servo_SetPulse(id, pulse);
}

/**
 * @brief  获取当前角度
 */
uint16_t Servo_GetAngle(ServoID_t id)
{
    if (id >= SERVO_NUM || !g_servos[id].initialized) return 0;
    return g_servos[id].current_angle;
}

/**
 * @brief  设置手爪开合
 */
void Servo_GripperSet(bool open)
{
    if (open) {
        Servo_SetAngle(SERVO_GRIPPER, 60);  /* 张开 */
    } else {
        Servo_SetAngle(SERVO_GRIPPER, 5);   /* 闭合夹紧 */
    }
}

/**
 * @brief  设置机械臂到预设姿态
 */
void Servo_SetArmPose(ArmPose_t pose)
{
    if (pose >= ARM_POSE_NUM) return;

    for (int i = 0; i < SERVO_NUM; i++) {
        Servo_SetAngle((ServoID_t)i, g_pose_angles[pose][i]);
    }
}

/**
 * @brief  缓慢移动舵机 (带简单插值)
 */
void Servo_SmoothMove(ServoID_t id, uint16_t target_angle, uint16_t step_delay_ms)
{
    if (id >= SERVO_NUM || !g_servos[id].initialized || step_delay_ms == 0) return;

    uint16_t current = g_servos[id].current_angle;
    int16_t diff = (int16_t)target_angle - (int16_t)current;
    int16_t step = (diff > 0) ? 2 : -2;  /* 每步2度 */

    uint16_t steps = (uint16_t)(abs(diff) / 2);
    for (uint16_t i = 0; i < steps; i++) {
        current += step;
        Servo_SetAngle(id, current);
        /* 注意: 在生产代码中应使用 vTaskDelay 替代忙等 */
        for (volatile uint32_t d = 0; d < step_delay_ms * 1000; d++);
    }

    /* 最终精确到位 */
    Servo_SetAngle(id, target_angle);
}
