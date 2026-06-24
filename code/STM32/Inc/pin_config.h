/**
 * @file    pin_config.h
 * @brief   STM32F407 引脚配置 — 适配 WHEELTEC C30D 2.0 GMR编码器版本
 * @note    基于轮趣科技 R550A C30D 2.0 硬件PCB布线
 */

#ifndef __PIN_CONFIG_H
#define __PIN_CONFIG_H

#include "stm32f4xx_hal.h"

/* ============================================================
 * 系统时钟
 * ============================================================ */
#define SYSTEM_CLOCK_FREQ       168000000U
#define APB1_TIMER_CLOCK        84000000U
#define APB2_TIMER_CLOCK        168000000U

/* ============================================================
 * 麦轮电机 PWM 引脚 (4个电机, 每个2路PWM = 8通道)
 *
 * WHEELTEC 用双通道PWM控制每个电机的正反转:
 *   - CH1 输出 → 电机正转
 *   - CH2 输出 → 电机反转
 *   同时只有一个通道有输出
 * ============================================================ */

/* --- 电机A (对应WHEELTEC编码器D, TIM5) --- */
#define MA_PWM1_PORT            GPIOB
#define MA_PWM1_PIN             GPIO_PIN_8      // TIM10_CH1
#define MA_PWM2_PORT            GPIOB
#define MA_PWM2_PIN             GPIO_PIN_9      // TIM11_CH1
#define MA_PWM1_TIM             TIM10
#define MA_PWM1_CHANNEL         TIM_CHANNEL_1
#define MA_PWM2_TIM             TIM11
#define MA_PWM2_CHANNEL         TIM_CHANNEL_1

/* --- 电机B (对应WHEELTEC编码器C, TIM4) --- */
#define MB_PWM1_PORT            GPIOE
#define MB_PWM1_PIN             GPIO_PIN_5      // TIM9_CH1
#define MB_PWM2_PORT            GPIOE
#define MB_PWM2_PIN             GPIO_PIN_6      // TIM9_CH2
#define MB_PWM1_TIM             TIM9
#define MB_PWM1_CHANNEL         TIM_CHANNEL_1
#define MB_PWM2_TIM             TIM9
#define MB_PWM2_CHANNEL         TIM_CHANNEL_2

/* --- 电机C (对应WHEELTEC编码器A, TIM2) --- */
#define MC_PWM1_PORT            GPIOE
#define MC_PWM1_PIN             GPIO_PIN_11     // TIM1_CH2
#define MC_PWM2_PORT            GPIOE
#define MC_PWM2_PIN             GPIO_PIN_9      // TIM1_CH1
#define MC_PWM1_TIM             TIM1
#define MC_PWM1_CHANNEL         TIM_CHANNEL_2
#define MC_PWM2_TIM             TIM1
#define MC_PWM2_CHANNEL         TIM_CHANNEL_1

/* --- 电机D (对应WHEELTEC编码器B, TIM3) --- */
#define MD_PWM1_PORT            GPIOE
#define MD_PWM1_PIN             GPIO_PIN_14     // TIM1_CH4
#define MD_PWM2_PORT            GPIOE
#define MD_PWM2_PIN             GPIO_PIN_13     // TIM1_CH3
#define MD_PWM1_TIM             TIM1
#define MD_PWM1_CHANNEL         TIM_CHANNEL_4
#define MD_PWM2_TIM             TIM1
#define MD_PWM2_CHANNEL         TIM_CHANNEL_3

/* PWM 参数: 168MHz / (0+1) = 168MHz → /(16799+1) = 10kHz */
#define MOTOR_PWM_ARR           16799
#define MOTOR_PWM_MAX           16799

/* ============================================================
 * 编码器引脚 (4个GMR编码器, 500线)
 *
 * WHEELTEC映射:
 *   编码器A (TIM2) → 电机C
 *   编码器B (TIM3) → 电机D
 *   编码器C (TIM4) → 电机B
 *   编码器D (TIM5) → 电机A
 * ============================================================ */

/* 编码器A → 电机C */
#define ENC_A_TIM               TIM2
#define ENC_A_PORT_A            GPIOA
#define ENC_A_PIN_A             GPIO_PIN_15     // TIM2_CH1
#define ENC_A_PORT_B            GPIOB
#define ENC_A_PIN_B             GPIO_PIN_3      // TIM2_CH2

/* 编码器B → 电机D */
#define ENC_B_TIM               TIM3
#define ENC_B_PORT_A            GPIOB
#define ENC_B_PIN_A             GPIO_PIN_4      // TIM3_CH1
#define ENC_B_PORT_B            GPIOB
#define ENC_B_PIN_B             GPIO_PIN_5      // TIM3_CH2

/* 编码器C → 电机B */
#define ENC_C_TIM               TIM4
#define ENC_C_PORT_A            GPIOB
#define ENC_C_PIN_A             GPIO_PIN_6      // TIM4_CH1
#define ENC_C_PORT_B            GPIOB
#define ENC_C_PIN_B             GPIO_PIN_7      // TIM4_CH2

/* 编码器D → 电机A */
#define ENC_D_TIM               TIM5
#define ENC_D_PORT_A            GPIOA
#define ENC_D_PIN_A             GPIO_PIN_0      // TIM5_CH1
#define ENC_D_PORT_B            GPIOA
#define ENC_D_PIN_B             GPIO_PIN_1      // TIM5_CH2

/* 编码器参数: 500线 × 4倍频 × 60减速比 = 120,000 脉冲/轮转 */
#define ENCODER_PPR             500
#define ENCODER_CPR             (ENCODER_PPR * 4)
#define GEAR_RATIO              60.0f
#define ENCODER_PRECISION       (uint32_t)(ENCODER_CPR * GEAR_RATIO)  // 120,000

/* ============================================================
 * 舵机引脚 (机械臂4自由度)
 *
 * WHEELTEC: TIM8 CH1~CH4 → PC6~PC9, 100Hz
 * 168MHz / 168 = 1MHz → /10000 = 100Hz
 * ============================================================ */
#define SERVO_TIM               TIM8
#define SERVO_PORT              GPIOC
#define SERVO1_PIN              GPIO_PIN_6      // TIM8_CH1 - 腰部
#define SERVO2_PIN              GPIO_PIN_7      // TIM8_CH2 - 大臂
#define SERVO3_PIN              GPIO_PIN_8      // TIM8_CH3 - 小臂
#define SERVO4_PIN              GPIO_PIN_9      // TIM8_CH4 - 手爪
#define SERVO_CH1               TIM_CHANNEL_1
#define SERVO_CH2               TIM_CHANNEL_2
#define SERVO_CH3               TIM_CHANNEL_3
#define SERVO_CH4               TIM_CHANNEL_4
#define SERVO_PSC               167             // 168-1
#define SERVO_ARR               9999            // 10000-1

/* ============================================================
 * 电机使能开关 (WHEELTEC 硬件)
 * ============================================================ */
#define EN_PORT                 GPIOD
#define EN_PIN                  GPIO_PIN_3

/* ============================================================
 * OLED 显示屏 (WHEELTEC SPI 接口, 非I2C!)
 * ============================================================ */
#define OLED_SPI_DC_PORT        GPIOD
#define OLED_SPI_DC_PIN         GPIO_PIN_11
#define OLED_SPI_RST_PORT       GPIOD
#define OLED_SPI_RST_PIN        GPIO_PIN_12
#define OLED_SPI_SDA_PORT       GPIOD
#define OLED_SPI_SDA_PIN        GPIO_PIN_13
#define OLED_SPI_SCL_PORT       GPIOD
#define OLED_SPI_SCL_PIN        GPIO_PIN_14

/* ============================================================
 * 通信引脚 — UART3 与 MaixCAM 通信
 * PD8(TX)/PD9(RX), 115200bps
 * (C30D-1.0 出厂默认 UART3 引脚)
 * ============================================================ */
#define MAIX_UART               USART3
#define MAIX_UART_TX_PORT       GPIOD
#define MAIX_UART_TX_PIN        GPIO_PIN_8
#define MAIX_UART_RX_PORT       GPIOD
#define MAIX_UART_RX_PIN        GPIO_PIN_9
#define MAIX_UART_BAUDRATE      115200

/* ============================================================
 * 传感器引脚
 * ============================================================ */

/* 灰度巡线传感器 (5路) — 使用自由引脚 */
#define GRAY1_PORT              GPIOD
#define GRAY1_PIN               GPIO_PIN_8
#define GRAY2_PORT              GPIOD
#define GRAY2_PIN               GPIO_PIN_9
#define GRAY3_PORT              GPIOD
#define GRAY3_PIN               GPIO_PIN_10
#define GRAY4_PORT              GPIOD
#define GRAY4_PIN               GPIO_PIN_15
#define GRAY5_PORT              GPIOE
#define GRAY5_PIN               GPIO_PIN_0

/* 超声波避障 */
#define US_TRIG_PORT            GPIOB
#define US_TRIG_PIN             GPIO_PIN_0
#define US_ECHO_PORT            GPIOB
#define US_ECHO_PIN             GPIO_PIN_1

/* 启动按钮 */
#define START_BTN_PORT          GPIOE
#define START_BTN_PIN           GPIO_PIN_1

/* 蜂鸣器 */
#define BUZZER_PORT             GPIOA
#define BUZZER_PIN              GPIO_PIN_8

/* 系统LED (C30D-1.0 V1.0板 = PA12) */
#define SYS_LED_PORT            GPIOA
#define SYS_LED_PIN             GPIO_PIN_12

#endif /* __PIN_CONFIG_H */
