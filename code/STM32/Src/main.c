/**
 * @file    main.c
 * @brief   智能搬运机器人 — 系统入口 (GMR编码器版)
 *
 * 适配 WHEELTEC C30D 2.0 硬件:
 *   电机PWM: TIM1(MC/MD), TIM9(MB), TIM10(MA_CH1), TIM11(MA_CH2) @ 10kHz
 *   编码器:  TIM2(ENC_A→MC), TIM3(ENC_B→MD), TIM4(ENC_C→MB), TIM5(ENC_D→MA)
 *   舵机PWM: TIM8 CH1-4 @ 100Hz
 *   OLED:    SPI (PD11-14)
 *   UART3:   PB10/PB11 → MaixCAM
 */

#include "main.h"

/* ============================================================
 * 外设句柄
 * ============================================================ */
TIM_HandleTypeDef htim1, htim9, htim10, htim11;         /* 电机PWM */
TIM_HandleTypeDef htim2_enc, htim3_enc, htim4_enc, htim5_enc; /* 编码器 */
TIM_HandleTypeDef htim8;                                 /* 舵机 */
UART_HandleTypeDef huart3;                               /* MaixCAM UART */

volatile bool g_system_ready = false;

/* ============================================================
 * FreeRTOS 任务属性
 * ============================================================ */
static const osThreadAttr_t TaskManager_Attr = {
    .name = "TaskManager", .priority = (osPriority_t)PRIO_TASK_MANAGER, .stack_size = STACK_TASK_MANAGER * 4
};
static const osThreadAttr_t TaskChassis_Attr = {
    .name = "TaskChassis", .priority = (osPriority_t)PRIO_TASK_CHASSIS, .stack_size = STACK_TASK_CHASSIS * 4
};
static const osThreadAttr_t TaskArm_Attr = {
    .name = "TaskArm", .priority = (osPriority_t)PRIO_TASK_ARM, .stack_size = STACK_TASK_ARM * 4
};
static const osThreadAttr_t TaskComm_Attr = {
    .name = "TaskComm", .priority = (osPriority_t)PRIO_TASK_COMM, .stack_size = STACK_TASK_COMM * 4
};
static const osThreadAttr_t TaskSensor_Attr = {
    .name = "TaskSensor", .priority = (osPriority_t)PRIO_TASK_SENSOR, .stack_size = STACK_TASK_SENSOR * 4
};
static const osThreadAttr_t TaskDisplay_Attr = {
    .name = "TaskDisplay", .priority = (osPriority_t)PRIO_TASK_DISPLAY, .stack_size = STACK_TASK_DISPLAY * 4
};
static const osThreadAttr_t TaskObstacle_Attr = {
    .name = "TaskObstacle", .priority = (osPriority_t)PRIO_TASK_OBSTACLE, .stack_size = STACK_TASK_OBSTACLE * 4
};
static const osThreadAttr_t TaskMonitor_Attr = {
    .name = "TaskMonitor", .priority = (osPriority_t)PRIO_TASK_MONITOR, .stack_size = STACK_TASK_MONITOR * 4
};

/* ============================================================
 * main()
 * ============================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM1_Init();     /* 电机C/D PWM */
    MX_TIM8_Init();     /* 舵机 PWM */
    MX_TIM9_Init();     /* 电机B PWM */
    MX_TIM10_Init();    /* 电机A CH1 PWM */
    MX_TIM11_Init();    /* 电机A CH2 PWM */
    MX_TIM2_Enc_Init(); /* 编码器A→电机C */
    MX_TIM3_Enc_Init(); /* 编码器B→电机D */
    MX_TIM4_Enc_Init(); /* 编码器C→电机B */
    MX_TIM5_Enc_Init(); /* 编码器D→电机A */
    MX_USART3_Init();
    USART3_IRQ_Enable();

    /* 模块初始化 */
    Motor_InitAll();
    Servo_Init(&htim8);
    Chassis_Init();
    Sensor_Init();
    Display_Init(NULL);
    Communication_Init(&huart3);
    TaskManager_Init();

    /* 使能电机 */
    HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_SET);

    Display_ShowSplash();
    HAL_Delay(2000);
    g_system_ready = true;

    /* 创建任务 */
    g_task_handle_manager  = osThreadNew(Task_Manager,       NULL, &TaskManager_Attr);
    g_task_handle_chassis  = osThreadNew(Task_Chassis,       NULL, &TaskChassis_Attr);
    g_task_handle_arm      = osThreadNew(Task_Arm,           NULL, &TaskArm_Attr);
    g_task_handle_comm     = osThreadNew(Task_Communication, NULL, &TaskComm_Attr);
    g_task_handle_sensor   = osThreadNew(Task_Sensor,        NULL, &TaskSensor_Attr);
    g_task_handle_display  = osThreadNew(Task_Display,       NULL, &TaskDisplay_Attr);
    g_task_handle_obstacle = osThreadNew(Task_Obstacle,      NULL, &TaskObstacle_Attr);
    g_task_handle_monitor  = osThreadNew(Task_Monitor,       NULL, &TaskMonitor_Attr);

    if (!g_task_handle_manager || !g_task_handle_chassis) Error_Handler();

    osKernelInitialize();
    osKernelStart();

    for (;;) Error_Handler();
}

/* ============================================================
 * 系统时钟: HSE 8MHz → PLL → 168MHz
 * ============================================================ */
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
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

/* ============================================================
 * GPIO 初始化
 * ============================================================ */
void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    /* 使能引脚 PD3: WHEELTEC使能开关, 高有效 */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pin = EN_PIN;
    HAL_GPIO_Init(EN_PORT, &g);

    /* OLED SPI 引脚在 Display_Init 中初始化 */
    /* 其余引脚 (传感器/按钮) 在 Sensor_Init 中初始化 */
}

/* ============================================================
 * TIM1: 电机C/D PWM, 10kHz, 168MHz/(16799+1) = 10kHz
 * ============================================================ */
void MX_TIM1_Init(void)
{
    __HAL_RCC_TIM1_CLK_ENABLE();
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.Period = MOTOR_PWM_ARR;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim1);
}

void MX_TIM9_Init(void)
{
    __HAL_RCC_TIM9_CLK_ENABLE();
    htim9.Instance = TIM9;
    htim9.Init.Prescaler = 0;
    htim9.Init.Period = MOTOR_PWM_ARR;
    htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim9);
}

void MX_TIM10_Init(void)
{
    __HAL_RCC_TIM10_CLK_ENABLE();
    htim10.Instance = TIM10;
    htim10.Init.Prescaler = 0;
    htim10.Init.Period = MOTOR_PWM_ARR;
    htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim10);
}

void MX_TIM11_Init(void)
{
    __HAL_RCC_TIM11_CLK_ENABLE();
    htim11.Instance = TIM11;
    htim11.Init.Prescaler = 0;
    htim11.Init.Period = MOTOR_PWM_ARR;
    htim11.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim11.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim11);
}

/* ============================================================
 * TIM8: 舵机 PWM, 100Hz
 * 168MHz / 168 = 1MHz → /10000 = 100Hz
 * ============================================================ */
void MX_TIM8_Init(void)
{
    __HAL_RCC_TIM8_CLK_ENABLE();
    htim8.Instance = TIM8;
    htim8.Init.Prescaler = SERVO_PSC;
    htim8.Init.Period = SERVO_ARR;
    htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim8);
}

/* ============================================================
 * 编码器初始化: TIM2-TIM5 全部 4倍频
 * ============================================================ */
static void MX_Encoder_Init(TIM_HandleTypeDef *htim, TIM_TypeDef *inst)
{
    __HAL_RCC_TIM2_CLK_ENABLE(); /* 所有编码器TIM2-5在APB1, 这里只使能一次 */
    htim->Instance = inst;
    htim->Init.Prescaler = 0;
    htim->Init.Period = 0xFFFF;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
}

void MX_TIM2_Enc_Init(void)
{
    MX_Encoder_Init(&htim2_enc, TIM2);
    TIM_Encoder_InitTypeDef e = {0};
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1;
    e.IC1Filter = 0;
    e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1;
    e.IC2Filter = 0;
    HAL_TIM_Encoder_Init(&htim2_enc, &e);
}

void MX_TIM3_Enc_Init(void)
{
    MX_Encoder_Init(&htim3_enc, TIM3);
    TIM_Encoder_InitTypeDef e = {0};
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1; e.IC1Filter = 0;
    e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1; e.IC2Filter = 0;
    HAL_TIM_Encoder_Init(&htim3_enc, &e);
}

void MX_TIM4_Enc_Init(void)
{
    MX_Encoder_Init(&htim4_enc, TIM4);
    TIM_Encoder_InitTypeDef e = {0};
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1; e.IC1Filter = 0;
    e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1; e.IC2Filter = 0;
    HAL_TIM_Encoder_Init(&htim4_enc, &e);
}

void MX_TIM5_Enc_Init(void)
{
    MX_Encoder_Init(&htim5_enc, TIM5);
    TIM_Encoder_InitTypeDef e = {0};
    e.EncoderMode = TIM_ENCODERMODE_TI12;
    e.IC1Polarity = TIM_ICPOLARITY_RISING;
    e.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC1Prescaler = TIM_ICPSC_DIV1; e.IC1Filter = 0;
    e.IC2Polarity = TIM_ICPOLARITY_RISING;
    e.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    e.IC2Prescaler = TIM_ICPSC_DIV1; e.IC2Filter = 0;
    HAL_TIM_Encoder_Init(&htim5_enc, &e);
}

/* ============================================================
 * USART3: MaixCAM 通信
 * ============================================================ */
void MX_USART3_Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();
    huart3.Instance = USART3;
    huart3.Init.BaudRate = MAIX_UART_BAUDRATE;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

void USART3_IRQ_Enable(void)
{
    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

/* ============================================================
 * HAL MSP 回调 — GPIO AF 配置
 * ============================================================ */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    if (htim->Instance == TIM1) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        g.Alternate = GPIO_AF1_TIM1;
        g.Pin = GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
        HAL_GPIO_Init(GPIOE, &g);
    }
    else if (htim->Instance == TIM8) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        g.Alternate = GPIO_AF3_TIM8;
        g.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
        HAL_GPIO_Init(GPIOC, &g);
    }
    else if (htim->Instance == TIM9) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        g.Alternate = GPIO_AF3_TIM9;
        g.Pin = GPIO_PIN_5 | GPIO_PIN_6;
        HAL_GPIO_Init(GPIOE, &g);
    }
    else if (htim->Instance == TIM10) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Alternate = GPIO_AF3_TIM10;
        g.Pin = GPIO_PIN_8;
        HAL_GPIO_Init(GPIOB, &g);
    }
    else if (htim->Instance == TIM11) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Alternate = GPIO_AF3_TIM11;
        g.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOB, &g);
    }
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;

    if (htim->Instance == TIM2) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Alternate = GPIO_AF1_TIM2;
        g.Pin = GPIO_PIN_15; HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_3;  HAL_GPIO_Init(GPIOB, &g);
    }
    else if (htim->Instance == TIM3) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Alternate = GPIO_AF2_TIM3;
        g.Pin = GPIO_PIN_4 | GPIO_PIN_5;
        HAL_GPIO_Init(GPIOB, &g);
    }
    else if (htim->Instance == TIM4) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Alternate = GPIO_AF2_TIM4;
        g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOB, &g);
    }
    else if (htim->Instance == TIM5) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        g.Alternate = GPIO_AF2_TIM5;
        g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        GPIO_InitTypeDef g = {0};
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF7_USART3;
        g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        HAL_GPIO_Init(GPIOD, &g);
    }
}

/* ============================================================
 * UART3 中断
 * ============================================================ */
void USART3_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart3.Instance->DR & 0xFF);
        Communication_RxCallback(byte);
        __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_RXNE);
    }
    HAL_UART_IRQHandler(&huart3);
}

/* ============================================================
 * 错误处理
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    for (;;) {
        HAL_GPIO_TogglePin(SYS_LED_PORT, SYS_LED_PIN);
        for (volatile uint32_t i = 0; i < 1000000; i++) { __NOP(); }
    }
}
