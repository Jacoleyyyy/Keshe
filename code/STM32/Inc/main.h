/**
 * @file    main.h
 * @brief   主头文件 (GMR编码器版)
 */

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "pin_config.h"
#include "FreeRTOSConfig.h"
#include "protocol.h"
#include "pid.h"
#include "motor.h"
#include "servo.h"
#include "chassis.h"
#include "sensor.h"
#include "display.h"
#include "communication.h"
#include "task_manager.h"

/* PWM定时器 */
extern TIM_HandleTypeDef htim1, htim9, htim10, htim11;
/* 编码器定时器 */
extern TIM_HandleTypeDef htim2_enc, htim3_enc, htim4_enc, htim5_enc;
/* 舵机定时器 */
extern TIM_HandleTypeDef htim8;
/* MaixCAM UART */
extern UART_HandleTypeDef huart3;

extern volatile bool g_system_ready;

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_TIM1_Init(void);
void MX_TIM8_Init(void);
void MX_TIM9_Init(void);
void MX_TIM10_Init(void);
void MX_TIM11_Init(void);
void MX_TIM2_Enc_Init(void);
void MX_TIM3_Enc_Init(void);
void MX_TIM4_Enc_Init(void);
void MX_TIM5_Enc_Init(void);
void MX_USART3_Init(void);
void USART3_IRQ_Enable(void);
void Error_Handler(void);

#endif
