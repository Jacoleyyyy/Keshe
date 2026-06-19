/**
 * @file    display.h
 * @brief   OLED 显示模块 (SSD1306, I2C)
 * @note    显示当前任务状态、进度、调试信息
 */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "pin_config.h"
#include "protocol.h"

/* OLED 分辨率 */
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_PAGES          (OLED_HEIGHT / 8)

/* 显示内容结构 */
typedef struct {
    SystemState_t state;            /* 当前状态 */
    uint8_t task_progress;          /* 任务进度 0~6 (6步) */
    TaskCode_t task_code;           /* 任务编码 */
    MaterialColor_t current_target; /* 当前目标颜色 */
    uint8_t step_index;             /* 当前步骤 (0~5) */
    char status_msg[32];            /* 状态消息 */
    uint32_t elapsed_time_s;        /* 运行时间 (秒) */
    bool is_complete;               /* 是否完成 */
} DisplayInfo_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  OLED 显示模块初始化
 */
void Display_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  更新显示内容
 */
void Display_Update(const DisplayInfo_t *info);

/**
 * @brief  显示启动画面
 */
void Display_ShowSplash(void);

/**
 * @brief  显示错误信息
 */
void Display_ShowError(const char *msg);

/**
 * @brief  清屏
 */
void Display_Clear(void);

/**
 * @brief  在指定位置显示字符串 (用于调试)
 */
void Display_PrintString(uint8_t page, uint8_t col, const char *str);

/* ============================================================
 * 底层操作 (SSD1306 命令)
 * ============================================================ */
void OLED_WriteCmd(uint8_t cmd);
void OLED_WriteData(uint8_t data);
void OLED_SetCursor(uint8_t page, uint8_t col);
void OLED_Refresh(void);

#endif /* __DISPLAY_H */
