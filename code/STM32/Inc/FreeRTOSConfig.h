/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS 配置文件 (STM32F407, CMSIS OS v2 wrapper)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* 引入 CMSIS-FreeRTOS 默认配置 */
#include "FreeRTOSConfig_template.h"

/* ============================================================
 * 硬件相关
 * ============================================================ */
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      ( (TickType_t) 1000 )
#define configMAX_PRIORITIES                    ( 16 )
#define configMINIMAL_STACK_SIZE                ( (uint16_t) 256 )
#define configTOTAL_HEAP_SIZE                   ( (size_t) (48 * 1024) )
#define configMAX_TASK_NAME_LEN                 ( 24 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* ============================================================
 * 调度选项
 * ============================================================ */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0

/* ============================================================
 * 同步与通信
 * ============================================================ */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    1
#define configUSE_TASK_NOTIFICATIONS            1

/* 软件定时器 */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( 256 )

/* ============================================================
 * 内存管理
 * ============================================================ */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1

/* ============================================================
 * Hook 函数
 * ============================================================ */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configCHECK_FOR_STACK_OVERFLOW          2

/* ============================================================
 * 运行时统计 (调试用)
 * ============================================================ */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ============================================================
 * 断言
 * ============================================================ */
#define configASSERT( x ) do { if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); } } while( 0 )

/* ============================================================
 * 可选功能
 * ============================================================ */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetIdleTaskHandle           1
#define INCLUDE_eTaskGetState                    1
#define INCLUDE_xTimerPendFunctionCall           1
#define INCLUDE_xQueueGetMutexHolder             1

/* ============================================================
 * 中断优先级
 * NVIC 优先级分组: 4位抢占优先级
 * 可被 FreeRTOS API 调用的中断优先级范围: 5~15
 * ============================================================ */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY          ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* ============================================================
 * 应用任务优先级
 * ============================================================ */
#define TASK_PRIORITY_IDLE              0
#define TASK_PRIORITY_LOW               1
#define TASK_PRIORITY_NORMAL            3
#define TASK_PRIORITY_HIGH              5
#define TASK_PRIORITY_CRITICAL          8

/* 具体任务优先级 */
#define PRIO_TASK_MANAGER       TASK_PRIORITY_HIGH
#define PRIO_TASK_CHASSIS       TASK_PRIORITY_CRITICAL
#define PRIO_TASK_ARM           TASK_PRIORITY_NORMAL
#define PRIO_TASK_COMM          TASK_PRIORITY_HIGH
#define PRIO_TASK_SENSOR        TASK_PRIORITY_NORMAL
#define PRIO_TASK_DISPLAY       TASK_PRIORITY_LOW
#define PRIO_TASK_OBSTACLE      TASK_PRIORITY_HIGH
#define PRIO_TASK_MONITOR       TASK_PRIORITY_LOW

/* ============================================================
 * 任务栈大小
 * ============================================================ */
#define STACK_TASK_MANAGER      512
#define STACK_TASK_CHASSIS      256
#define STACK_TASK_ARM          256
#define STACK_TASK_COMM         512
#define STACK_TASK_SENSOR       256
#define STACK_TASK_DISPLAY      512
#define STACK_TASK_OBSTACLE     256
#define STACK_TASK_MONITOR      256

#endif /* FREERTOS_CONFIG_H */
