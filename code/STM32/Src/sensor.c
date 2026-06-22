/**
 * @file    sensor.c
 * @brief   传感器模块 (GMR版: 更新引脚)
 */

#include "sensor.h"

static volatile uint32_t g_us_pulse_width = 0;
static volatile bool g_us_ready = false;

void Sensor_Init(void)
{
    GPIO_InitTypeDef g = {0};

    /* 灰度传感器 5路: PD8/9/10/15, PE0 */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pin = GRAY1_PIN | GRAY2_PIN | GRAY3_PIN | GRAY4_PIN;
    HAL_GPIO_Init(GPIOD, &g);
    g.Pin = GRAY5_PIN;
    HAL_GPIO_Init(GPIOE, &g);

    /* 超声波 TRIG: PB0, 推挽输出 */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Pin = US_TRIG_PIN;
    HAL_GPIO_Init(US_TRIG_PORT, &g);
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);

    /* 超声波 ECHO: PB1, 输入 */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    g.Pin = US_ECHO_PIN;
    HAL_GPIO_Init(US_ECHO_PORT, &g);

    /* 启动按钮: PE1, 上拉输入 (按下=低电平) */
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Pin = START_BTN_PIN;
    HAL_GPIO_Init(START_BTN_PORT, &g);

    /* 蜂鸣器: PA8, 推挽输出 */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Pin = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_PORT, &g);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void Sensor_ReadGray(GraySensor_t *gray)
{
    if (!gray) return;

    gray->raw_value[0] = (HAL_GPIO_ReadPin(GRAY1_PORT, GRAY1_PIN) == GPIO_PIN_RESET) ? 0 : 1;
    gray->raw_value[1] = (HAL_GPIO_ReadPin(GRAY2_PORT, GRAY2_PIN) == GPIO_PIN_RESET) ? 0 : 1;
    gray->raw_value[2] = (HAL_GPIO_ReadPin(GRAY3_PORT, GRAY3_PIN) == GPIO_PIN_RESET) ? 0 : 1;
    gray->raw_value[3] = (HAL_GPIO_ReadPin(GRAY4_PORT, GRAY4_PIN) == GPIO_PIN_RESET) ? 0 : 1;
    gray->raw_value[4] = (HAL_GPIO_ReadPin(GRAY5_PORT, GRAY5_PIN) == GPIO_PIN_RESET) ? 0 : 1;

    /* 计算中心位置: 0=全右, 50=正中, 100=全左 */
    uint8_t black = 0;
    int32_t sum = 0;
    for (int i = 0; i < 5; i++) {
        if (gray->raw_value[i] == 0) { black++; sum += i; }
    }
    if (black > 0) {
        gray->line_position = (uint8_t)(sum * 100 / (black * 4));
        gray->on_line = true;
    } else {
        gray->line_position = 50;
        gray->on_line = false;
    }
    gray->at_intersection = (black >= 3);
}

void Sensor_TriggerUltrasonic(void)
{
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_SET);
    for (volatile uint32_t i = 0; i < 50; i++) __NOP();
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);
}

void Sensor_UltrasonicEchoCallback(uint32_t pulse_us)
{
    g_us_pulse_width = pulse_us;
    g_us_ready = true;
}

Ultrasonic_t Sensor_GetUltrasonicData(void)
{
    Ultrasonic_t us = {0};
    if (g_us_ready) {
        us.distance_mm = (float)g_us_pulse_width * 0.17f;
        us.obstacle_detected = (us.distance_mm < OBSTACLE_DISTANCE_MM && us.distance_mm > 0);
        us.valid = true;
        g_us_ready = false;
    }
    return us;
}

bool Sensor_IsStartButtonPressed(void)
{
    return (HAL_GPIO_ReadPin(START_BTN_PORT, START_BTN_PIN) == GPIO_PIN_RESET);
}

bool Sensor_IsOnLane(const GraySensor_t *g)
{
    return g ? g->on_line : false;
}

int8_t Sensor_GetLaneOffset(const GraySensor_t *g)
{
    if (!g || !g->on_line) return -100;
    return (int8_t)(50 - g->line_position);
}

void Sensor_BuzzerOn(void)  { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET); }
void Sensor_BuzzerOff(void) { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); }
void Sensor_BuzzerBeep(uint16_t ms)
{
    Sensor_BuzzerOn();
    HAL_Delay(ms);
    Sensor_BuzzerOff();
}
