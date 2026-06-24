/* motor_test.c — C30D 1.0 版: PD8/PD9 UART, PA12 LED, 30:1 减速比 */
#include "stm32f4xx_hal.h"
int main(void){
    HAL_Init();
    RCC_OscInitTypeDef o={0}; RCC_ClkInitTypeDef c={0};
    o.OscillatorType=RCC_OSCILLATORTYPE_HSE; o.HSEState=RCC_HSE_ON;
    o.PLL.PLLState=RCC_PLL_ON; o.PLL.PLLSource=RCC_PLLSOURCE_HSE;
    o.PLL.PLLM=8; o.PLL.PLLN=336; o.PLL.PLLP=RCC_PLLP_DIV2; o.PLL.PLLQ=4;
    HAL_RCC_OscConfig(&o);
    c.ClockType=RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK; c.AHBCLKDivider=RCC_SYSCLK_DIV1;
    c.APB1CLKDivider=RCC_HCLK_DIV4; c.APB2CLKDivider=RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&c,FLASH_LATENCY_5);
    SystemCoreClock=168000000;
    HAL_SYSTICK_Config(168000000/1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    __HAL_RCC_GPIOD_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE(); __HAL_RCC_TIM9_CLK_ENABLE();
    __HAL_RCC_TIM10_CLK_ENABLE(); __HAL_RCC_TIM11_CLK_ENABLE();

    GPIO_InitTypeDef g={0};
    /* PD3 输出高 → 使能电机驱动 */
    g.Mode=GPIO_MODE_OUTPUT_PP; g.Pin=GPIO_PIN_3; HAL_GPIO_Init(GPIOD,&g);
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,GPIO_PIN_SET);
    /* PA12 心跳 LED */
    g.Mode=GPIO_MODE_OUTPUT_PP; g.Pin=GPIO_PIN_12; HAL_GPIO_Init(GPIOA,&g);

    /* UART3: PD8(TX) PD9(RX) */
    __HAL_RCC_USART3_CLK_ENABLE();
    g.Mode=GPIO_MODE_AF_PP; g.Pull=GPIO_PULLUP; g.Speed=GPIO_SPEED_FREQ_HIGH;
    g.Alternate=GPIO_AF7_USART3; g.Pin=GPIO_PIN_8;
    HAL_GPIO_Init(GPIOD,&g);

    TIM_HandleTypeDef h1,h9,h10,h11;
    TIM_OC_InitTypeDef oc={0};
    oc.OCMode=TIM_OCMODE_PWM1; oc.OCPolarity=TIM_OCPOLARITY_HIGH; oc.Pulse=0;

    /* TIM1 PE9/PE11/PE13/PE14 */
    g.Mode=GPIO_MODE_AF_PP; g.Pull=GPIO_PULLUP; g.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate=GPIO_AF1_TIM1; g.Pin=GPIO_PIN_9|GPIO_PIN_11|GPIO_PIN_13|GPIO_PIN_14;
    HAL_GPIO_Init(GPIOE,&g);
    h1.Instance=TIM1; h1.Init.Prescaler=0; h1.Init.Period=16799;
    h1.Init.CounterMode=TIM_COUNTERMODE_UP; h1.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    h1.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&h1);
    HAL_TIM_PWM_ConfigChannel(&h1,&oc,TIM_CHANNEL_1); HAL_TIM_PWM_Start(&h1,TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&h1,&oc,TIM_CHANNEL_2); HAL_TIM_PWM_Start(&h1,TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&h1,&oc,TIM_CHANNEL_3); HAL_TIM_PWM_Start(&h1,TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&h1,&oc,TIM_CHANNEL_4); HAL_TIM_PWM_Start(&h1,TIM_CHANNEL_4);

    /* TIM9 PE5/PE6 */
    g.Alternate=GPIO_AF3_TIM9; g.Pin=GPIO_PIN_5|GPIO_PIN_6;
    HAL_GPIO_Init(GPIOE,&g);
    h9.Instance=TIM9; h9.Init=h1.Init; HAL_TIM_PWM_Init(&h9);
    HAL_TIM_PWM_ConfigChannel(&h9,&oc,TIM_CHANNEL_1); HAL_TIM_PWM_Start(&h9,TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&h9,&oc,TIM_CHANNEL_2); HAL_TIM_PWM_Start(&h9,TIM_CHANNEL_2);

    /* TIM10 PB8 */
    g.Alternate=GPIO_AF3_TIM10; g.Pin=GPIO_PIN_8;
    HAL_GPIO_Init(GPIOB,&g);
    h10.Instance=TIM10; h10.Init=h1.Init; HAL_TIM_PWM_Init(&h10);
    HAL_TIM_PWM_ConfigChannel(&h10,&oc,TIM_CHANNEL_1); HAL_TIM_PWM_Start(&h10,TIM_CHANNEL_1);

    /* TIM11 PB9 */
    g.Alternate=GPIO_AF3_TIM11; g.Pin=GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB,&g);
    h11.Instance=TIM11; h11.Init=h1.Init; HAL_TIM_PWM_Init(&h11);
    HAL_TIM_PWM_ConfigChannel(&h11,&oc,TIM_CHANNEL_1); HAL_TIM_PWM_Start(&h11,TIM_CHANNEL_1);

    for(int i=0;i<3;i++){HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_12);HAL_Delay(200);}

    uint32_t d=8400;
    __HAL_TIM_SET_COMPARE(&h10,TIM_CHANNEL_1,d);HAL_Delay(2000);__HAL_TIM_SET_COMPARE(&h10,TIM_CHANNEL_1,0);HAL_Delay(500);
    __HAL_TIM_SET_COMPARE(&h9,TIM_CHANNEL_1,d); HAL_Delay(2000);__HAL_TIM_SET_COMPARE(&h9,TIM_CHANNEL_1,0); HAL_Delay(500);
    __HAL_TIM_SET_COMPARE(&h1,TIM_CHANNEL_2,d); HAL_Delay(2000);__HAL_TIM_SET_COMPARE(&h1,TIM_CHANNEL_2,0); HAL_Delay(500);
    __HAL_TIM_SET_COMPARE(&h1,TIM_CHANNEL_4,d); HAL_Delay(2000);__HAL_TIM_SET_COMPARE(&h1,TIM_CHANNEL_4,0);HAL_Delay(500);

    while(1){HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_12);HAL_Delay(500);}
}
