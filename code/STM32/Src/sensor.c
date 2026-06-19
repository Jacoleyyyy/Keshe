/**
 * @file    sensor.c
 * @brief   传感器模块实现
 */

#include "sensor.h"

/* 超声波状态 */
static volatile uint32_t g_us_pulse_start = 0;
static volatile uint32_t g_us_pulse_width = 0;
static volatile bool g_us_data_ready = false;

/**
 * @brief  传感器模块初始化
 */
void Sensor_Init(void)
{
    /* 灰度传感器引脚: 上拉输入 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = GRAY1_PIN | GRAY2_PIN | GRAY3_PIN | GRAY4_PIN | GRAY5_PIN;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* 超声波触发引脚: 推挽输出 */
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = US_TRIG_PIN;
    HAL_GPIO_Init(US_TRIG_PORT, &gpio);
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);

    /* 超声波回波引脚: 输入 */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = US_ECHO_PIN;
    HAL_GPIO_Init(US_ECHO_PORT, &gpio);

    /* 启动按钮: 上拉输入 */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = START_BTN_PIN;
    HAL_GPIO_Init(START_BTN_PORT, &gpio);

    /* 蜂鸣器: 推挽输出 */
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  读取5路灰度传感器
 */
void Sensor_ReadGray(GraySensor_t *gray)
{
    if (!gray) return;

    gray->raw_value[0] = (HAL_GPIO_ReadPin(GRAY1_PORT, GRAY1_PIN) == GPIO_PIN_SET) ? 1 : 0;
    gray->raw_value[1] = (HAL_GPIO_ReadPin(GRAY2_PORT, GRAY2_PIN) == GPIO_PIN_SET) ? 1 : 0;
    gray->raw_value[2] = (HAL_GPIO_ReadPin(GRAY3_PORT, GRAY3_PIN) == GPIO_PIN_SET) ? 1 : 0;
    gray->raw_value[3] = (HAL_GPIO_ReadPin(GRAY4_PORT, GRAY4_PIN) == GPIO_PIN_SET) ? 1 : 0;
    gray->raw_value[4] = (HAL_GPIO_ReadPin(GRAY5_PORT, GRAY5_PIN) == GPIO_PIN_SET) ? 1 : 0;

    /* 计算线位置: 0=全部在左, 50=正中, 100=全部在右 */
    uint8_t black_count = 0;
    int32_t weighted_sum = 0;
    for (int i = 0; i < 5; i++) {
        if (gray->raw_value[i] == 0) {  /* 0=黑线(灰色车道) */
            black_count++;
            weighted_sum += i;
        }
    }

    if (black_count > 0) {
        gray->line_position = (uint8_t)(weighted_sum * 100 / (black_count * 4));
        gray->on_line = true;
    } else {
        gray->line_position = 50; /* 默认中间 */
        gray->on_line = false;
    }

    /* 交叉口检测: 3个以上传感器检测到黑线 */
    gray->at_intersection = (black_count >= 3);
}

/**
 * @brief  启动超声波测量
 */
void Sensor_TriggerUltrasonic(void)
{
    /* 发送 10us 高脉冲 */
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_SET);
    /* 延时约 15us (168MHz下 ~2500 cycles) */
    for (volatile uint32_t i = 0; i < 50; i++) { __NOP(); }
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  超声波 Echo 中断回调
 * @param  pulse_width_us  脉宽 (us)
 */
void Sensor_UltrasonicEchoCallback(uint32_t pulse_width_us)
{
    g_us_pulse_width = pulse_width_us;
    g_us_data_ready = true;
}

/**
 * @brief  获取最新超声波数据
 */
Ultrasonic_t Sensor_GetUltrasonicData(void)
{
    Ultrasonic_t us = {0};
    if (g_us_data_ready) {
        us.distance_mm = (float)g_us_pulse_width * 0.17f;  /* 声速340m/s → 0.17mm/us /2 */
        us.obstacle_detected = (us.distance_mm < OBSTACLE_DISTANCE_MM);
        us.valid = true;
        g_us_data_ready = false;
    }
    return us;
}

/**
 * @brief  检测启动按钮
 */
bool Sensor_IsStartButtonPressed(void)
{
    return (HAL_GPIO_ReadPin(START_BTN_PORT, START_BTN_PIN) == GPIO_PIN_RESET);
}

/**
 * @brief  检查是否在灰色车道上
 */
bool Sensor_IsOnLane(const GraySensor_t *gray)
{
    if (!gray) return false;
    return gray->on_line;
}

/**
 * @brief  车道中心偏移量
 */
int8_t Sensor_GetLaneOffset(const GraySensor_t *gray)
{
    if (!gray) return 0;
    /* 0=中间, -50=偏左, +50=偏右, -100=掉线 */
    if (!gray->on_line) return -100;
    return (int8_t)(50 - gray->line_position);
}

/**
 * @brief  蜂鸣器控制
 */
void Sensor_BuzzerOn(void)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

void Sensor_BuzzerOff(void)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void Sensor_BuzzerBeep(uint16_t duration_ms)
{
    Sensor_BuzzerOn();
    HAL_Delay(duration_ms);
    Sensor_BuzzerOff();
}
