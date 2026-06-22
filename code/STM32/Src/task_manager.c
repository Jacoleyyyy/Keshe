/**
 * @file    task_manager.c
 * @brief   任务管理器 - 系统状态机与 FreeRTOS 任务实现
 * @note    核心调度逻辑，管理从启动到完成的完整搬运流程
 *
 * 流程:
 *   IDLE → 扫描QR → 按顺序搬运3个物料到粗加工区
 *   → 再从粗加工区搬运到暂存区码垛 → 返回启停区 → 完成
 */

#include "task_manager.h"
#include "chassis.h"
#include "communication.h"
#include "servo.h"
#include "sensor.h"
#include "display.h"
#include "motor.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================
 * 视觉车道检测 — 备选项
 *
 *   设为 1: 启用 MaixCAM 视觉车道检测 (配合灰度传感器, 双重保障)
 *   设为 0: 仅用 5 路灰度传感器
 *
 *   视觉优势: 分辨率高, 可提前预判偏离, 不受地面污渍干扰
 * ============================================================ */
#define USE_VISION_LANE  1

/* 全局系统上下文 */
SystemContext_t g_sys;

/* 视觉车道检测数据 (由 Task_Sensor 更新) */
LaneInfo_t g_lane;

/* FreeRTOS 任务句柄 */
osThreadId_t g_task_handle_manager   = NULL;
osThreadId_t g_task_handle_chassis   = NULL;
osThreadId_t g_task_handle_arm       = NULL;
osThreadId_t g_task_handle_comm      = NULL;
osThreadId_t g_task_handle_sensor    = NULL;
osThreadId_t g_task_handle_display   = NULL;
osThreadId_t g_task_handle_obstacle  = NULL;
osThreadId_t g_task_handle_monitor   = NULL;

/* 内部队列消息 */
typedef struct {
    SystemState_t new_state;
    uint32_t timestamp;
} StateTransition_t;

/* ============================================================
 * 系统初始化
 * ============================================================ */
void TaskManager_Init(void)
{
    memset(&g_sys, 0, sizeof(g_sys));

    g_sys.state = STATE_IDLE;
    g_sys.prev_state = STATE_IDLE;
    g_sys.total_steps = 6;  /* 3次粗加工 + 3次暂存区 */

    /* 创建信号量 */
    g_sys.sem_maix_done = osSemaphoreNew(1, 0, NULL);
    g_sys.sem_motion_done = osSemaphoreNew(1, 0, NULL);
    g_sys.sem_arm_done = osSemaphoreNew(1, 0, NULL);

    /* 创建队列 */
    g_sys.queue_state = osMessageQueueNew(8, sizeof(StateTransition_t), NULL);
    g_sys.queue_cmd = osMessageQueueNew(16, sizeof(uint8_t), NULL);

    /* 创建互斥锁 */
    g_sys.mutex_state = osMutexNew(NULL);
    g_sys.mutex_pose = osMutexNew(NULL);
}

/**
 * @brief  系统启动
 */
void TaskManager_Start(void)
{
    g_sys.start_tick = osKernelGetTickCount();
    g_sys.state = STATE_WAIT_MAIX_READY;

    TaskManager_Beep(100);
}

/**
 * @brief  状态转换
 */
void TaskManager_TransitState(SystemState_t new_state)
{
    osMutexAcquire(g_sys.mutex_state, osWaitForever);
    g_sys.prev_state = g_sys.state;
    g_sys.state = new_state;
    osMutexRelease(g_sys.mutex_state);

    /* 通知状态变更 */
    StateTransition_t trans = { .new_state = new_state, .timestamp = osKernelGetTickCount() };
    osMessageQueuePut(g_sys.queue_state, &trans, 0, 0);
}

/**
 * @brief  获取当前状态
 */
SystemState_t TaskManager_GetState(void)
{
    SystemState_t state;
    osMutexAcquire(g_sys.mutex_state, osWaitForever);
    state = g_sys.state;
    osMutexRelease(g_sys.mutex_state);
    return state;
}

/**
 * @brief  记录错误
 */
void TaskManager_SetError(const char *msg)
{
    g_sys.has_error = true;
    strncpy(g_sys.error_msg, msg, sizeof(g_sys.error_msg) - 1);
    TaskManager_TransitState(STATE_ERROR);
}

/**
 * @brief  蜂鸣提示
 */
void TaskManager_Beep(uint16_t duration_ms)
{
    Sensor_BuzzerBeep(duration_ms);
}

/* ============================================================
 * 颜色名称
 * ============================================================ */
static const char* color_name(MaterialColor_t c)
{
    switch (c) {
        case COLOR_RED:   return "RED";
        case COLOR_GREEN: return "GREEN";
        case COLOR_BLUE:  return "BLUE";
        default:          return "???";
    }
}

/* ============================================================
 * 主任务 — 任务管理器
 *
 * 核心状态机，驱动整个竞赛流程。
 *
 * 步骤分解:
 *   Step 0: 扫描二维码
 *   Step 1: 搬运物料1到粗加工区 (抓取→运输→放置)
 *   Step 2: 搬运物料2到粗加工区
 *   Step 3: 搬运物料3到粗加工区
 *   Step 4: 搬运物料1到暂存区码垛 (从粗加工区拾取→运输→码垛)
 *   Step 5: 搬运物料2到暂存区码垛
 *   Step 6: 搬运物料3到暂存区码垛
 *   Step 7: 返回启停区
 * ============================================================ */
/* ============================================================
 * 带车道保持的导航辅助函数
 *
 * 在长途行驶时使用视觉+灰度双重车道保持,
 * 接近目标 (<300mm) 时切换到纯点对点导航
 * ============================================================ */
static bool NavigateWithLane(float tx, float ty, float tyaw, float speed)
{
    /* 先判断距离 */
    Pose2D_t pose = Chassis_GetPose();
    float dx = tx - pose.x_mm;
    float dy = ty - pose.y_mm;
    float dist = sqrtf(dx * dx + dy * dy);

    /* 近距离: 纯点对点导航 (不依赖车道线) */
    if (dist < 300.0f) {
        return Chassis_NavigateTo(tx, ty, tyaw, speed);
    }

    /* 长途: 使用混合车道跟随 + 导航引导 */
    float target_angle = atan2f(dy, dx) * 180.0f / (float)M_PI;
    float angle_err = target_angle - pose.yaw_deg;
    while (angle_err > 180.0f) angle_err -= 360.0f;
    while (angle_err < -180.0f) angle_err += 360.0f;

    /* 方向偏离大时先转弯 */
    if (fabsf(angle_err) > 30.0f) {
        float rot = (angle_err > 0) ? 45.0f : -45.0f;
        Chassis_SetVelocity(0, 0, rot);
        return false;
    }

    /* 使用混合车道跟随 (视觉优先, 灰度降级) */
    LaneInfo_t lane;
    osMutexAcquire(g_sys.mutex_pose, osWaitForever);
    lane = g_lane;
    osMutexRelease(g_sys.mutex_pose);

    float effective_speed = speed;

    /* 大偏移时自动减速 */
    if (lane.on_lane && abs(lane.offset_mm) > LANE_WARN_OFFSET_MM)
        effective_speed = speed * 0.5f;

    /* 角度引导修正: 让纵向速度向目标方向倾斜 */
    float vx_nav = sinf(angle_err * (float)M_PI / 180.0f) * effective_speed * 0.3f;

#if USE_VISION_LANE
    if (lane.on_lane) {
        /* 车道修正: 与导航引导合并 */
        float vx_lane = -(float)lane.offset_mm * 2.5f;
        if (vx_lane > 120.0f) vx_lane = 120.0f;
        if (vx_lane < -120.0f) vx_lane = -120.0f;
        /* 视觉权重 0.7, 导航权重 0.3 */
        Chassis_SetVelocity(vx_lane * 0.7f + vx_nav * 0.3f,
                            effective_speed,
                            -(float)lane.offset_mm * 0.8f);
    } else
#endif
    {
        /* 视觉不可用: 纯导航 */
        Chassis_SetVelocity(vx_nav, effective_speed, angle_err * 2.0f);
    }

    return false;
}

/* ============================================================
 * 主任务
 * ============================================================ */
void Task_Manager(void *argument)
{
    (void)argument;

    /* 等待系统就绪 */
    osDelay(1000);

    for (;;) {
        SystemState_t state = TaskManager_GetState();

        switch (state) {

        /* ================================================
         * IDLE: 等待启动按钮
         * ================================================ */
        case STATE_IDLE:
            /* 等待按钮按下 */
            while (!Sensor_IsStartButtonPressed()) {
                osDelay(10);
            }
            TaskManager_Start();
            break;

        /* ================================================
         * WAIT_MAIX_READY: 等待视觉单元就绪
         * ================================================ */
        case STATE_WAIT_MAIX_READY:
            {
                CommError_t err = Communication_WaitReady(10000);
                if (err == COMM_OK) {
                    TaskManager_TransitState(STATE_MOVE_TO_QR);
                } else {
                    TaskManager_SetError("MaixCAM no response");
                }
            }
            break;

        /* ================================================
         * MOVE_TO_QR: 移动到二维码板
         * ================================================ */
        case STATE_MOVE_TO_QR:
            {
                /* 设置机械臂到扫描姿态 */
                osMessageQueuePut(g_sys.queue_cmd, &(uint8_t){CMD_SCAN_QR}, 0, 0);

                /* 导航到二维码位置 */
                bool arrived = false;
                while (!arrived) {
                    arrived = NavigateWithLane(QR_X_MM, QR_Y_MM, 0.0f, DEFAULT_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                osDelay(500);  /* 稳定 */
                TaskManager_TransitState(STATE_SCAN_QR);
            }
            break;

        /* ================================================
         * SCAN_QR: 读取二维码
         * ================================================ */
        case STATE_SCAN_QR:
            {
                CommError_t err = Communication_ScanQR(&g_sys.task_code, 5000);
                if (err == COMM_OK && g_sys.task_code.valid) {
                    TaskManager_Beep(200);
                    TaskManager_TransitState(STATE_PROCESS_QR);
                } else {
                    /* 重试一次 */
                    osDelay(1000);
                    err = Communication_ScanQR(&g_sys.task_code, 5000);
                    if (err == COMM_OK && g_sys.task_code.valid) {
                        TaskManager_Beep(200);
                        TaskManager_TransitState(STATE_PROCESS_QR);
                    } else {
                        TaskManager_SetError("QR Read Failed");
                    }
                }
            }
            break;

        /* ================================================
         * PROCESS_QR: 处理任务编码，开始搬运循环
         * ================================================ */
        case STATE_PROCESS_QR:
            {
                g_sys.current_step = 0;
                g_sys.phase2 = false;
                g_sys.target_color = (MaterialColor_t)g_sys.task_code.colors[0];
                TaskManager_TransitState(STATE_MOVE_TO_RAW);
            }
            break;

        /* ================================================
         * MOVE_TO_RAW: 移动到原料区
         * ================================================ */
        case STATE_MOVE_TO_RAW:
            {
                bool arrived = false;
                while (!arrived) {
                    arrived = NavigateWithLane(RAW_AREA_X_MM, RAW_AREA_Y_MM, 0.0f, DEFAULT_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                TaskManager_TransitState(STATE_FIND_MATERIAL);
            }
            break;

        /* ================================================
         * FIND_MATERIAL: 寻找目标颜色物料
         * ================================================ */
        case STATE_FIND_MATERIAL:
            {
                CommError_t err = Communication_FindMaterial(
                    g_sys.target_color, &g_sys.target_material, 8000);
                if (err == COMM_OK) {
                    TaskManager_TransitState(STATE_APPROACH_MATERIAL);
                } else {
                    TaskManager_SetError("Material not found");
                }
            }
            break;

        /* ================================================
         * APPROACH_MATERIAL: 接近物料
         * ================================================ */
        case STATE_APPROACH_MATERIAL:
            {
                /* 根据视觉返回的位置微调机器人 */
                int16_t dx = g_sys.target_material.x_mm;
                int16_t dy = g_sys.target_material.y_mm;

                /* 计算全局目标位置 */
                Pose2D_t pose = Chassis_GetPose();
                float target_x = pose.x_mm + (float)dx;
                float target_y = pose.y_mm + (float)dy;

                bool arrived = false;
                while (!arrived) {
                    arrived = Chassis_NavigateTo(target_x, target_y, 0.0f, APPROACH_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                osDelay(300);
                TaskManager_TransitState(STATE_PICK_MATERIAL);
            }
            break;

        /* ================================================
         * PICK_MATERIAL: 抓取物料
         * ================================================ */
        case STATE_PICK_MATERIAL:
            {
                /* 发送机械臂抓取序列 */
                /* 1. 预抓取姿态 */
                uint8_t cmd1 = 0xA1;  /* ARM_POSE_PRE_PICK */
                osMessageQueuePut(g_sys.queue_cmd, &cmd1, 0, osWaitForever);
                osDelay(1500);

                /* 2. 抓取 */
                uint8_t cmd2 = 0xA2;  /* ARM_POSE_PICK */
                osMessageQueuePut(g_sys.queue_cmd, &cmd2, 0, osWaitForever);
                osDelay(1000);

                /* 3. 运输姿态 */
                uint8_t cmd3 = 0xA3;  /* ARM_POSE_CARRY */
                osMessageQueuePut(g_sys.queue_cmd, &cmd3, 0, osWaitForever);
                osDelay(800);

                TaskManager_TransitState(STATE_TRANSPORT_TO_ROUGH);
            }
            break;

        /* ================================================
         * TRANSPORT_TO_ROUGH: 运输到粗加工区
         * ================================================ */
        case STATE_TRANSPORT_TO_ROUGH:
            {
                /* 粗加工区X固定，Y根据颜色偏移 */
                float zone_x = ROUGH_AREA_X_MM;
                float zone_y = ROUGH_AREA_Y_MM;

                /* 3个颜色区域并排 */
                switch (g_sys.target_color) {
                    case COLOR_RED:   zone_x -= ROUGH_ZONE_SPACING_MM; break;
                    case COLOR_GREEN: zone_x += 0;                      break;
                    case COLOR_BLUE:  zone_x += ROUGH_ZONE_SPACING_MM; break;
                    default: break;
                }

                bool arrived = false;
                while (!arrived) {
                    arrived = NavigateWithLane(zone_x, zone_y, 0.0f, DEFAULT_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                osDelay(300);
                TaskManager_TransitState(STATE_APPROACH_ROUGH);
            }
            break;

        /* ================================================
         * APPROACH_ROUGH: 接近粗加工区精确位置
         * ================================================ */
        case STATE_APPROACH_ROUGH:
            {
                /* 使用视觉确认色环位置 */
                ZoneInfo_t zone;
                CommError_t err = Communication_CheckZone(g_sys.target_color, &zone, 5000);
                if (err == COMM_OK) {
                    /* 微调到视觉确认的位置 */
                    Pose2D_t pose = Chassis_GetPose();
                    bool arrived = false;
                    while (!arrived) {
                        arrived = Chassis_NavigateTo(
                            pose.x_mm + (float)zone.x_mm,
                            pose.y_mm + (float)zone.y_mm,
                            0.0f, APPROACH_SPEED_MM_S * 0.5f);
                        Chassis_UpdateOdometry();
                        osDelay(10);
                    }
                }
                TaskManager_TransitState(STATE_PLACE_ROUGH);
            }
            break;

        /* ================================================
         * PLACE_ROUGH: 在粗加工区放置物料 (精确到色环)
         * ================================================ */
        case STATE_PLACE_ROUGH:
            {
                /* 机械臂放置序列 */
                /* 1. 放置姿态 */
                uint8_t cmd1 = 0xA4;  /* ARM_POSE_PLACE */
                osMessageQueuePut(g_sys.queue_cmd, &cmd1, 0, osWaitForever);
                osDelay(1200);

                /* 2. 张开手爪释放 */
                uint8_t cmd2 = 0xA5;  /* 张开 */
                osMessageQueuePut(g_sys.queue_cmd, &cmd2, 0, osWaitForever);
                osDelay(500);

                /* 3. 回到行驶姿态 */
                uint8_t cmd3 = 0xA0;  /* ARM_POSE_IDLE */
                osMessageQueuePut(g_sys.queue_cmd, &cmd3, 0, osWaitForever);
                osDelay(500);
                TaskManager_Beep(50);

                /* 判断是否进入第二阶段 */
                g_sys.current_step++;
                if (g_sys.current_step >= 3) {
                    /* 粗加工区3个已放完，进入暂存区码垛阶段 */
                    g_sys.phase2 = true;
                    g_sys.current_step = 3; /* 重置为第二阶段的开始 */
                    TaskManager_TransitState(STATE_MOVE_TO_ROUGH_PICK);
                } else {
                    /* 下一个物料 */
                    g_sys.target_color = (MaterialColor_t)g_sys.task_code.colors[g_sys.current_step];
                    TaskManager_TransitState(STATE_MOVE_TO_RAW);
                }
            }
            break;

        /* ================================================
         * MOVE_TO_ROUGH_PICK: 移动到粗加工区取回物料 (第二阶段)
         * ================================================ */
        case STATE_MOVE_TO_ROUGH_PICK:
            {
                /* 确定目标颜色: 第二阶段按相同顺序 */
                uint8_t step_idx = g_sys.current_step - 3; /* 0,1,2 */
                g_sys.target_color = (MaterialColor_t)g_sys.task_code.colors[step_idx];

                /* 移动到粗加工区对应颜色区域 */
                float zone_x = ROUGH_AREA_X_MM;
                float zone_y = ROUGH_AREA_Y_MM;
                switch (g_sys.target_color) {
                    case COLOR_RED:   zone_x -= ROUGH_ZONE_SPACING_MM; break;
                    case COLOR_GREEN: zone_x += 0;                      break;
                    case COLOR_BLUE:  zone_x += ROUGH_ZONE_SPACING_MM; break;
                    default: break;
                }

                bool arrived = false;
                while (!arrived) {
                    arrived = NavigateWithLane(zone_x, zone_y, 0.0f, DEFAULT_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                osDelay(300);
                TaskManager_TransitState(STATE_PICK_FROM_ROUGH);
            }
            break;

        /* ================================================
         * PICK_FROM_ROUGH: 从粗加工区拾取物料
         * ================================================ */
        case STATE_PICK_FROM_ROUGH:
            {
                /* 视觉确认物料位置 */
                CommError_t err = Communication_FindMaterial(
                    g_sys.target_color, &g_sys.target_material, 5000);
                if (err != COMM_OK) {
                    /* 如果找不到，可能是视觉盲区，尝试直接拾取 */
                    osDelay(500);
                }

                /* 抓取序列 */
                uint8_t cmd1 = 0xA1;  /* ARM_POSE_PRE_PICK */
                osMessageQueuePut(g_sys.queue_cmd, &cmd1, 0, osWaitForever);
                osDelay(1500);

                uint8_t cmd2 = 0xA2;  /* ARM_POSE_PICK - 闭合手爪 */
                osMessageQueuePut(g_sys.queue_cmd, &cmd2, 0, osWaitForever);
                osDelay(1000);

                uint8_t cmd3 = 0xA3;  /* ARM_POSE_CARRY */
                osMessageQueuePut(g_sys.queue_cmd, &cmd3, 0, osWaitForever);
                osDelay(800);

                TaskManager_TransitState(STATE_TRANSPORT_TO_TEMP);
            }
            break;

        /* ================================================
         * TRANSPORT_TO_TEMP: 运输到暂存区
         * ================================================ */
        case STATE_TRANSPORT_TO_TEMP:
            {
                bool arrived = false;
                while (!arrived) {
                    arrived = NavigateWithLane(TEMP_AREA_X_MM, TEMP_AREA_Y_MM, 0.0f, DEFAULT_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                osDelay(300);
                TaskManager_TransitState(STATE_APPROACH_TEMP);
            }
            break;

        /* ================================================
         * APPROACH_TEMP: 接近暂存区码垛位置
         * ================================================ */
        case STATE_APPROACH_TEMP:
            {
                /* 视觉辅助确认码垛位置 */
                ZoneInfo_t zone;
                CommError_t err = Communication_CheckZone(g_sys.target_color, &zone, 5000);
                if (err == COMM_OK) {
                    Pose2D_t pose = Chassis_GetPose();
                    bool arrived = false;
                    while (!arrived) {
                        arrived = Chassis_NavigateTo(
                            pose.x_mm + (float)zone.x_mm,
                            pose.y_mm + (float)zone.y_mm,
                            0.0f, APPROACH_SPEED_MM_S * 0.5f);
                        Chassis_UpdateOdometry();
                        osDelay(10);
                    }
                }
                TaskManager_TransitState(STATE_STACK_TEMP);
            }
            break;

        /* ================================================
         * STACK_TEMP: 在暂存区码垛 (堆叠在已有物料上)
         *
         * 码垛要求:
         *   - 只能堆叠在已有物料上
         *   - 第1个物料直接放置
         *   - 第2个物料叠在第1个上
         *   - 第3个物料叠在第2个上
         *
         * 实现: 根据 current_step 调整放置高度
         * ================================================ */
        case STATE_STACK_TEMP:
            {
                uint8_t stack_level = g_sys.current_step - 3; /* 0,1,2 */

                /* 码垛姿态 (随层数抬高) */
                /* 第一层直接放, 第二三层抬高大臂角度 */
                uint8_t cmd_stack = 0xA6;  /* ARM_POSE_STACK */
                osMessageQueuePut(g_sys.queue_cmd, &cmd_stack, 0, osWaitForever);
                osDelay(1200);

                /* 根据层数调整放置高度 */
                if (stack_level >= 1) {
                    /* 抬高机械臂以堆叠 */
                    /* 发送特殊指令: 基础码垛 + 层数偏移 */
                    uint8_t cmd_adjust = 0xA6 + stack_level;  /* 预留码垛层数指令 */
                    osMessageQueuePut(g_sys.queue_cmd, &cmd_adjust, 0, osWaitForever);
                    osDelay(800);
                }

                /* 释放手爪 */
                uint8_t cmd_release = 0xA5;  /* 张开 */
                osMessageQueuePut(g_sys.queue_cmd, &cmd_release, 0, osWaitForever);
                osDelay(500);

                /* 回到行驶姿态 */
                uint8_t cmd_idle = 0xA0;  /* ARM_POSE_IDLE */
                osMessageQueuePut(g_sys.queue_cmd, &cmd_idle, 0, osWaitForever);
                osDelay(500);
                TaskManager_Beep(50);

                /* 判断完成 */
                g_sys.current_step++;
                if (g_sys.current_step >= 6) {
                    /* 全部6步完成 */
                    TaskManager_TransitState(STATE_RETURN_START);
                } else {
                    /* 下一个物料 */
                    TaskManager_TransitState(STATE_MOVE_TO_ROUGH_PICK);
                }
            }
            break;

        /* ================================================
         * RETURN_START: 返回蓝色启停区
         * ================================================ */
        case STATE_RETURN_START:
            {
                /* 回到坐标原点 (启停区中心) */
                bool arrived = false;
                while (!arrived) {
                    arrived = NavigateWithLane(0.0f, 0.0f, 0.0f, DEFAULT_SPEED_MM_S);
                    Chassis_UpdateOdometry();
                    osDelay(10);
                }
                osDelay(500);
                TaskManager_TransitState(STATE_COMPLETE);
            }
            break;

        /* ================================================
         * COMPLETE: 任务完成
         * ================================================ */
        case STATE_COMPLETE:
            {
                /* 停止所有运动 */
                Chassis_Stop();
                Servo_SetArmPose(ARM_POSE_IDLE);

                /* 记录时间 */
                g_sys.elapsed_ticks = osKernelGetTickCount() - g_sys.start_tick;

                /* 长蜂鸣表示完成 */
                Sensor_BuzzerOn();
                osDelay(1000);
                Sensor_BuzzerOff();

                /* 无限等待 (任务完成, 不再动作) */
                for (;;) {
                    osDelay(1000);
                }
            }
            break;

        /* ================================================
         * ERROR: 错误状态
         * ================================================ */
        case STATE_ERROR:
            {
                Chassis_EmergencyStop();
                Sensor_BuzzerOn();
                osDelay(300);
                Sensor_BuzzerOff();
                osDelay(200);
                Sensor_BuzzerOn();
                osDelay(300);
                Sensor_BuzzerOff();

                /* 错误后保持停止 */
                for (;;) {
                    osDelay(1000);
                }
            }
            break;

        default:
            break;
        }

        /* 主循环周期: 10ms */
        osDelay(10);
    }
}

/* ============================================================
 * 底盘控制任务
 *
 * 周期执行:
 *   - PID 速度调节 (1kHz)
 *   - 里程计更新
 *   - 避障响应
 * ============================================================ */
void Task_Chassis(void *argument)
{
    (void)argument;

    uint32_t last_wake = osKernelGetTickCount();

    for (;;) {
        /* PID 速度控制 (1ms周期) */
        Motor_PIDUpdate();

        /* 里程计更新 */
        Chassis_UpdateOdometry();

        /* 精确延时到下一个1ms */
        osDelayUntil(last_wake + 1);
        last_wake = osKernelGetTickCount();
    }
}

/* ============================================================
 * 机械臂控制任务
 *
 * 接收命令队列，执行机械臂动作序列
 * 命令格式:
 *   0xA0: 空闲姿态
 *   0xA1: 预抓取姿态
 *   0xA2: 抓取姿态
 *   0xA3: 运输姿态
 *   0xA4: 放置姿态
 *   0xA5: 张开手爪
 *   0xA6: 码垛姿态
 *   0xA7: 码垛姿态+1层 (第2层)
 *   0xA8: 码垛姿态+2层 (第3层)
 * ============================================================ */
void Task_Arm(void *argument)
{
    (void)argument;

    for (;;) {
        uint8_t cmd;
        if (osMessageQueueGet(g_sys.queue_cmd, &cmd, NULL, osWaitForever) == osOK) {
            switch (cmd) {
                case 0xA0:
                    Servo_SetArmPose(ARM_POSE_IDLE);
                    break;
                case 0xA1:
                    Servo_SetArmPose(ARM_POSE_PRE_PICK);
                    Servo_GripperSet(true);  /* 张开手爪 */
                    break;
                case 0xA2:
                    Servo_SetArmPose(ARM_POSE_PICK);
                    Servo_GripperSet(false); /* 闭合手爪 */
                    break;
                case 0xA3:
                    Servo_SetArmPose(ARM_POSE_CARRY);
                    break;
                case 0xA4:
                    Servo_SetArmPose(ARM_POSE_PLACE);
                    break;
                case 0xA5:
                    Servo_GripperSet(true);  /* 张开手爪释放 */
                    break;
                case 0xA6:
                    /* 基础码垛姿态 */
                    Servo_SetArmPose(ARM_POSE_STACK);
                    break;
                case 0xA7:
                    /* 码垛第2层: 在码垛姿态基础上升高大臂10度 */
                    Servo_SetArmPose(ARM_POSE_STACK);
                    Servo_SetAngle(SERVO_SHOULDER,
                        Servo_GetAngle(SERVO_SHOULDER) + 10);
                    break;
                case 0xA8:
                    /* 码垛第3层: 在码垛姿态基础上升高大臂20度 */
                    Servo_SetArmPose(ARM_POSE_STACK);
                    Servo_SetAngle(SERVO_SHOULDER,
                        Servo_GetAngle(SERVO_SHOULDER) + 20);
                    break;
                default:
                    break;
            }

            /* 通知主任务动作完成 */
            osSemaphoreRelease(g_sys.sem_arm_done);
        }
    }
}

/* ============================================================
 * 通信任务
 *
 * 管理与 MaixCAM 的所有 UART 通信
 * ============================================================ */
void Task_Communication(void *argument)
{
    (void)argument;

    for (;;) {
        /* 心跳检测 */
        if (!g_frame_ready) {
            osDelay(200);
        }

        /* 定期发送心跳 */
        static uint32_t last_heartbeat = 0;
        uint32_t now = osKernelGetTickCount();
        if ((now - last_heartbeat) > 1000) {
            Communication_SendCommand(CMD_HEARTBEAT);
            last_heartbeat = now;
        }

        osDelay(50);
    }
}

/* ============================================================
 * 传感器任务
 *
 * 周期读取:
 *   - 灰度传感器 (车道线)
 *   - 超声波 (避障)
 *   - 按钮状态
 * ============================================================ */
void Task_Sensor(void *argument)
{
    (void)argument;

    GraySensor_t gray;
    Ultrasonic_t us;
    uint32_t last_lane_query = 0;

    for (;;) {
        /* 读取灰度传感器 */
        Sensor_ReadGray(&gray);

        /* 触发超声波测量 */
        Sensor_TriggerUltrasonic();

        /* 获取超声波数据 */
        us = Sensor_GetUltrasonicData();

        /* 如果检测到障碍物，发布紧急消息 */
        if (us.obstacle_detected && us.valid) {
            /* 发布障碍物告警 */
            StateTransition_t alert = {
                .new_state = TaskManager_GetState(),
                .timestamp = osKernelGetTickCount()
            };
            /* 高优先级告警 */
        }

#if USE_VISION_LANE
        /* 视觉车道检测 (5Hz = 每200ms查询一次) */
        uint32_t now = osKernelGetTickCount();
        if ((now - last_lane_query) >= 200) {
            SystemState_t st = TaskManager_GetState();
            /* 仅在运动状态时查询, 避免干扰其他命令 */
            if (st >= STATE_MOVE_TO_QR && st <= STATE_RETURN_START) {
                LaneInfo_t lane;
                CommError_t err = Communication_CheckLane(&lane, 50);
                if (err == COMM_OK) {
                    osMutexAcquire(g_sys.mutex_pose, osWaitForever);
                    g_lane = lane;
                    osMutexRelease(g_sys.mutex_pose);
                } else {
                    /* 通信失败: 标记掉线 */
                    osMutexAcquire(g_sys.mutex_pose, osWaitForever);
                    g_lane.on_lane = false;
                    g_lane.offset_mm = 0;
                    osMutexRelease(g_sys.mutex_pose);
                }
            }
            last_lane_query = now;
        }
#endif

        osDelay(20);  /* 50Hz 更新率 */
    }
}

/* ============================================================
 * 显示更新任务
 *
 * 周期刷新 OLED 显示
 * ============================================================ */
void Task_Display(void *argument)
{
    (void)argument;

    DisplayInfo_t info;

    for (;;) {
        /* 收集当前信息 */
        info.state = TaskManager_GetState();
        info.task_code = g_sys.task_code;
        info.step_index = g_sys.current_step;

        /* 进度百分比 */
        info.task_progress = (uint8_t)(g_sys.current_step * 100 / g_sys.total_steps);

        /* 当前目标颜色 */
        info.current_target = g_sys.target_color;

        /* 状态消息 */
        if (g_sys.has_error) {
            snprintf(info.status_msg, sizeof(info.status_msg), "ERR:%s", g_sys.error_msg);
        } else if (g_sys.state == STATE_COMPLETE) {
            snprintf(info.status_msg, sizeof(info.status_msg), "COMPLETE!");
        } else {
            snprintf(info.status_msg, sizeof(info.status_msg),
                     "%s %s", g_sys.phase2 ? "[P2]" : "[P1]", color_name(g_sys.target_color));
        }

        info.elapsed_time_s = (osKernelGetTickCount() - g_sys.start_tick) / 1000;
        info.is_complete = (g_sys.state == STATE_COMPLETE);

        /* 更新显示 */
        Display_Update(&info);

        osDelay(500);  /* 2Hz 刷新率 */
    }
}

/* ============================================================
 * 避障监控任务
 *
 * 高优先级任务, 监控超声波并紧急避障
 * ============================================================ */
void Task_Obstacle(void *argument)
{
    (void)argument;

    for (;;) {
        Ultrasonic_t us = Sensor_GetUltrasonicData();

        if (us.obstacle_detected && us.valid) {
            SystemState_t current_state = TaskManager_GetState();

            /* 仅在行驶状态时避障 */
            if (current_state >= STATE_MOVE_TO_QR &&
                current_state <= STATE_RETURN_START) {
                /* 紧急停止, 等待障碍物清除 */
                Chassis_EmergencyStop();

                /* 避障逻辑: 横向移动绕过 */
                osDelay(500);

                /* 尝试绕过: 左移一段 */
                Chassis_SetMotion(CHASSIS_MOVE_LEFT, 100.0f);
                osDelay(1000);
                Chassis_Stop();

                /* 重新测量 */
                Sensor_TriggerUltrasonic();
                osDelay(200);
                us = Sensor_GetUltrasonicData();

                if (!us.obstacle_detected) {
                    /* 路径已通畅, 恢复 */
                    Chassis_Stop();
                }
            }
        }

        osDelay(50);  /* 20Hz */
    }
}

/* ============================================================
 * 系统监控任务
 *
 * 低优先级任务:
 *   - LED 心跳闪烁
 *   - 系统健康检查
 *   - 调试输出
 * ============================================================ */
void Task_Monitor(void *argument)
{
    (void)argument;

    bool led_state = false;

    for (;;) {
        /* LED 心跳 */
        led_state = !led_state;
        HAL_GPIO_WritePin(SYS_LED_PORT, SYS_LED_PIN,
                          led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);

        /* 通信健康检查 */
        if (!Communication_IsHealthy()) {
            /* 通信丢失 - 记录但不停止 */
        }

        /* 检查任务栈水位 (调试用) */
        #if (configCHECK_FOR_STACK_OVERFLOW > 0)
        {
            UBaseType_t stack_free;
            stack_free = uxTaskGetStackHighWaterMark(g_task_handle_manager);
            /* 可以在调试时将 stack_free 输出 */
            (void)stack_free;
        }
        #endif

        osDelay(500);
    }
}
