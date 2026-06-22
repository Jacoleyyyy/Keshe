/**
 * @file    servo.c
 * @brief   舵机控制 (GMR版: TIM8 PC6-PC9, 100Hz)
 *
 * TIM8: 168MHz / 168 = 1MHz → /10000 = 100Hz
 * 脉宽范围: 500us(0°) ~ 2500us(180°), 对应CCR 500~2500
 */

#include "servo.h"
#include <string.h>

static TIM_HandleTypeDef *g_htim = NULL;

/* 预设姿态角度表 [BASE, SHOULDER, ELBOW, GRIPPER] */
static const uint16_t g_poses[ARM_POSE_NUM][SERVO_NUM] = {
    [ARM_POSE_IDLE]       = { 90, 20, 10,  5 },
    [ARM_POSE_SCAN]       = { 90, 60, 30,  5 },
    [ARM_POSE_PRE_PICK]   = { 90, 80, 60, 60 },
    [ARM_POSE_PICK]       = { 90, 85, 65,  5 },
    [ARM_POSE_CARRY]      = { 90, 50, 40,  5 },
    [ARM_POSE_PLACE]      = { 90, 80, 60, 30 },
    [ARM_POSE_STACK]      = { 90, 90, 80, 30 },
};

/* 当前角度 */
static uint16_t g_angles[SERVO_NUM] = {90, 20, 10, 5};

/* 角度→脉宽映射: 0°=500us, 180°=2500us, 1°≈11.1us */
static uint16_t AngleToPulse(uint16_t angle)
{
    if (angle > 180) angle = 180;
    return 500 + (uint16_t)((uint32_t)angle * 2000 / 180);
}

void Servo_Init(TIM_HandleTypeDef *htim)
{
    g_htim = htim;
    TIM_ChannelConfTypeDef sConfig = {0};
    sConfig.OCMode = TIM_OCMODE_PWM1;
    sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode = TIM_OCFAST_DISABLE;

    /* 4通道, 起始脉宽=中位1500us */
    uint16_t init_pulse[] = {1500, AngleToPulse(20), AngleToPulse(10), 500};
    uint32_t channels[] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};

    for (int i = 0; i < 4; i++) {
        sConfig.Pulse = init_pulse[i];
        HAL_TIM_PWM_ConfigChannel(htim, &sConfig, channels[i]);
        HAL_TIM_PWM_Start(htim, channels[i]);
    }
}

void Servo_SetAngle(ServoID_t id, uint16_t angle)
{
    if (id >= SERVO_NUM || !g_htim) return;
    if (angle > 180) angle = 180;
    g_angles[id] = angle;

    uint16_t pulse = AngleToPulse(angle);
    uint32_t ch[] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};
    __HAL_TIM_SET_COMPARE(g_htim, ch[id], pulse);
}

void Servo_SetPulse(ServoID_t id, uint16_t pulse_us)
{
    if (id >= SERVO_NUM || !g_htim) return;
    uint32_t ch[] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};
    __HAL_TIM_SET_COMPARE(g_htim, ch[id], pulse_us);
}

uint16_t Servo_GetAngle(ServoID_t id)
{
    return (id < SERVO_NUM) ? g_angles[id] : 0;
}

void Servo_GripperSet(bool open)
{
    Servo_SetAngle(SERVO_GRIPPER, open ? 60 : 5);
}

void Servo_SetArmPose(ArmPose_t pose)
{
    if (pose >= ARM_POSE_NUM) return;
    for (int i = 0; i < SERVO_NUM; i++)
        Servo_SetAngle((ServoID_t)i, g_poses[pose][i]);
}

void Servo_SmoothMove(ServoID_t id, uint16_t target, uint16_t delay_ms)
{
    int16_t cur = (int16_t)Servo_GetAngle(id);
    int16_t diff = (int16_t)target - cur;
    uint16_t steps = (uint16_t)(abs(diff) / 2);
    for (uint16_t i = 0; i < steps; i++) {
        cur += (diff > 0) ? 2 : -2;
        Servo_SetAngle(id, (uint16_t)cur);
        for (volatile uint32_t d = 0; d < delay_ms * 1000; d++);
    }
    Servo_SetAngle(id, target);
}
