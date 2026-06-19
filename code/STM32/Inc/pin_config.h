/**
 * @file    pin_config.h
 * @brief   STM32F407 引脚配置定义
 * @note    针对智能搬运机器人硬件平台
 */

#ifndef __PIN_CONFIG_H
#define __PIN_CONFIG_H

#include "stm32f4xx_hal.h"

/* ============================================================
 * 系统时钟配置
 * ============================================================ */
#define SYSTEM_CLOCK_FREQ       168000000U  // 168 MHz
#define APB1_TIMER_CLOCK        84000000U   // APB1: 84 MHz
#define APB2_TIMER_CLOCK        168000000U  // APB2: 168 MHz

/* ============================================================
 * 麦轮电机引脚定义 (4个带编码器直流电机)
 * ============================================================ */

/* --- 电机1: 左前 (Front-Left) --- */
#define M1_PWM_PORT             GPIOA
#define M1_PWM_PIN              GPIO_PIN_0      // TIM2_CH1
#define M1_DIR_PORT             GPIOC
#define M1_DIR_PIN              GPIO_PIN_0
#define M1_ENC_A_PORT           GPIOA
#define M1_ENC_A_PIN            GPIO_PIN_6      // TIM3_CH1
#define M1_ENC_B_PORT           GPIOA
#define M1_ENC_B_PIN            GPIO_PIN_7      // TIM3_CH2
#define M1_PWM_TIM              TIM2
#define M1_PWM_CHANNEL          TIM_CHANNEL_1
#define M1_ENC_TIM              TIM3

/* --- 电机2: 右前 (Front-Right) --- */
#define M2_PWM_PORT             GPIOA
#define M2_PWM_PIN              GPIO_PIN_1      // TIM2_CH2
#define M2_DIR_PORT             GPIOC
#define M2_DIR_PIN              GPIO_PIN_1
#define M2_ENC_A_PORT           GPIOB
#define M2_ENC_A_PIN            GPIO_PIN_6      // TIM4_CH1
#define M2_ENC_B_PORT           GPIOB
#define M2_ENC_B_PIN            GPIO_PIN_7      // TIM4_CH2
#define M2_PWM_TIM              TIM2
#define M2_PWM_CHANNEL          TIM_CHANNEL_2
#define M2_ENC_TIM              TIM4

/* --- 电机3: 左后 (Rear-Left) --- */
#define M3_PWM_PORT             GPIOA
#define M3_PWM_PIN              GPIO_PIN_2      // TIM2_CH3
#define M3_DIR_PORT             GPIOC
#define M3_DIR_PIN              GPIO_PIN_2
#define M3_ENC_A_PORT           GPIOA
#define M3_ENC_A_PIN            GPIO_PIN_0      // TIM5_CH1
#define M3_ENC_B_PORT           GPIOA
#define M3_ENC_B_PIN            GPIO_PIN_1      // TIM5_CH2
#define M3_PWM_TIM              TIM2
#define M3_PWM_CHANNEL          TIM_CHANNEL_3
#define M3_ENC_TIM              TIM5

/* --- 电机4: 右后 (Rear-Right) --- */
#define M4_PWM_PORT             GPIOA
#define M4_PWM_PIN              GPIO_PIN_3      // TIM2_CH4
#define M4_DIR_PORT             GPIOC
#define M4_DIR_PIN              GPIO_PIN_3
#define M4_ENC_A_PORT           GPIOC
#define M4_ENC_A_PIN            GPIO_PIN_6      // TIM8_CH1
#define M4_ENC_B_PORT           GPIOC
#define M4_ENC_B_PIN            GPIO_PIN_7      // TIM8_CH2
#define M4_PWM_TIM              TIM2
#define M4_PWM_CHANNEL          TIM_CHANNEL_4
#define M4_ENC_TIM              TIM8

/* PWM 参数 */
#define MOTOR_PWM_ARR           8399            // 84MHz / 8400 = 10kHz
#define MOTOR_PWM_MAX           8399
#define MOTOR_PWM_MIN           0

/* 编码器参数 */
#define ENCODER_PPR             11              // 编码器线数
#define ENCODER_CPR             (ENCODER_PPR * 4)  // 4倍频
#define GEAR_RATIO              30.0f           // 减速比 30:1
#define WHEEL_RADIUS_MM         32.5f           // 轮子半径 (mm)
#define PULSE_PER_REV           (uint32_t)(ENCODER_CPR * GEAR_RATIO)

/* ============================================================
 * 舵机引脚定义 (机械臂)
 * ============================================================ */

/* 4路舵机: TIM1 4通道 */
#define SERVO1_PORT             GPIOE
#define SERVO1_PIN              GPIO_PIN_9      // TIM1_CH1 - 腰部旋转
#define SERVO2_PORT             GPIOE
#define SERVO2_PIN              GPIO_PIN_11     // TIM1_CH2 - 大臂
#define SERVO3_PORT             GPIOE
#define SERVO3_PIN              GPIO_PIN_13     // TIM1_CH3 - 小臂
#define SERVO4_PORT             GPIOE
#define SERVO4_PIN              GPIO_PIN_14     // TIM1_CH4 - 手爪
#define SERVO_TIM               TIM1
#define SERVO_TIM_ARR           19999           // 168MHz/20000 = 8400? 50Hz需要20000
/* 校正: 168MHz / (prescaler 84-1) / (period 20000-1) = 168M/84/20000 = 100Hz
   改为 prescaler=168-1, period=20000-1 → 168M/168/20000 = 50Hz */
#define SERVO_PRESCALER         167             // 168-1
#define SERVO_PERIOD            19999           // 20000-1

/* 舵机角度限位 */
#define SERVO1_ANGLE_MIN        0
#define SERVO1_ANGLE_MAX        180
#define SERVO2_ANGLE_MIN        0
#define SERVO2_ANGLE_MAX        120
#define SERVO3_ANGLE_MIN        0
#define SERVO3_ANGLE_MAX        120
#define SERVO4_ANGLE_MIN        0       // 手爪闭合
#define SERVO4_ANGLE_MAX        60      // 手爪张开

/* ============================================================
 * 传感器引脚定义
 * ============================================================ */

/* 超声波避障传感器 */
#define US_TRIG_PORT            GPIOB
#define US_TRIG_PIN             GPIO_PIN_0
#define US_ECHO_PORT            GPIOB
#define US_ECHO_PIN             GPIO_PIN_1

/* 灰度传感器 (5路, 用于车道线检测) */
#define GRAY1_PORT              GPIOD
#define GRAY1_PIN               GPIO_PIN_8
#define GRAY2_PORT              GPIOD
#define GRAY2_PIN               GPIO_PIN_9
#define GRAY3_PORT              GPIOD
#define GRAY3_PIN               GPIO_PIN_10
#define GRAY4_PORT              GPIOD
#define GRAY4_PIN               GPIO_PIN_11
#define GRAY5_PORT              GPIOD
#define GRAY5_PIN               GPIO_PIN_12

/* 启动按钮 */
#define START_BTN_PORT          GPIOE
#define START_BTN_PIN           GPIO_PIN_0

/* 蜂鸣器 */
#define BUZZER_PORT             GPIOB
#define BUZZER_PIN              GPIO_PIN_8

/* ============================================================
 * 显示模块引脚定义 (OLED SSD1306, I2C)
 * ============================================================ */
#define OLED_I2C                I2C1
#define OLED_I2C_SCL_PORT       GPIOB
#define OLED_I2C_SCL_PIN        GPIO_PIN_8
#define OLED_I2C_SDA_PORT       GPIOB
#define OLED_I2C_SDA_PIN        GPIO_PIN_9
#define OLED_ADDR               (0x3C << 1)

/* ============================================================
 * 通信引脚定义 (UART 与 MaixCAM 通信)
 * ============================================================ */
#define MAIX_UART               USART3
#define MAIX_UART_TX_PORT       GPIOB
#define MAIX_UART_TX_PIN        GPIO_PIN_10
#define MAIX_UART_RX_PORT       GPIOB
#define MAIX_UART_RX_PIN        GPIO_PIN_11
#define MAIX_UART_BAUDRATE      115200

/* ============================================================
 * 系统 LED 指示灯
 * ============================================================ */
#define SYS_LED_PORT            GPIOE
#define SYS_LED_PIN             GPIO_PIN_1

#endif /* __PIN_CONFIG_H */
