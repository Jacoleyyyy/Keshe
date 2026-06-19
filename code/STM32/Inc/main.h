/**
 * @file    main.h
 * @brief   智能搬运机器人 - 主头文件
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

/* 硬件配置头文件 */
#include "pin_config.h"
#include "FreeRTOSConfig.h"

/* 协议定义 */
#include "protocol.h"

/* 模块头文件 */
#include "pid.h"
#include "motor.h"
#include "servo.h"
#include "chassis.h"
#include "sensor.h"
#include "display.h"
#include "communication.h"
#include "task_manager.h"

/* ============================================================
 * 全局句柄 (由 main.c 定义，各模块引用)
 * ============================================================ */
extern TIM_HandleTypeDef   htim_motor_pwm;    /* 电机 PWM 定时器 */
extern TIM_HandleTypeDef   htim_m1_enc;       /* 电机1 编码器 */
extern TIM_HandleTypeDef   htim_m2_enc;       /* 电机2 编码器 */
extern TIM_HandleTypeDef   htim_m3_enc;       /* 电机3 编码器 */
extern TIM_HandleTypeDef   htim_m4_enc;       /* 电机4 编码器 */
extern TIM_HandleTypeDef   htim_servo;        /* 舵机定时器 */
extern UART_HandleTypeDef  huart_maix;        /* MaixCAM UART */
extern I2C_HandleTypeDef   hi2c_oled;         /* OLED I2C */

/* 系统就绪标志 */
extern volatile bool g_system_ready;

/* ============================================================
 * 全局函数声明
 * ============================================================ */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_TIM_Init(void);
void MX_USART_Init(void);
void MX_I2C_Init(void);
void Error_Handler(void);

#endif /* __MAIN_H */
