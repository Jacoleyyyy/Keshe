/**
 * @file    task_manager.h
 * @brief   任务管理器 - 系统状态机与 FreeRTOS 任务协调
 * @note    核心调度模块，管理整个搬运流程
 */

#ifndef __TASK_MANAGER_H
#define __TASK_MANAGER_H

#include "cmsis_os2.h"
#include "protocol.h"
#include "communication.h"
#include "chassis.h"

/* ============================================================
 * 全局系统上下文
 * ============================================================ */
typedef struct {
    /* 任务编码 */
    TaskCode_t task_code;

    /* 系统状态 */
    SystemState_t state;
    SystemState_t prev_state;

    /* 搬运进度 */
    uint8_t current_step;       /* 当前是第几步 (0=第1个物料, 1=第2..., 3~5=暂存区搬运) */
    uint8_t total_steps;        /* 总步数=6 (粗加工3次+暂存区3次) */
    bool phase2;                /* true=第二阶段(暂存区码垛) */

    /* 当前目标 */
    MaterialColor_t target_color;
    MaterialPos_t target_material;
    ZoneInfo_t target_zone;

    /* 运动状态 */
    Pose2D_t current_pose;
    char step_name[32];

    /* 运行计时 */
    uint32_t start_tick;
    uint32_t elapsed_ticks;

    /* 错误信息 */
    bool has_error;
    char error_msg[64];

    /* 信号量 */
    osSemaphoreId_t sem_maix_done;      /* MaixCAM响应就绪 */
    osSemaphoreId_t sem_motion_done;    /* 运动完成 */
    osSemaphoreId_t sem_arm_done;       /* 机械臂动作完成 */

    /* 队列 */
    osMessageQueueId_t queue_state;     /* 状态变更队列 */
    osMessageQueueId_t queue_cmd;       /* 命令队列 */

    /* 互斥锁 */
    osMutexId_t mutex_state;            /* 状态访问锁 */
    osMutexId_t mutex_pose;             /* 位姿访问锁 */

} SystemContext_t;

/* 全局上下文 */
extern SystemContext_t g_sys;

/* ============================================================
 * FreeRTOS 任务句柄 (由 main.c 创建)
 * ============================================================ */
extern osThreadId_t g_task_handle_manager;
extern osThreadId_t g_task_handle_chassis;
extern osThreadId_t g_task_handle_arm;
extern osThreadId_t g_task_handle_comm;
extern osThreadId_t g_task_handle_sensor;
extern osThreadId_t g_task_handle_display;
extern osThreadId_t g_task_handle_obstacle;
extern osThreadId_t g_task_handle_monitor;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  系统上下文初始化
 */
void TaskManager_Init(void);

/**
 * @brief  系统启动 (进入主循环)
 */
void TaskManager_Start(void);

/**
 * @brief  主任务入口函数
 */
void Task_Manager(void *argument);

/**
 * @brief  底盘控制任务
 */
void Task_Chassis(void *argument);

/**
 * @brief  机械臂控制任务
 */
void Task_Arm(void *argument);

/**
 * @brief  通信任务
 */
void Task_Communication(void *argument);

/**
 * @brief  传感器读取任务
 */
void Task_Sensor(void *argument);

/**
 * @brief  显示更新任务
 */
void Task_Display(void *argument);

/**
 * @brief  避障监控任务
 */
void Task_Obstacle(void *argument);

/**
 * @brief  系统监控任务 (心跳/看门狗)
 */
void Task_Monitor(void *argument);

/**
 * @brief  执行状态转换
 */
void TaskManager_TransitState(SystemState_t new_state);

/**
 * @brief  获取当前状态
 */
SystemState_t TaskManager_GetState(void);

/**
 * @brief  记录错误
 */
void TaskManager_SetError(const char *msg);

/**
 * @brief  蜂鸣提示
 */
void TaskManager_Beep(uint16_t duration_ms);

#endif /* __TASK_MANAGER_H */
