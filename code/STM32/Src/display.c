/**
 * @file    display.c
 * @brief   OLED SSD1306 SPI 驱动 (WHEELTEC接口: PD11-DC, PD12-RST, PD13-SDA, PD14-SCL)
 */

#include "display.h"
#include <string.h>
#include <stdio.h>

/* WHEELTEC OLED引脚宏 */
#define OLED_DC_H()   HAL_GPIO_WritePin(OLED_SPI_DC_PORT,  OLED_SPI_DC_PIN,  GPIO_PIN_SET)
#define OLED_DC_L()   HAL_GPIO_WritePin(OLED_SPI_DC_PORT,  OLED_SPI_DC_PIN,  GPIO_PIN_RESET)
#define OLED_RST_H()  HAL_GPIO_WritePin(OLED_SPI_RST_PORT, OLED_SPI_RST_PIN, GPIO_PIN_SET)
#define OLED_RST_L()  HAL_GPIO_WritePin(OLED_SPI_RST_PORT, OLED_SPI_RST_PIN, GPIO_PIN_RESET)
#define OLED_SCL_H()  HAL_GPIO_WritePin(OLED_SPI_SCL_PORT, OLED_SPI_SCL_PIN, GPIO_PIN_SET)
#define OLED_SCL_L()  HAL_GPIO_WritePin(OLED_SPI_SCL_PORT, OLED_SPI_SCL_PIN, GPIO_PIN_RESET)
#define OLED_SDA_H()  HAL_GPIO_WritePin(OLED_SPI_SDA_PORT, OLED_SPI_SDA_PIN, GPIO_PIN_SET)
#define OLED_SDA_L()  HAL_GPIO_WritePin(OLED_SPI_SDA_PORT, OLED_SPI_SDA_PIN, GPIO_PIN_RESET)

static uint8_t g_oled_buf[128 * 8];

/* 软件SPI写一个字节 */
static void OLED_SPI_Write(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        OLED_SCL_L();
        if (data & 0x80) OLED_SDA_H(); else OLED_SDA_L();
        OLED_SCL_H();
        data <<= 1;
    }
}

/* 写命令 */
static void OLED_WriteCmd(uint8_t cmd)
{
    OLED_DC_L();
    OLED_SPI_Write(cmd);
    OLED_DC_H();
}

/* 写数据 */
static void OLED_WriteData(uint8_t data)
{
    OLED_DC_H();
    OLED_SPI_Write(data);
}

void Display_Init(I2C_HandleTypeDef *unused)
{
    (void)unused;

    /* GPIO初始化 */
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    g.Pin = OLED_SPI_DC_PIN;
    HAL_GPIO_Init(OLED_SPI_DC_PORT, &g);
    g.Pin = OLED_SPI_RST_PIN;
    HAL_GPIO_Init(OLED_SPI_RST_PORT, &g);
    g.Pin = OLED_SPI_SDA_PIN;
    HAL_GPIO_Init(OLED_SPI_SDA_PORT, &g);
    g.Pin = OLED_SPI_SCL_PIN;
    HAL_GPIO_Init(OLED_SPI_SCL_PORT, &g);

    /* 复位序列 */
    OLED_RST_L(); HAL_Delay(10);
    OLED_RST_H(); HAL_Delay(10);

    /* SSD1306初始化 */
    OLED_WriteCmd(0xAE); // Display OFF
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80); // Clock div
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F); // Mux=64
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00); // Offset=0
    OLED_WriteCmd(0x40); // Start line=0
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14); // Charge pump ON
    OLED_WriteCmd(0xA1); // Segment remap
    OLED_WriteCmd(0xC8); // COM scan reverse
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12); // COM pins
    OLED_WriteCmd(0x81); OLED_WriteCmd(0x7F); // Contrast
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1); // Pre-charge
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x40); // VCOM detect
    OLED_WriteCmd(0xA4); // Resume to RAM
    OLED_WriteCmd(0xA6); // Normal display
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x00); // Horizontal mode
    OLED_WriteCmd(0xAF); // Display ON
}

void OLED_Refresh(void)
{
    OLED_WriteCmd(0x21); OLED_WriteCmd(0); OLED_WriteCmd(127); // Col 0-127
    OLED_WriteCmd(0x22); OLED_WriteCmd(0); OLED_WriteCmd(7);   // Page 0-7
    for (uint16_t i = 0; i < 128 * 8; i++)
        OLED_WriteData(g_oled_buf[i]);
}

void Display_Clear(void)
{
    memset(g_oled_buf, 0, sizeof(g_oled_buf));
    OLED_Refresh();
}

/* 6x8 5x7字体 */
static const uint8_t font5x7[][5] = {
    ['0'-32]={0x3E,0x51,0x49,0x45,0x3E},['1'-32]={0x00,0x42,0x7F,0x40,0x00},
    ['2'-32]={0x42,0x61,0x51,0x49,0x46},['3'-32]={0x21,0x41,0x45,0x4B,0x31},
    ['4'-32]={0x18,0x14,0x12,0x7F,0x10},['5'-32]={0x27,0x45,0x45,0x45,0x39},
    ['6'-32]={0x3C,0x4A,0x49,0x49,0x30},['7'-32]={0x01,0x71,0x09,0x05,0x03},
    ['8'-32]={0x36,0x49,0x49,0x49,0x36},['9'-32]={0x06,0x49,0x49,0x29,0x1E},
    ['A'-32]={0x7E,0x11,0x09,0x11,0x7E},['B'-32]={0x7F,0x49,0x49,0x49,0x36},
    ['C'-32]={0x3E,0x41,0x41,0x41,0x22},['D'-32]={0x7F,0x41,0x41,0x22,0x1C},
    ['E'-32]={0x7F,0x49,0x49,0x49,0x41},['F'-32]={0x7F,0x09,0x09,0x09,0x01},
    ['G'-32]={0x3E,0x41,0x49,0x49,0x7A},['H'-32]={0x7F,0x08,0x08,0x08,0x7F},
    ['I'-32]={0x00,0x41,0x7F,0x41,0x00},['J'-32]={0x20,0x40,0x41,0x3F,0x01},
    ['K'-32]={0x7F,0x08,0x14,0x22,0x41},['L'-32]={0x7F,0x40,0x40,0x40,0x40},
    ['M'-32]={0x7F,0x02,0x0C,0x02,0x7F},['N'-32]={0x7F,0x04,0x08,0x10,0x7F},
    ['O'-32]={0x3E,0x41,0x41,0x41,0x3E},['P'-32]={0x7F,0x09,0x09,0x09,0x06},
    ['R'-32]={0x7F,0x09,0x19,0x29,0x46},['S'-32]={0x46,0x49,0x49,0x49,0x31},
    ['T'-32]={0x01,0x01,0x7F,0x01,0x01},['U'-32]={0x3F,0x40,0x40,0x40,0x3F},
    ['W'-32]={0x3F,0x40,0x38,0x40,0x3F},['X'-32]={0x63,0x14,0x08,0x14,0x63},
    ['Y'-32]={0x07,0x08,0x70,0x08,0x07},['Z'-32]={0x61,0x51,0x49,0x45,0x43},
    [' '-32]={0,0,0,0,0},[':'-32]={0,0x36,0x36,0,0},['.'-32]={0,0x60,0x60,0,0},
    ['/'-32]={0x60,0x10,0x08,0x04,0x03},['-'-32]={0x08,0x08,0x08,0x08,0x08},
};

static void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t c)
{
    if (x >= 128 || y >= 64) return;
    uint16_t idx = x + (y / 8) * 128;
    if (c) g_oled_buf[idx] |= (1 << (y % 8));
    else   g_oled_buf[idx] &= ~(1 << (y % 8));
}

static void OLED_DrawChar(uint8_t x, uint8_t y, char c)
{
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    const uint8_t *b = font5x7[idx];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (b[col] & (1 << row))
                OLED_DrawPixel(x + col, y + row, 1);
}

static void OLED_DrawStr(uint8_t x, uint8_t y, const char *s)
{
    while (*s) { OLED_DrawChar(x, y, *s); x += 6; s++; }
}

void Display_ShowSplash(void)
{
    memset(g_oled_buf, 0, sizeof(g_oled_buf));
    OLED_DrawStr(5, 0, "Smart Carrier");
    OLED_DrawStr(5, 20, "GMR Encoder");
    OLED_DrawStr(5, 40, "FreeRTOS Ready");
    OLED_Refresh();
}

void Display_ShowError(const char *msg)
{
    memset(g_oled_buf, 0, sizeof(g_oled_buf));
    OLED_DrawStr(0, 0, "!!! ERROR !!!");
    OLED_DrawStr(0, 16, msg);
    OLED_Refresh();
}

void Display_Update(const DisplayInfo_t *info)
{
    if (!info) return;
    memset(g_oled_buf, 0, sizeof(g_oled_buf));
    char b[22];

    snprintf(b, sizeof(b), "S:%s", STATE_NAMES[info->state]);
    OLED_DrawStr(0, 0, b);

    if (info->task_code.valid)
        snprintf(b, sizeof(b), "T:%d%d%d", info->task_code.digits[0],
                 info->task_code.digits[1], info->task_code.digits[2]);
    else snprintf(b, sizeof(b), "T:---");
    OLED_DrawStr(0, 10, b);

    snprintf(b, sizeof(b), "Step:%d/6", info->step_index + 1);
    OLED_DrawStr(0, 20, b);

    snprintf(b, sizeof(b), "%.21s", info->status_msg);
    OLED_DrawStr(0, 30, b);

    uint32_t s = info->elapsed_time_s;
    snprintf(b, sizeof(b), "T:%02lu:%02lu", s / 60, s % 60);
    OLED_DrawStr(0, 42, b);

    uint8_t p = info->task_progress * 128 / 100;
    for (uint8_t x = 0; x < 128; x++)
        for (uint8_t y = 56; y < 64; y++)
            OLED_DrawPixel(x, y, x < p ? 1 : 0);

    OLED_Refresh();
}

void Display_PrintString(uint8_t page, uint8_t col, const char *s)
{
    OLED_DrawStr(col, page * 8, s);
}
