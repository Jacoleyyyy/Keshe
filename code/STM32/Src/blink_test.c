/**
 * @file    blink_test.c
 * @brief   板载 LED 闪烁测试 — 验证 MCU 是否正常跑起来
 *
 * 用法:
 *   1. 编译: gcc 在 CMakeLists.txt 临时替换 main.c 为 blink_test.c
 *      或者在 Keil 中新建工程只加这个文件 + HAL/CMSIS
 *   2. 烧录 → 板子上的 LED 以 200ms 频率闪烁
 *
 * WHEELTEC C30D 板载 LED:
 *   V1.0 版 → PA12
 *   V1.1 版 → PE8
 *   两个都写, 至少一个会亮
 */

#include "stm32f4xx_hal.h"

/* 两个版本的 LED 都试 */
#define LED1_PORT  GPIOA
#define LED1_PIN   GPIO_PIN_12   // V1.0 板 LED
#define LED2_PORT  GPIOE
#define LED2_PIN   GPIO_PIN_8    // V1.1 板 LED

void SystemClock_Config(void);
static void LED_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    /* 启动: LED 快闪 3 次表示进入程序 */
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
        HAL_Delay(100);
    }

    /* 主循环: 500ms 一直流闪烁 */
    while (1) {
        HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
        HAL_GPIO_TogglePin(LED2_PORT, LED2_PIN);
        HAL_Delay(500);
    }
}

static void LED_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    g.Pin = LED1_PIN;
    HAL_GPIO_Init(LED1_PORT, &g);
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

    g.Pin = LED2_PIN;
    HAL_GPIO_Init(LED2_PORT, &g);
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}
