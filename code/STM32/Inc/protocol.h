/**
 * @file    protocol.h
 * @brief   通信协议 + 场地坐标 + 运动参数 (GMR编码器版本)
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 帧格式: "前缀:类型[,参数...]\r\n"
 * ============================================================ */
#define PROTOCOL_DELIMITER      "\r\n"
#define PROTOCOL_FRAME_MAX_LEN  128

typedef enum {
    CMD_SCAN_QR = 0x01, CMD_DETECT_COLOR = 0x02, CMD_FIND_MATERIAL = 0x03,
    CMD_CHECK_ZONE = 0x04, CMD_HEARTBEAT = 0x05, CMD_READY = 0x06,
} MaixCommand_t;

typedef enum {
    RSP_QR_CODE = 0x10, RSP_COLOR_RESULT = 0x11, RSP_MAT_POS = 0x12,
    RSP_ZONE_INFO = 0x13, RSP_ACK = 0x14, RSP_ERROR = 0x15,
    RSP_READY_OK = 0x16, RSP_DONE = 0x17,
} MaixResponse_t;

/* ============================================================
 * 数据结构
 * ============================================================ */
typedef struct {
    uint8_t digits[3];
    uint8_t colors[3];
    bool valid;
} TaskCode_t;

typedef enum {
    COLOR_UNKNOWN = 0, COLOR_RED = 1, COLOR_GREEN = 2, COLOR_BLUE = 3,
} MaterialColor_t;

typedef struct {
    int16_t x_mm, y_mm, angle_deg;
    MaterialColor_t color;
} MaterialPos_t;

typedef struct {
    int16_t x_mm, y_mm;
    MaterialColor_t color;
} ZoneInfo_t;

typedef enum {
    STATE_IDLE = 0, STATE_WAIT_MAIX_READY, STATE_MOVE_TO_QR,
    STATE_SCAN_QR, STATE_PROCESS_QR, STATE_MOVE_TO_RAW,
    STATE_FIND_MATERIAL, STATE_APPROACH_MATERIAL, STATE_PICK_MATERIAL,
    STATE_TRANSPORT_TO_ROUGH, STATE_APPROACH_ROUGH, STATE_PLACE_ROUGH,
    STATE_MOVE_TO_ROUGH_PICK, STATE_PICK_FROM_ROUGH,
    STATE_TRANSPORT_TO_TEMP, STATE_APPROACH_TEMP, STATE_STACK_TEMP,
    STATE_RETURN_START, STATE_COMPLETE, STATE_ERROR, STATE_NUM
} SystemState_t;

/* ============================================================
 * 场地坐标 (mm, 启停区中心为原点)
 * ============================================================ */
#define FIELD_SIZE_MM           2400
#define QR_X_MM                 400
#define QR_Y_MM                 0
#define RAW_AREA_X_MM           0
#define RAW_AREA_Y_MM           600
#define ROUGH_AREA_X_MM         0
#define ROUGH_AREA_Y_MM         -300
#define ROUGH_ZONE_SPACING_MM   200
#define TEMP_AREA_X_MM          0
#define TEMP_AREA_Y_MM          -700

/* ============================================================
 * 运动参数 (GMR编码器版: 75mm轮径, 60:1减速比)
 * ============================================================ */
#define WHEEL_DIAMETER_MM       75.0f
#define WHEEL_RADIUS_MM         (WHEEL_DIAMETER_MM / 2.0f)
#define CHASSIS_LX_MM           93.0f       /* 半轮距 */
#define CHASSIS_LY_MM           85.0f       /* 半轴距 */
#define CHASSIS_L_SUM           (CHASSIS_LX_MM + CHASSIS_LY_MM)

#define ENCODER_PPR             500
#define ENCODER_CPR             (ENCODER_PPR * 4)
#define GEAR_RATIO              60.0f
#define PULSE_PER_REV           (uint32_t)(ENCODER_CPR * GEAR_RATIO) /* 120,000 */

#define MAX_LINEAR_SPEED_MM_S   300.0f
#define MAX_ANGULAR_SPEED_DEG_S 90.0f
#define DEFAULT_SPEED_MM_S      200.0f
#define APPROACH_SPEED_MM_S     80.0f
#define POSITION_TOLERANCE_MM   10.0f
#define ANGLE_TOLERANCE_DEG     3.0f

#define OBSTACLE_DISTANCE_MM    200
#define SAFE_DISTANCE_MM        150

#define MOTOR_MAX_RPM           300.0f

/* 状态名称字符串 */
static const char* STATE_NAMES[] = {
    [STATE_IDLE] = "IDLE", [STATE_WAIT_MAIX_READY] = "WAIT_MAIX",
    [STATE_MOVE_TO_QR] = "MOVE_QR", [STATE_SCAN_QR] = "SCAN_QR",
    [STATE_PROCESS_QR] = "PROC_QR", [STATE_MOVE_TO_RAW] = "MOVE_RAW",
    [STATE_FIND_MATERIAL] = "FIND_MAT", [STATE_APPROACH_MATERIAL] = "APPR_MAT",
    [STATE_PICK_MATERIAL] = "PICK_MAT", [STATE_TRANSPORT_TO_ROUGH] = "TRANS_ROUGH",
    [STATE_APPROACH_ROUGH] = "APPR_ROUGH", [STATE_PLACE_ROUGH] = "PLACE_ROUGH",
    [STATE_MOVE_TO_ROUGH_PICK] = "MOVE_R_PICK", [STATE_PICK_FROM_ROUGH] = "PICK_ROUGH",
    [STATE_TRANSPORT_TO_TEMP] = "TRANS_TEMP", [STATE_APPROACH_TEMP] = "APPR_TEMP",
    [STATE_STACK_TEMP] = "STACK_TEMP", [STATE_RETURN_START] = "RETURN",
    [STATE_COMPLETE] = "DONE", [STATE_ERROR] = "ERROR",
};

/* 命令/响应字符串 */
static const char* MAIX_CMD_STR[] = {
    [CMD_SCAN_QR] = "SCAN_QR", [CMD_DETECT_COLOR] = "DETECT_COLOR",
    [CMD_FIND_MATERIAL] = "FIND_MATERIAL", [CMD_CHECK_ZONE] = "CHECK_ZONE",
    [CMD_HEARTBEAT] = "HEARTBEAT", [CMD_READY] = "READY",
};
static const char* MAIX_RSP_STR[] = {
    [RSP_QR_CODE - 0x10] = "QR", [RSP_COLOR_RESULT - 0x10] = "COLOR",
    [RSP_MAT_POS - 0x10] = "MAT", [RSP_ZONE_INFO - 0x10] = "ZONE",
    [RSP_ACK - 0x10] = "ACK", [RSP_ERROR - 0x10] = "ERR",
    [RSP_READY_OK - 0x10] = "READY", [RSP_DONE - 0x10] = "DONE",
};

#endif /* __PROTOCOL_H */
