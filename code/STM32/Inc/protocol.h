/**
 * @file    protocol.h
 * @brief   STM32 <-> MaixCAM UART 通信协议定义
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 帧格式
 * STM32 → MaixCAM:  "CMD:<command>\r\n"
 * MaixCAM → STM32:  "RSP:<type>,<data...>\r\n"
 * ============================================================ */

#define PROTOCOL_DELIMITER      "\r\n"
#define PROTOCOL_CMD_PREFIX     "CMD:"
#define PROTOCOL_RSP_PREFIX     "RSP:"

/* 最大帧长度 */
#define PROTOCOL_FRAME_MAX_LEN  128

/* ============================================================
 * STM32 → MaixCAM 命令
 * ============================================================ */
typedef enum {
    CMD_SCAN_QR        = 0x01,  // 扫描二维码
    CMD_DETECT_COLOR   = 0x02,  // 颜色识别
    CMD_FIND_MATERIAL  = 0x03,  // 寻找物料位置
    CMD_CHECK_ZONE     = 0x04,  // 检查放置区域
    CMD_HEARTBEAT      = 0x05,  // 心跳包
    CMD_READY          = 0x06,  // 查询 MaixCAM 就绪状态
} MaixCommand_t;

/* 命令字符串表 */
static const char* MAIX_CMD_STR[] = {
    [CMD_SCAN_QR]       = "SCAN_QR",
    [CMD_DETECT_COLOR]  = "DETECT_COLOR",
    [CMD_FIND_MATERIAL] = "FIND_MATERIAL",
    [CMD_CHECK_ZONE]    = "CHECK_ZONE",
    [CMD_HEARTBEAT]     = "HEARTBEAT",
    [CMD_READY]         = "READY",
};

/* ============================================================
 * MaixCAM → STM32 响应
 * ============================================================ */
typedef enum {
    RSP_QR_CODE      = 0x10,  // QR码内容: "QR,<code>"
    RSP_COLOR_RESULT = 0x11,  // 颜色结果: "COLOR,<r>,<g>,<b>,<name>"
    RSP_MAT_POS      = 0x12,  // 物料位置: "MAT,<x>,<y>,<color>,<angle>"
    RSP_ZONE_INFO    = 0x13,  // 区域信息: "ZONE,<x>,<y>,<color>"
    RSP_ACK          = 0x14,  // 确认: "ACK"
    RSP_ERROR        = 0x15,  // 错误: "ERR,<msg>"
    RSP_READY_OK     = 0x16,  // 就绪: "READY"
    RSP_DONE         = 0x17,  // 完成: "DONE"
} MaixResponse_t;

/* 响应字符串表 */
static const char* MAIX_RSP_STR[] = {
    [RSP_QR_CODE - 0x10]      = "QR",
    [RSP_COLOR_RESULT - 0x10] = "COLOR",
    [RSP_MAT_POS - 0x10]      = "MAT",
    [RSP_ZONE_INFO - 0x10]    = "ZONE",
    [RSP_ACK - 0x10]          = "ACK",
    [RSP_ERROR - 0x10]        = "ERR",
    [RSP_READY_OK - 0x10]     = "READY",
    [RSP_DONE - 0x10]         = "DONE",
};

/* ============================================================
 * 数据结构
 * ============================================================ */

/* 任务编码 (三位数字) */
typedef struct {
    uint8_t digits[3];      // 三位数字，digits[0]=第一位
    uint8_t colors[3];      // 对应颜色: 1=红, 2=绿, 3=蓝
    bool valid;
} TaskCode_t;

/* 物料颜色枚举 */
typedef enum {
    COLOR_UNKNOWN = 0,
    COLOR_RED     = 1,
    COLOR_GREEN   = 2,
    COLOR_BLUE    = 3,
} MaterialColor_t;

/* 物料位置信息 */
typedef struct {
    int16_t x_mm;           // 相对于相机的X坐标 (mm)
    int16_t y_mm;           // 相对于相机的Y坐标 (mm)
    int16_t angle_deg;      // 物料角度
    MaterialColor_t color;
} MaterialPos_t;

/* 区域信息 */
typedef struct {
    int16_t x_mm;
    int16_t y_mm;
    MaterialColor_t color;
} ZoneInfo_t;

/* 系统状态 */
typedef enum {
    STATE_IDLE = 0,
    STATE_WAIT_MAIX_READY,
    STATE_MOVE_TO_QR,
    STATE_SCAN_QR,
    STATE_PROCESS_QR,
    STATE_MOVE_TO_RAW,
    STATE_FIND_MATERIAL,
    STATE_APPROACH_MATERIAL,
    STATE_PICK_MATERIAL,
    STATE_TRANSPORT_TO_ROUGH,
    STATE_APPROACH_ROUGH,
    STATE_PLACE_ROUGH,
    STATE_MOVE_TO_ROUGH_PICK,
    STATE_PICK_FROM_ROUGH,
    STATE_TRANSPORT_TO_TEMP,
    STATE_APPROACH_TEMP,
    STATE_STACK_TEMP,
    STATE_RETURN_START,
    STATE_COMPLETE,
    STATE_ERROR,
    STATE_NUM
} SystemState_t;

/* 状态名称字符串 */
static const char* STATE_NAMES[] = {
    [STATE_IDLE]                = "IDLE",
    [STATE_WAIT_MAIX_READY]     = "WAIT_MAIX",
    [STATE_MOVE_TO_QR]          = "MOVE_QR",
    [STATE_SCAN_QR]             = "SCAN_QR",
    [STATE_PROCESS_QR]          = "PROC_QR",
    [STATE_MOVE_TO_RAW]         = "MOVE_RAW",
    [STATE_FIND_MATERIAL]       = "FIND_MAT",
    [STATE_APPROACH_MATERIAL]   = "APPR_MAT",
    [STATE_PICK_MATERIAL]       = "PICK_MAT",
    [STATE_TRANSPORT_TO_ROUGH]  = "TRANS_ROUGH",
    [STATE_APPROACH_ROUGH]      = "APPR_ROUGH",
    [STATE_PLACE_ROUGH]         = "PLACE_ROUGH",
    [STATE_MOVE_TO_ROUGH_PICK]  = "MOVE_R_PICK",
    [STATE_PICK_FROM_ROUGH]     = "PICK_ROUGH",
    [STATE_TRANSPORT_TO_TEMP]   = "TRANS_TEMP",
    [STATE_APPROACH_TEMP]       = "APPR_TEMP",
    [STATE_STACK_TEMP]          = "STACK_TEMP",
    [STATE_RETURN_START]        = "RETURN",
    [STATE_COMPLETE]            = "DONE",
    [STATE_ERROR]               = "ERROR",
};

/* ============================================================
 * 场地坐标定义 (单位: mm, 以启停区中心为原点)
 * ============================================================ */

/* 赛场尺寸: 2400×2400mm, 启停区中心为坐标原点 */
#define FIELD_SIZE_MM           2400

/* 启停区: 300×300mm 蓝色区域, 中心在原点 */
#define START_ZONE_SIZE_MM      300

/* 二维码板大致位置 (根据现场调整) */
#define QR_X_MM                 400
#define QR_Y_MM                 0

/* 原料区位置 (根据现场调整) */
#define RAW_AREA_X_MM           0
#define RAW_AREA_Y_MM           600

/* 粗加工区位置 */
#define ROUGH_AREA_X_MM         0
#define ROUGH_AREA_Y_MM         -300
#define ROUGH_ZONE_SPACING_MM   200

/* 暂存区位置 */
#define TEMP_AREA_X_MM          0
#define TEMP_AREA_Y_MM          -700

/* ============================================================
 * 运动参数
 * ============================================================ */
#define MAX_LINEAR_SPEED_MM_S   300.0f      // 最大线速度 300 mm/s
#define MAX_ANGULAR_SPEED_DEG_S 90.0f       // 最大角速度 90 deg/s
#define DEFAULT_SPEED_MM_S      200.0f      // 默认线速度
#define APPROACH_SPEED_MM_S     80.0f       // 接近速度
#define POSITION_TOLERANCE_MM   10.0f       // 位置容差 (mm)
#define ANGLE_TOLERANCE_DEG     3.0f        // 角度容差 (deg)

/* ============================================================
 * 避障参数
 * ============================================================ */
#define OBSTACLE_DISTANCE_MM    200         // 避障触发距离
#define SAFE_DISTANCE_MM        150         // 安全距离

#endif /* __PROTOCOL_H */
