/**
 * oled_test.c — OLED "ceshi 测试" 全屏点亮 (HAL)
 */
#include "stm32f4xx_hal.h"

#define DC    GPIO_PIN_11
#define RST   GPIO_PIN_12
#define SDA   GPIO_PIN_13
#define SCL   GPIO_PIN_14

static void oled_wr(uint8_t d, uint8_t dc) {
    if(dc) GPIOD->BSRR=DC; else GPIOD->BSRR=DC<<16;
    for(int i=0;i<8;i++){
        GPIOD->BSRR=SCL<<16;
        if(d&0x80) GPIOD->BSRR=SDA; else GPIOD->BSRR=SDA<<16;
        GPIOD->BSRR=SCL;
        d<<=1;
    }
    GPIOD->BSRR=DC;
}

int main(void) {
    HAL_Init();
    /* 直接用 HAL 的 SystemClock_Config (来自 main.c) —
       但 oled_test 没有 main.c, 所以内联一个 */
    {
        RCC_OscInitTypeDef osc = {0}; RCC_ClkInitTypeDef clk = {0};
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSE; osc.HSEState = RCC_HSE_ON;
        osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
        osc.PLL.PLLM = 8; osc.PLL.PLLN = 336; osc.PLL.PLLP = RCC_PLLP_DIV2; osc.PLL.PLLQ = 4;
        HAL_RCC_OscConfig(&osc);
        clk.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
        clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
        clk.APB1CLKDivider = RCC_HCLK_DIV4; clk.APB2CLKDivider = RCC_HCLK_DIV2;
        HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);
        HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
        HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
    }

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    GPIOD->MODER  = (GPIOD->MODER  & ~0xFF000000) | 0x55000000;
    GPIOD->OTYPER &= ~0xF000;
    GPIOD->PUPDR  &= ~0xFF000000;
    GPIOD->OSPEEDR &= ~0xFF000000;

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);
    HAL_PWR_DisableBkUpAccess();

    GPIOD->BSRR=RST<<16; HAL_Delay(100); GPIOD->BSRR=RST;

    oled_wr(0xAE,0); oled_wr(0xD5,0); oled_wr(80,0); oled_wr(0xA8,0); oled_wr(0x3F,0);
    oled_wr(0xD3,0); oled_wr(0x00,0); oled_wr(0x40,0);
    oled_wr(0x8D,0); oled_wr(0x14,0);
    oled_wr(0x20,0); oled_wr(0x02,0);
    oled_wr(0xA1,0); oled_wr(0xC0,0); oled_wr(0xDA,0); oled_wr(0x12,0);
    oled_wr(0x81,0); oled_wr(0xEF,0); oled_wr(0xD9,0); oled_wr(0xF1,0);
    oled_wr(0xDB,0); oled_wr(0x30,0); oled_wr(0xA4,0); oled_wr(0xA6,0);
    oled_wr(0xAF,0);

    for(int pg=0; pg<8; pg++) {
        oled_wr(0xB0+pg, 0); oled_wr(0x00,0); oled_wr(0x10,0);
        for(int c=0; c<128; c++) oled_wr(0xFF, 1);
    }

    while(1);
}
