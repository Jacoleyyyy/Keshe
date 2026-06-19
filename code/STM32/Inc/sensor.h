/**
 * @file    sensor.h
 * @brief   传感器模块 (超声波避障 + 灰度巡线 + 按钮)
 */

#ifndef __SENSOR_H
#define __SENSOR_H

#include "pin_config.h"

/* 灰度传感器结构 */
typedef struct {
    uint8_t raw_value[5];     /* 5路灰度原始值 (0=黑色, 1=白色) */
    uint8_t line_position;    /* 线位置估计 (0~100, 50=中间) */
    bool on_line;             /* 是否在车道上 */
    bool at_intersection;     /* 是否在交叉口 */
} GraySensor_t;

/* 超声波传感器结构 */
typedef struct {
    float distance_mm;        /* 距离 (mm) */
    bool obstacle_detected;   /* 是否检测到障碍物 */
    bool valid;               /* 数据是否有效 */
} Ultrasonic_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  传感器模块初始化
 */
void Sensor_Init(void);

/**
 * @brief  读取5路灰度传感器
 */
void Sensor_ReadGray(GraySensor_t *gray);

/**
 * @brief  测量超声波距离 (阻塞式)
 */
void Sensor_ReadUltrasonic(Ultrasonic_t *us);

/**
 * @brief  启动超声波非阻塞测量 (由定时器中断完成读取)
 */
void Sensor_TriggerUltrasonic(void);

/**
 * @brief  超声波Echo中断回调
 */
void Sensor_UltrasonicEchoCallback(uint32_t pulse_width_us);

/**
 * @brief  获取最新超声波数据
 */
Ultrasonic_t Sensor_GetUltrasonicData(void);

/**
 * @brief  检测启动按钮是否按下
 */
bool Sensor_IsStartButtonPressed(void);

/**
 * @brief  检查是否在灰色车道上 (基于灰度传感器)
 */
bool Sensor_IsOnLane(const GraySensor_t *gray);

/**
 * @brief  车道中心偏移量 (-100~100, 0=正中)
 */
int8_t Sensor_GetLaneOffset(const GraySensor_t *gray);

/**
 * @brief  蜂鸣器控制
 */
void Sensor_BuzzerOn(void);
void Sensor_BuzzerOff(void);
void Sensor_BuzzerBeep(uint16_t duration_ms);

#endif /* __SENSOR_H */
