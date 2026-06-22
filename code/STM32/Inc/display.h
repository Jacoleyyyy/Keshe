/**
 * @file    display.h
 * @brief   OLED SSD1306 SPI 显示接口 (WHEELTEC接口)
 */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "pin_config.h"
#include "protocol.h"

#define OLED_WIDTH          128
#define OLED_HEIGHT         64

typedef struct {
    SystemState_t state;
    uint8_t task_progress;
    TaskCode_t task_code;
    MaterialColor_t current_target;
    uint8_t step_index;
    char status_msg[32];
    uint32_t elapsed_time_s;
    bool is_complete;
} DisplayInfo_t;

void Display_Init(I2C_HandleTypeDef *unused);
void Display_Update(const DisplayInfo_t *info);
void Display_ShowSplash(void);
void Display_ShowError(const char *msg);
void Display_Clear(void);
void Display_PrintString(uint8_t page, uint8_t col, const char *str);
void OLED_Refresh(void);

#endif
