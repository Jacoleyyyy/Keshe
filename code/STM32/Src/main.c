/**
 * @file    main.c
 * @brief   智能搬运机器人 - 系统入口
 *
 * 硬件平台: STM32F407 (Cortex-M4, 168MHz)
 * RTOS:     FreeRTOS (CMSIS OS v2)
 * 视觉:     MaixCAM Pro (UART通信)
 *
 * 启动流程:
 *   1. HAL 库初始化
 *   2. 系统时钟配置 (168MHz)
 *   3. 外设初始化 (GPIO, TIM, UART, I2C)
 *   4. FreeRTOS 内核启动
 *   5. 创建8个任务
 *   6. 调度器开始运行
 */

#include "main.h"

/* ============================================================
 * 外设句柄
 * ============================================================ */
TIM_HandleTypeDef  htim_motor_pwm;    /* TIM2: 电机PWM */
TIM_HandleTypeDef  htim_m1_enc;       /* TIM3: 电机1编码器 */
TIM_HandleTypeDef  htim_m2_enc;       /* TIM4: 电机2编码器 */
TIM_HandleTypeDef  htim_m3_enc;       /* TIM5: 电机3编码器 */
TIM_HandleTypeDef  htim_m4_enc;       /* TIM8: 电机4编码器 */
TIM_HandleTypeDef  htim_servo;        /* TIM1: 舵机 */
UART_HandleTypeDef huart_maix;        /* USART3: MaixCAM通信 */
I2C_HandleTypeDef  hi2c_oled;         /* I2C1: OLED */

/* 系统就绪标志 */
volatile bool g_system_ready = false;

/* ============================================================
 * FreeRTOS 任务属性
 * ============================================================ */
static const osThreadAttr_t TaskManager_Attr = {
    .name       = "TaskManager",
    .priority   = (osPriority_t)PRIO_TASK_MANAGER,
    .stack_size = STACK_TASK_MANAGER * 4,
};

static const osThreadAttr_t TaskChassis_Attr = {
    .name       = "TaskChassis",
    .priority   = (osPriority_t)PRIO_TASK_CHASSIS,
    .stack_size = STACK_TASK_CHASSIS * 4,
};

static const osThreadAttr_t TaskArm_Attr = {
    .name       = "TaskArm",
    .priority   = (osPriority_t)PRIO_TASK_ARM,
    .stack_size = STACK_TASK_ARM * 4,
};

static const osThreadAttr_t TaskComm_Attr = {
    .name       = "TaskComm",
    .priority   = (osPriority_t)PRIO_TASK_COMM,
    .stack_size = STACK_TASK_COMM * 4,
};

static const osThreadAttr_t TaskSensor_Attr = {
    .name       = "TaskSensor",
    .priority   = (osPriority_t)PRIO_TASK_SENSOR,
    .stack_size = STACK_TASK_SENSOR * 4,
};

static const osThreadAttr_t TaskDisplay_Attr = {
    .name       = "TaskDisplay",
    .priority   = (osPriority_t)PRIO_TASK_DISPLAY,
    .stack_size = STACK_TASK_DISPLAY * 4,
};

static const osThreadAttr_t TaskObstacle_Attr = {
    .name       = "TaskObstacle",
    .priority   = (osPriority_t)PRIO_TASK_OBSTACLE,
    .stack_size = STACK_TASK_OBSTACLE * 4,
};

static const osThreadAttr_t TaskMonitor_Attr = {
    .name       = "TaskMonitor",
    .priority   = (osPriority_t)PRIO_TASK_MONITOR,
    .stack_size = STACK_TASK_MONITOR * 4,
};

/* ============================================================
 * main — 程序入口
 * ============================================================ */
int main(void)
{
    /* HAL 库初始化 */
    HAL_Init();

    /* 系统时钟配置: HSE 8MHz → PLL → 168MHz */
    SystemClock_Config();

    /* 外设初始化 */
    MX_GPIO_Init();
    MX_TIM_Init();
    MX_USART_Init();
    MX_I2C_Init();

    /* 模块初始化 */
    Motor_InitAll();
    Servo_Init(&htim_servo);
    Chassis_Init();
    Sensor_Init();
    Display_Init(&hi2c_oled);
    Communication_Init(&huart_maix);
    TaskManager_Init();

    /* 显示启动画面 */
    Display_ShowSplash();
    HAL_Delay(2000);

    /* 标记系统就绪 */
    g_system_ready = true;

    /* ============================================================
     * 创建 FreeRTOS 任务
     * ============================================================ */

    /* 核心任务 */
    g_task_handle_manager = osThreadNew(Task_Manager, NULL, &TaskManager_Attr);

    /* 运动控制 */
    g_task_handle_chassis = osThreadNew(Task_Chassis, NULL, &TaskChassis_Attr);

    /* 机械臂控制 */
    g_task_handle_arm = osThreadNew(Task_Arm, NULL, &TaskArm_Attr);

    /* 通信管理 */
    g_task_handle_comm = osThreadNew(Task_Communication, NULL, &TaskComm_Attr);

    /* 传感器 */
    g_task_handle_sensor = osThreadNew(Task_Sensor, NULL, &TaskSensor_Attr);

    /* 显示更新 */
    g_task_handle_display = osThreadNew(Task_Display, NULL, &TaskDisplay_Attr);

    /* 避障监控 */
    g_task_handle_obstacle = osThreadNew(Task_Obstacle, NULL, &TaskObstacle_Attr);

    /* 系统监控 */
    g_task_handle_monitor = osThreadNew(Task_Monitor, NULL, &TaskMonitor_Attr);

    /* 检查任务创建 */
    if (g_task_handle_manager == NULL ||
        g_task_handle_chassis == NULL ||
        g_task_handle_arm == NULL ||
        g_task_handle_comm == NULL) {
        Error_Handler();
    }

    /* ============================================================
     * 启动调度器 (此函数不返回)
     * ============================================================ */
    osKernelInitialize();
    osKernelStart();

    /* 不应到达此处 */
    for (;;) {
        Error_Handler();
    }
}

/* ============================================================
 * 系统时钟配置
 * HSE 8MHz → PLL (x336, /8) → 168MHz
 * AHB: 168MHz, APB1: 42MHz, APB2: 84MHz
 * ============================================================ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 使能 HSE (8MHz 外部晶振) 并配置 PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;          /* HSE / 8 = 1MHz */
    RCC_OscInitStruct.PLL.PLLN = 336;        /* 1MHz * 336 = 336MHz VCO */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;  /* 336MHz / 2 = 168MHz (SYSCLK) */
    RCC_OscInitStruct.PLL.PLLQ = 4;          /* SPI / I2S 时钟 */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /* 配置时钟树
     * SYSCLK  = 168MHz
     * HCLK    = 168MHz
     * PCLK1   = 42MHz  (APB1, max 42MHz)
     * PCLK2   = 84MHz  (APB2, max 84MHz)
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                   RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 |
                                   RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;   /* 168/4 = 42MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;   /* 168/2 = 84MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* 配置 SysTick 为 1ms (FreeRTOS 时基) */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    /* NVIC 优先级分组: 4位抢占优先级 */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORGROUP_4);
}

/* ============================================================
 * GPIO 初始化
 * ============================================================ */
void MX_GPIO_Init(void)
{
    /* 使能 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 方向引脚初始化在 Motor_Init 中完成 */
    /* 传感器/按钮在 Sensor_Init 中初始化 */
    /* 其余引脚使用默认状态 */
}

/* ============================================================
 * 定时器初始化
 * ============================================================ */
void MX_TIM_Init(void)
{
    TIM_OC_InitTypeDef sOC = {0};
    TIM_Encoder_InitTypeDef sEncoder = {0};

    /* 使能定时器时钟 */
    __HAL_RCC_TIM1_CLK_ENABLE();   /* APB2: 舵机 */
    __HAL_RCC_TIM2_CLK_ENABLE();   /* APB1: 电机PWM */
    __HAL_RCC_TIM3_CLK_ENABLE();   /* APB1: M1编码器 */
    __HAL_RCC_TIM4_CLK_ENABLE();   /* APB1: M2编码器 */
    __HAL_RCC_TIM5_CLK_ENABLE();   /* APB1: M3编码器 */
    __HAL_RCC_TIM8_CLK_ENABLE();   /* APB2: M4编码器 */

    /* ============================================================
     * TIM1: 舵机 PWM (50Hz)
     * 时钟 = APB2 * 2 = 168MHz (APB2 prescaler != 1 时有倍频)
     * 168MHz / 168 = 1MHz → 20,000 = 50Hz
     * ============================================================ */
    htim_servo.Instance = TIM1;
    htim_servo.Init.Prescaler = SERVO_PRESCALER;
    htim_servo.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_servo.Init.Period = SERVO_PERIOD;
    htim_servo.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_servo.Init.RepetitionCounter = 0;
    htim_servo.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim_servo) != HAL_OK) {
        Error_Handler();
    }

    sOC.OCMode = TIM_OCMODE_PWM1;
    sOC.Pulse = 1500;  /* 中位: 1500us */
    sOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim_servo, &sOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim_servo, &sOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim_servo, &sOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim_servo, &sOC, TIM_CHANNEL_4);

    /* ============================================================
     * TIM2: 电机 PWM (10kHz)
     * 时钟 = APB1 * 2 = 84MHz (APB1 prescaler=4, timer clock=84MHz)
     * 84MHz / (0+1) = 84MHz → 84MHz/(8399+1)=10kHz
     * ============================================================ */
    htim_motor_pwm.Instance = TIM2;
    htim_motor_pwm.Init.Prescaler = 0;
    htim_motor_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_motor_pwm.Init.Period = MOTOR_PWM_ARR;
    htim_motor_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_motor_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim_motor_pwm) != HAL_OK) {
        Error_Handler();
    }

    sOC.OCMode = TIM_OCMODE_PWM1;
    sOC.Pulse = 0;
    sOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim_motor_pwm, &sOC, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim_motor_pwm, &sOC, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim_motor_pwm, &sOC, TIM_CHANNEL_3);
    HAL_TIM_PWM_ConfigChannel(&htim_motor_pwm, &sOC, TIM_CHANNEL_4);

    /* ============================================================
     * TIM3: 电机1编码器 (4倍频)
     * ============================================================ */
    htim_m1_enc.Instance = TIM3;
    htim_m1_enc.Init.Prescaler = 0;
    htim_m1_enc.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_m1_enc.Init.Period = 0xFFFF;
    htim_m1_enc.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_m1_enc.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    sEncoder.EncoderMode = TIM_ENCODERMODE_TI12;
    sEncoder.IC1Polarity = TIM_ICPOLARITY_RISING;
    sEncoder.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sEncoder.IC1Prescaler = TIM_ICPSC_DIV1;
    sEncoder.IC1Filter = 0;
    sEncoder.IC2Polarity = TIM_ICPOLARITY_RISING;
    sEncoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sEncoder.IC2Prescaler = TIM_ICPSC_DIV1;
    sEncoder.IC2Filter = 0;

    if (HAL_TIM_Encoder_Init(&htim_m1_enc, &sEncoder) != HAL_OK) {
        Error_Handler();
    }

    /* ============================================================
     * TIM4: 电机2编码器
     * ============================================================ */
    htim_m2_enc.Instance = TIM4;
    htim_m2_enc.Init.Prescaler = 0;
    htim_m2_enc.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_m2_enc.Init.Period = 0xFFFF;
    htim_m2_enc.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_m2_enc.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Encoder_Init(&htim_m2_enc, &sEncoder) != HAL_OK) {
        Error_Handler();
    }

    /* ============================================================
     * TIM5: 电机3编码器
     * ============================================================ */
    htim_m3_enc.Instance = TIM5;
    htim_m3_enc.Init.Prescaler = 0;
    htim_m3_enc.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_m3_enc.Init.Period = 0xFFFF;
    htim_m3_enc.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_m3_enc.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Encoder_Init(&htim_m3_enc, &sEncoder) != HAL_OK) {
        Error_Handler();
    }

    /* ============================================================
     * TIM8: 电机4编码器
     * ============================================================ */
    htim_m4_enc.Instance = TIM8;
    htim_m4_enc.Init.Prescaler = 0;
    htim_m4_enc.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_m4_enc.Init.Period = 0xFFFF;
    htim_m4_enc.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_m4_enc.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Encoder_Init(&htim_m4_enc, &sEncoder) != HAL_OK) {
        Error_Handler();
    }
}

/* ============================================================
 * UART 初始化 (USART3 → MaixCAM)
 * ============================================================ */
void MX_USART_Init(void)
{
    /* 使能 USART3 时钟 */
    __HAL_RCC_USART3_CLK_ENABLE();

    huart_maix.Instance = USART3;
    huart_maix.Init.BaudRate = MAIX_UART_BAUDRATE;
    huart_maix.Init.WordLength = UART_WORDLENGTH_8B;
    huart_maix.Init.StopBits = UART_STOPBITS_1;
    huart_maix.Init.Parity = UART_PARITY_NONE;
    huart_maix.Init.Mode = UART_MODE_TX_RX;
    huart_maix.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart_maix.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart_maix) != HAL_OK) {
        Error_Handler();
    }

    /* 使能接收中断 */
    __HAL_UART_ENABLE_IT(&huart_maix, UART_IT_RXNE);
}

/* ============================================================
 * I2C 初始化 (I2C1 → OLED SSD1306)
 * ============================================================ */
void MX_I2C_Init(void)
{
    /* 使能 I2C1 时钟 */
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c_oled.Instance = I2C1;
    hi2c_oled.Init.ClockSpeed = 400000;   /* 400kHz 快速模式 */
    hi2c_oled.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c_oled.Init.OwnAddress1 = 0;
    hi2c_oled.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c_oled.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c_oled.Init.OwnAddress2 = 0;
    hi2c_oled.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c_oled.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c_oled) != HAL_OK) {
        Error_Handler();
    }
}

/* ============================================================
 * UART 中断处理 (转发到通信模块)
 * ============================================================ */
void USART3_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart_maix, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart_maix.Instance->DR & 0xFF);
        Communication_RxCallback(byte);
        __HAL_UART_CLEAR_FLAG(&huart_maix, UART_FLAG_RXNE);
    }
    HAL_UART_IRQHandler(&huart_maix);
}

/* ============================================================
 * HAL 定时器 MSP 回调 (GPIO 初始化由 HAL 自动调用)
 * ============================================================ */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        /* 舵机: TIM1_CH1/2/3/4 → PE9/11/13/14 */
        __HAL_RCC_GPIOE_CLK_ENABLE();
        GPIO_InitTypeDef gpio = {0};
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF1_TIM1;

        gpio.Pin = GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
        HAL_GPIO_Init(GPIOE, &gpio);
    }
    else if (htim->Instance == TIM2) {
        /* 电机PWM: TIM2_CH1/2/3/4 → PA0/1/2/3 */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef gpio = {0};
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF1_TIM2;

        gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    if (htim->Instance == TIM3) {
        /* M1编码器: PA6(TIM3_CH1), PA7(TIM3_CH2) */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Alternate = GPIO_AF2_TIM3;
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
    else if (htim->Instance == TIM4) {
        /* M2编码器: PB6(TIM4_CH1), PB7(TIM4_CH2) */
        __HAL_RCC_GPIOB_CLK_ENABLE();
        gpio.Alternate = GPIO_AF2_TIM4;
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOB, &gpio);
    }
    else if (htim->Instance == TIM5) {
        /* M3编码器: PA0(TIM5_CH1), PA1(TIM5_CH2) */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Alternate = GPIO_AF2_TIM5;
        gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
    else if (htim->Instance == TIM8) {
        /* M4编码器: PC6(TIM8_CH1), PC7(TIM8_CH2) */
        __HAL_RCC_GPIOC_CLK_ENABLE();
        gpio.Alternate = GPIO_AF3_TIM8;
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOC, &gpio);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitTypeDef gpio = {0};
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF7_USART3;

        /* TX: PB10, RX: PB11 */
        gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        HAL_GPIO_Init(GPIOB, &gpio);

        /* 使能 USART3 中断 */
        HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitTypeDef gpio = {0};
        gpio.Mode = GPIO_MODE_AF_OD;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF4_I2C1;

        /* SCL: PB8, SDA: PB9 */
        gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        HAL_GPIO_Init(GPIOB, &gpio);
    }
}

/* ============================================================
 * 错误处理
 * ============================================================ */
void Error_Handler(void)
{
    /* 禁用中断 */
    __disable_irq();

    /* 蜂鸣器 SOS 信号 */
    Sensor_BuzzerOn();

    for (;;) {
        /* LED 快闪 */
        HAL_GPIO_TogglePin(SYS_LED_PORT, SYS_LED_PIN);
        for (volatile uint32_t i = 0; i < 1000000; i++) { __NOP(); }
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
