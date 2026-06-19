/**
 * @file    communication.c
 * @brief   STM32 <-> MaixCAM UART 通信实现
 * @note    帧协议: "CMD:xxx\r\n" / "RSP:type,data...\r\n"
 */

#include "communication.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 通信状态 */
static UART_HandleTypeDef *g_huart = NULL;

/* 接收缓冲区 */
#define RX_BUF_SIZE         256
static uint8_t g_rx_buf[RX_BUF_SIZE];
static volatile uint16_t g_rx_index = 0;
static volatile bool g_frame_ready = false;

/* 响应解析缓冲区 */
static char g_response_data[PROTOCOL_FRAME_MAX_LEN];

/* 发送缓冲区 */
static char g_tx_buf[PROTOCOL_FRAME_MAX_LEN];

/**
 * @brief  通信模块初始化
 */
void Communication_Init(UART_HandleTypeDef *huart)
{
    g_huart = huart;
    memset(g_rx_buf, 0, sizeof(g_rx_buf));
    g_rx_index = 0;
    g_frame_ready = false;

    /* 启动 UART 中断接收 */
    __HAL_UART_ENABLE_IT(g_huart, UART_IT_RXNE);
}

/**
 * @brief  发送命令到 MaixCAM
 */
void Communication_SendCommand(MaixCommand_t cmd)
{
    if (cmd >= sizeof(MAIX_CMD_STR) / sizeof(MAIX_CMD_STR[0])) return;

    int len = snprintf(g_tx_buf, sizeof(g_tx_buf),
                       "CMD:%s\r\n", MAIX_CMD_STR[cmd]);
    if (len > 0 && len < (int)sizeof(g_tx_buf)) {
        HAL_UART_Transmit(g_huart, (uint8_t *)g_tx_buf, len, 100);
    }
}

/**
 * @brief  带参数发送命令
 */
void Communication_SendCommandEx(MaixCommand_t cmd, const char *params)
{
    if (cmd >= sizeof(MAIX_CMD_STR) / sizeof(MAIX_CMD_STR[0])) return;

    int len;
    if (params && strlen(params) > 0) {
        len = snprintf(g_tx_buf, sizeof(g_tx_buf),
                       "CMD:%s,%s\r\n", MAIX_CMD_STR[cmd], params);
    } else {
        len = snprintf(g_tx_buf, sizeof(g_tx_buf),
                       "CMD:%s\r\n", MAIX_CMD_STR[cmd]);
    }
    if (len > 0 && len < (int)sizeof(g_tx_buf)) {
        HAL_UART_Transmit(g_huart, (uint8_t *)g_tx_buf, len, 100);
    }
}

/**
 * @brief  UART 接收中断回调
 * @note   在 HAL_UART_IRQHandler 中调用此函数
 */
void Communication_RxCallback(uint8_t byte)
{
    if (g_rx_index < RX_BUF_SIZE - 1) {
        g_rx_buf[g_rx_index++] = byte;

        /* 检测帧尾 \r\n */
        if (g_rx_index >= 2 &&
            g_rx_buf[g_rx_index - 2] == '\r' &&
            g_rx_buf[g_rx_index - 1] == '\n') {
            g_rx_buf[g_rx_index] = '\0';
            g_frame_ready = true;
        }

        /* 溢出保护 */
        if (g_rx_index >= RX_BUF_SIZE - 2) {
            g_rx_index = 0;  /* 丢弃缓冲区 */
        }
    }
}

/**
 * @brief  检查是否有新响应
 */
bool Communication_HasResponse(void)
{
    return g_frame_ready;
}

/**
 * @brief  解析响应消息
 *
 * 帧格式: "RSP:<type>,<data...>\r\n"
 * 示例:
 *   "RSP:QR,231\r\n"
 *   "RSP:COLOR,255,0,0,RED\r\n"
 *   "RSP:MAT,150,200,1,45\r\n"
 *   "RSP:ACK\r\n"
 *   "RSP:ERR,timeout\r\n"
 */
int Communication_ParseResponse(MaixResponse_t *type, char *data, uint16_t *len)
{
    if (!g_frame_ready) return COMM_ERR_NO_RESP;

    /* 复制帧到临时缓冲区 */
    char frame[RX_BUF_SIZE];
    memcpy(frame, (char *)g_rx_buf, g_rx_index + 1);

    /* 复位接收状态 */
    g_rx_index = 0;
    g_frame_ready = false;

    /* 检查前缀 */
    if (strncmp(frame, "RSP:", 4) != 0) {
        return COMM_ERR_PARSE;
    }

    /* 提取类型字符串 */
    char *start = frame + 4;
    char *comma = strchr(start, ',');
    char *crlf = strstr(start, "\r\n");

    char type_str[16] = {0};
    if (comma) {
        size_t type_len = comma - start;
        if (type_len > 15) type_len = 15;
        strncpy(type_str, start, type_len);
    } else if (crlf) {
        size_t type_len = crlf - start;
        if (type_len > 15) type_len = 15;
        strncpy(type_str, start, type_len);
    }

    /* 匹配响应类型 */
    *type = (MaixResponse_t)0;
    for (int i = 0; i < 8; i++) {
        if (strcmp(type_str, MAIX_RSP_STR[i]) == 0) {
            *type = (MaixResponse_t)(0x10 + i);
            break;
        }
    }

    /* 提取数据部分 */
    if (data && len) {
        if (comma) {
            char *dstart = comma + 1;
            char *dend = crlf ? crlf : (dstart + strlen(dstart));
            size_t dlen = dend - dstart;
            if (dlen >= PROTOCOL_FRAME_MAX_LEN) dlen = PROTOCOL_FRAME_MAX_LEN - 1;
            memcpy(data, dstart, dlen);
            data[dlen] = '\0';
            *len = (uint16_t)dlen;
        } else {
            data[0] = '\0';
            *len = 0;
        }
    }

    return COMM_OK;
}

/**
 * @brief  发送命令并等待响应
 */
CommError_t Communication_SendAndWait(MaixCommand_t cmd, uint32_t timeout_ms,
                                       MaixResponse_t *response_type, char *data)
{
    /* 发送命令 */
    Communication_SendCommand(cmd);

    /* 等待响应 */
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (Communication_HasResponse()) {
            uint16_t len;
            return (CommError_t)Communication_ParseResponse(response_type, data, &len);
        }
        /* 给 FreeRTOS 让出 CPU (在任务中使用 vTaskDelay) */
        osDelay(1);
    }

    return COMM_ERR_TIMEOUT;
}

/**
 * @brief  等待 MaixCAM 就绪
 */
CommError_t Communication_WaitReady(uint32_t timeout_ms)
{
    MaixResponse_t rsp;
    char data[64];

    /* 重复发送 HEARTBEAT 直到 MaixCAM 响应 */
    CommError_t err;
    for (int retry = 0; retry < 3; retry++) {
        err = Communication_SendAndWait(CMD_HEARTBEAT, timeout_ms / 3, &rsp, data);
        if (err == COMM_OK && rsp == RSP_READY_OK) {
            return COMM_OK;
        }
        HAL_Delay(500);
    }
    return COMM_ERR_TIMEOUT;
}

/**
 * @brief  请求二维码扫描
 */
CommError_t Communication_ScanQR(TaskCode_t *code, uint32_t timeout_ms)
{
    MaixResponse_t rsp;
    char data[64];
    uint16_t len;

    CommError_t err = Communication_SendAndWait(CMD_SCAN_QR, timeout_ms, &rsp, data);
    if (err != COMM_OK || rsp != RSP_QR_CODE) {
        code->valid = false;
        return err;
    }

    /* 解析三位数字编码 */
    if (len >= 3 && strlen(data) >= 3) {
        for (int i = 0; i < 3; i++) {
            code->digits[i] = (uint8_t)(data[i] - '0');
            code->colors[i] = code->digits[i];  /* 1=红, 2=绿, 3=蓝 */
        }
        code->valid = true;
    } else {
        code->valid = false;
    }

    return COMM_OK;
}

/**
 * @brief  请求颜色检测
 */
CommError_t Communication_DetectColor(MaterialColor_t *color, uint32_t timeout_ms)
{
    MaixResponse_t rsp;
    char data[64];
    uint16_t len;

    CommError_t err = Communication_SendAndWait(CMD_DETECT_COLOR, timeout_ms, &rsp, data);
    if (err != COMM_OK || rsp != RSP_COLOR_RESULT) {
        *color = COLOR_UNKNOWN;
        return err;
    }

    /* 解析颜色名称: "R,G,B,NAME" */
    char *name = strrchr(data, ',');
    if (name) {
        name++; /* 跳过逗号 */
        if (strstr(name, "RED") || strstr(name, "red")) {
            *color = COLOR_RED;
        } else if (strstr(name, "GREEN") || strstr(name, "green")) {
            *color = COLOR_GREEN;
        } else if (strstr(name, "BLUE") || strstr(name, "blue")) {
            *color = COLOR_BLUE;
        } else {
            *color = COLOR_UNKNOWN;
        }
    } else {
        *color = COLOR_UNKNOWN;
    }

    return COMM_OK;
}

/**
 * @brief  请求寻找物料位置
 */
CommError_t Communication_FindMaterial(MaterialColor_t target, MaterialPos_t *pos,
                                        uint32_t timeout_ms)
{
    MaixResponse_t rsp;
    char data[64];
    uint16_t len;

    /* 发送带颜色参数的命令 */
    char params[16];
    snprintf(params, sizeof(params), "%d", (int)target);
    Communication_SendCommandEx(CMD_FIND_MATERIAL, params);

    /* 等待响应 */
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (Communication_HasResponse()) {
            CommError_t err = (CommError_t)Communication_ParseResponse(&rsp, data, &len);
            if (err == COMM_OK && rsp == RSP_MAT_POS) {
                /* 解析 "x,y,color,angle" */
                int x, y, col, ang;
                if (sscanf(data, "%d,%d,%d,%d", &x, &y, &col, &ang) >= 2) {
                    pos->x_mm = (int16_t)x;
                    pos->y_mm = (int16_t)y;
                    pos->color = (col >= 1 && col <= 3) ? (MaterialColor_t)col : COLOR_UNKNOWN;
                    pos->angle_deg = (ang > 0) ? (int16_t)ang : 0;
                    return COMM_OK;
                }
            }
        }
        osDelay(5);
    }

    return COMM_ERR_TIMEOUT;
}

/**
 * @brief  请求检查放置区域
 */
CommError_t Communication_CheckZone(MaterialColor_t color, ZoneInfo_t *zone,
                                     uint32_t timeout_ms)
{
    MaixResponse_t rsp;
    char data[64];
    uint16_t len;

    char params[16];
    snprintf(params, sizeof(params), "%d", (int)color);
    Communication_SendCommandEx(CMD_CHECK_ZONE, params);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (Communication_HasResponse()) {
            CommError_t err = (CommError_t)Communication_ParseResponse(&rsp, data, &len);
            if (err == COMM_OK && rsp == RSP_ZONE_INFO) {
                int x, y, col;
                if (sscanf(data, "%d,%d,%d", &x, &y, &col) >= 2) {
                    zone->x_mm = (int16_t)x;
                    zone->y_mm = (int16_t)y;
                    zone->color = (MaterialColor_t)col;
                    return COMM_OK;
                }
            }
        }
        osDelay(5);
    }

    return COMM_ERR_TIMEOUT;
}

/**
 * @brief  通信健康检查
 */
bool Communication_IsHealthy(void)
{
    return (g_huart != NULL) && (g_huart->Instance != NULL);
}
