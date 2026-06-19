/**
 * @file    display.c
 * @brief   OLED SSD1306 显示模块实现 (I2C)
 */

#include "display.h"
#include <string.h>
#include <stdio.h>

static I2C_HandleTypeDef *g_hi2c_oled = NULL;
static uint8_t g_oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8]; /* 128x64/8 = 1024 bytes */

/* 6x8 ASCII 字体 (部分, 可打印字符 0x20-0x7F) */
static const uint8_t FONT_6X8[][6] = {
    /* 空格 ~ ! " # $ % & ' ( ) * + , - . / */
    /* 为了简洁，这里仅包含数字和大写字母的部分字体。
       完整字体需从标准字库引入。这里使用简化版本。 */
};

/* SSD1306 命令 */
#define SSD1306_CMD_DISPLAY_OFF     0xAE
#define SSD1306_CMD_DISPLAY_ON      0xAF
#define SSD1306_CMD_SET_MUX_RATIO   0xA8
#define SSD1306_CMD_SET_DISP_OFFSET 0xD3
#define SSD1306_CMD_SET_START_LINE  0x40
#define SSD1306_CMD_SEG_REMAP       0xA1
#define SSD1306_CMD_COM_SCAN_DEC    0xC8
#define SSD1306_CMD_COM_PINS        0xDA
#define SSD1306_CMD_SET_CONTRAST    0x81
#define SSD1306_CMD_DISPLAY_ALL_ON  0xA5
#define SSD1306_CMD_NORMAL_DISPLAY  0xA6
#define SSD1306_CMD_INVERT_DISPLAY  0xA7
#define SSD1306_CMD_SET_OSC_FREQ    0xD5
#define SSD1306_CMD_SET_PRECHARGE   0xD9
#define SSD1306_CMD_SET_VCOM_DETECT 0xDB
#define SSD1306_CMD_MEMORY_MODE     0x20
#define SSD1306_CMD_COL_ADDR        0x21
#define SSD1306_CMD_PAGE_ADDR       0x22
#define SSD1306_CMD_CHARGE_PUMP     0x8D

/**
 * @brief  发送命令到 OLED
 */
void OLED_WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };  /* 0x00 = 命令模式 */
    HAL_I2C_Master_Transmit(g_hi2c_oled, OLED_ADDR, buf, 2, 100);
}

/**
 * @brief  发送数据到 OLED
 */
void OLED_WriteData(uint8_t data)
{
    uint8_t buf[2] = { 0x40, data };  /* 0x40 = 数据模式 */
    HAL_I2C_Master_Transmit(g_hi2c_oled, OLED_ADDR, buf, 2, 100);
}

/**
 * @brief  OLED 初始化
 */
void Display_Init(I2C_HandleTypeDef *hi2c)
{
    g_hi2c_oled = hi2c;
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));

    /* SSD1306 初始化序列 */
    OLED_WriteCmd(SSD1306_CMD_DISPLAY_OFF);

    OLED_WriteCmd(SSD1306_CMD_SET_MUX_RATIO);
    OLED_WriteCmd(0x3F);  /* 64行 */

    OLED_WriteCmd(SSD1306_CMD_SET_DISP_OFFSET);
    OLED_WriteCmd(0x00);

    OLED_WriteCmd(SSD1306_CMD_SET_START_LINE);

    OLED_WriteCmd(SSD1306_CMD_SEG_REMAP);       /* 列重映射 */
    OLED_WriteCmd(SSD1306_CMD_COM_SCAN_DEC);    /* COM扫描方向 */

    OLED_WriteCmd(SSD1306_CMD_COM_PINS);
    OLED_WriteCmd(0x12);

    OLED_WriteCmd(SSD1306_CMD_SET_CONTRAST);
    OLED_WriteCmd(0x7F);

    OLED_WriteCmd(SSD1306_CMD_DISPLAY_ALL_ON);
    OLED_WriteCmd(SSD1306_CMD_NORMAL_DISPLAY);

    OLED_WriteCmd(SSD1306_CMD_SET_OSC_FREQ);
    OLED_WriteCmd(0x80);

    OLED_WriteCmd(SSD1306_CMD_MEMORY_MODE);
    OLED_WriteCmd(0x00);  /* 水平寻址模式 */

    /* 电荷泵 */
    OLED_WriteCmd(SSD1306_CMD_CHARGE_PUMP);
    OLED_WriteCmd(0x14);  /* 使能 */

    OLED_WriteCmd(SSD1306_CMD_DISPLAY_ON);
}

/**
 * @brief  清屏
 */
void Display_Clear(void)
{
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
    OLED_Refresh();
}

/**
 * @brief  刷新显示 (将缓冲区写入 OLED)
 */
void OLED_Refresh(void)
{
    /* 设置列地址 */
    OLED_WriteCmd(SSD1306_CMD_COL_ADDR);
    OLED_WriteCmd(0);
    OLED_WriteCmd(OLED_WIDTH - 1);

    /* 设置页地址 */
    OLED_WriteCmd(SSD1306_CMD_PAGE_ADDR);
    OLED_WriteCmd(0);
    OLED_WriteCmd(OLED_PAGES - 1);

    /* 批量写入缓冲区 */
    for (uint16_t i = 0; i < sizeof(g_oled_buffer); i++) {
        OLED_WriteData(g_oled_buffer[i]);
    }
}

/**
 * @brief  在缓冲区中绘制一个像素
 */
static void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    uint16_t idx = x + (y / 8) * OLED_WIDTH;
    if (color) {
        g_oled_buffer[idx] |= (1 << (y % 8));
    } else {
        g_oled_buffer[idx] &= ~(1 << (y % 8));
    }
}

/**
 * @brief  绘制6x8字符到缓冲区
 */
static void OLED_DrawChar(uint8_t x, uint8_t y, char c)
{
    if (c < 32 || c > 126) c = ' ';

    /* 简单6x8字体位图 - 使用程序化生成 */
    static const uint8_t simple_font_5x7[96][5] = {
        /* 仅关键字符，其余用点阵占位 */
        ['0' - 32] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
        ['1' - 32] = {0x00, 0x42, 0x7F, 0x40, 0x00},
        ['2' - 32] = {0x42, 0x61, 0x51, 0x49, 0x46},
        ['3' - 32] = {0x21, 0x41, 0x45, 0x4B, 0x31},
        ['4' - 32] = {0x18, 0x14, 0x12, 0x7F, 0x10},
        ['5' - 32] = {0x27, 0x45, 0x45, 0x45, 0x39},
        ['6' - 32] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
        ['7' - 32] = {0x01, 0x71, 0x09, 0x05, 0x03},
        ['8' - 32] = {0x36, 0x49, 0x49, 0x49, 0x36},
        ['9' - 32] = {0x06, 0x49, 0x49, 0x29, 0x1E},
        ['A' - 32] = {0x7E, 0x11, 0x09, 0x11, 0x7E},
        ['B' - 32] = {0x7F, 0x49, 0x49, 0x49, 0x36},
        ['C' - 32] = {0x3E, 0x41, 0x41, 0x41, 0x22},
        ['D' - 32] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
        ['E' - 32] = {0x7F, 0x49, 0x49, 0x49, 0x41},
        ['F' - 32] = {0x7F, 0x09, 0x09, 0x09, 0x01},
        ['G' - 32] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
        ['H' - 32] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
        ['I' - 32] = {0x00, 0x41, 0x7F, 0x41, 0x00},
        ['J' - 32] = {0x20, 0x40, 0x41, 0x3F, 0x01},
        ['K' - 32] = {0x7F, 0x08, 0x14, 0x22, 0x41},
        ['L' - 32] = {0x7F, 0x40, 0x40, 0x40, 0x40},
        ['M' - 32] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
        ['N' - 32] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
        ['O' - 32] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
        ['P' - 32] = {0x7F, 0x09, 0x09, 0x09, 0x06},
        ['Q' - 32] = {0x3E, 0x41, 0x51, 0x21, 0x5E},
        ['R' - 32] = {0x7F, 0x09, 0x19, 0x29, 0x46},
        ['S' - 32] = {0x46, 0x49, 0x49, 0x49, 0x31},
        ['T' - 32] = {0x01, 0x01, 0x7F, 0x01, 0x01},
        ['U' - 32] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
        ['V' - 32] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
        ['W' - 32] = {0x3F, 0x40, 0x38, 0x40, 0x3F},
        ['X' - 32] = {0x63, 0x14, 0x08, 0x14, 0x63},
        ['Y' - 32] = {0x07, 0x08, 0x70, 0x08, 0x07},
        ['Z' - 32] = {0x61, 0x51, 0x49, 0x45, 0x43},
        [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00},
        [':' - 32] = {0x00, 0x36, 0x36, 0x00, 0x00},
        ['-' - 32] = {0x08, 0x08, 0x08, 0x08, 0x08},
        ['/' - 32] = {0x60, 0x10, 0x08, 0x04, 0x03},
        ['.' - 32] = {0x00, 0x60, 0x60, 0x00, 0x00},
        [',' - 32] = {0x00, 0x80, 0x60, 0x00, 0x00},
    };

    int idx = c - 32;
    if (idx < 0 || idx >= 96) idx = 0;
    const uint8_t *bitmap = simple_font_5x7[idx];

    for (int col = 0; col < 5; col++) {
        uint8_t line = bitmap[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                OLED_DrawPixel(x + col, y + row, 1);
            }
        }
    }
}

/**
 * @brief  绘制字符串
 */
static void OLED_DrawString(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        OLED_DrawChar(x, y, *str);
        x += 6;
        if (x >= OLED_WIDTH) break;
        str++;
    }
}

/**
 * @brief  在指定页/列显示字符串
 */
void Display_PrintString(uint8_t page, uint8_t col, const char *str)
{
    OLED_DrawString(col, page * 8, str);
}

/**
 * @brief  显示启动画面
 */
void Display_ShowSplash(void)
{
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
    OLED_DrawString(10, 0, "Smart Carrier");
    OLED_DrawString(5, 20, "Initializing...");
    OLED_DrawString(2, 40, "FreeRTOS Ready");
    OLED_Refresh();
}

/**
 * @brief  显示错误信息
 */
void Display_ShowError(const char *msg)
{
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
    OLED_DrawString(0, 0, "!!! ERROR !!!");
    OLED_DrawString(0, 16, msg);
    OLED_Refresh();
}

/**
 * @brief  更新显示 (任务/进度)
 */
void Display_Update(const DisplayInfo_t *info)
{
    if (!info) return;

    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));

    char buf[22];

    /* 第1行: 状态 */
    snprintf(buf, sizeof(buf), "S: %s", STATE_NAMES[info->state]);
    OLED_DrawString(0, 0, buf);

    /* 第2行: 任务编码 */
    if (info->task_code.valid) {
        snprintf(buf, sizeof(buf), "T: %d%d%d",
                 info->task_code.digits[0],
                 info->task_code.digits[1],
                 info->task_code.digits[2]);
    } else {
        snprintf(buf, sizeof(buf), "T: ---");
    }
    OLED_DrawString(0, 10, buf);

    /* 第3行: 步骤 */
    snprintf(buf, sizeof(buf), "Step: %d/6", info->step_index + 1);
    OLED_DrawString(0, 20, buf);

    /* 第4行: 状态消息 */
    snprintf(buf, sizeof(buf), "%.21s", info->status_msg);
    OLED_DrawString(0, 30, buf);

    /* 第5行: 计时 */
    uint32_t sec = info->elapsed_time_s;
    snprintf(buf, sizeof(buf), "T: %02lu:%02lu", sec / 60, sec % 60);
    OLED_DrawString(0, 42, buf);

    /* 第6行: 进度条 */
    uint8_t progress = info->task_progress * 128 / 100;
    for (uint8_t x = 0; x < 128; x++) {
        for (uint8_t y = 56; y < 64; y++) {
            OLED_DrawPixel(x, y, (x < progress) ? 1 : 0);
        }
    }

    OLED_Refresh();
}
