/**
 * @file    communication.c
 * @brief   UART 通信模块 (GMR版: USART3 PB10/PB11)
 */

#include "communication.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static UART_HandleTypeDef *g_huart = NULL;

#define RX_BUF_SIZE 256
static uint8_t g_rx_buf[RX_BUF_SIZE];
static volatile uint16_t g_rx_idx = 0;
volatile bool g_frame_ready = false;

static char g_tx_buf[PROTOCOL_FRAME_MAX_LEN];

void Communication_Init(UART_HandleTypeDef *huart)
{
    g_huart = huart;
    memset(g_rx_buf, 0, sizeof(g_rx_buf));
    g_rx_idx = 0;
    g_frame_ready = false;
}

void Communication_SendCommand(MaixCommand_t cmd)
{
    if (!g_huart) return;
    int len = snprintf(g_tx_buf, sizeof(g_tx_buf), "CMD:%s\r\n", MAIX_CMD_STR[cmd]);
    if (len > 0) HAL_UART_Transmit(g_huart, (uint8_t *)g_tx_buf, len, 100);
}

void Communication_SendCommandEx(MaixCommand_t cmd, const char *params)
{
    if (!g_huart) return;
    int len;
    if (params && strlen(params) > 0)
        len = snprintf(g_tx_buf, sizeof(g_tx_buf), "CMD:%s,%s\r\n", MAIX_CMD_STR[cmd], params);
    else
        len = snprintf(g_tx_buf, sizeof(g_tx_buf), "CMD:%s\r\n", MAIX_CMD_STR[cmd]);
    if (len > 0) HAL_UART_Transmit(g_huart, (uint8_t *)g_tx_buf, len, 100);
}

void Communication_RxCallback(uint8_t byte)
{
    if (g_rx_idx < RX_BUF_SIZE - 1) {
        g_rx_buf[g_rx_idx++] = byte;
        if (g_rx_idx >= 2 && g_rx_buf[g_rx_idx - 2] == '\r' && g_rx_buf[g_rx_idx - 1] == '\n') {
            g_rx_buf[g_rx_idx] = '\0';
            g_frame_ready = true;
        }
        if (g_rx_idx >= RX_BUF_SIZE - 2) g_rx_idx = 0;
    }
}

bool Communication_HasResponse(void) { return g_frame_ready; }

int Communication_ParseResponse(MaixResponse_t *type, char *data, uint16_t *len)
{
    if (!g_frame_ready) return COMM_ERR_NO_RESP;
    char frame[RX_BUF_SIZE];
    memcpy(frame, (char *)g_rx_buf, g_rx_idx + 1);
    g_rx_idx = 0;
    g_frame_ready = false;

    if (strncmp(frame, "RSP:", 4) != 0) return COMM_ERR_PARSE;

    char *start = frame + 4;
    char *comma = strchr(start, ',');
    char *crlf = strstr(start, "\r\n");

    char type_str[16] = {0};
    if (comma) {
        size_t n = comma - start; if (n > 15) n = 15;
        strncpy(type_str, start, n);
    } else if (crlf) {
        size_t n = crlf - start; if (n > 15) n = 15;
        strncpy(type_str, start, n);
    }

    *type = (MaixResponse_t)0;
    for (int i = 0; i < 8; i++)
        if (strcmp(type_str, MAIX_RSP_STR[i]) == 0) { *type = (MaixResponse_t)(0x10 + i); break; }

    if (data && len) {
        if (comma) {
            char *d = comma + 1;
            char *e = crlf ? crlf : (d + strlen(d));
            size_t n = e - d;
            if (n >= PROTOCOL_FRAME_MAX_LEN) n = PROTOCOL_FRAME_MAX_LEN - 1;
            memcpy(data, d, n); data[n] = '\0'; *len = (uint16_t)n;
        } else { data[0] = '\0'; *len = 0; }
    }
    return COMM_OK;
}

CommError_t Communication_SendAndWait(MaixCommand_t cmd, uint32_t to,
                                       MaixResponse_t *rsp, char *data)
{
    Communication_SendCommand(cmd);
    uint32_t t = HAL_GetTick();
    while ((HAL_GetTick() - t) < to) {
        if (Communication_HasResponse()) {
            uint16_t l;
            return (CommError_t)Communication_ParseResponse(rsp, data, &l);
        }
        osDelay(1);
    }
    return COMM_ERR_TIMEOUT;
}

CommError_t Communication_WaitReady(uint32_t to)
{
    MaixResponse_t r;
    char d[64];
    for (int i = 0; i < 3; i++) {
        if (Communication_SendAndWait(CMD_HEARTBEAT, to / 3, &r, d) == COMM_OK && r == RSP_READY_OK)
            return COMM_OK;
        HAL_Delay(500);
    }
    return COMM_ERR_TIMEOUT;
}

CommError_t Communication_ScanQR(TaskCode_t *code, uint32_t to)
{
    MaixResponse_t r;
    char d[64];
    uint16_t l;
    CommError_t e = Communication_SendAndWait(CMD_SCAN_QR, to, &r, d);
    if (e != COMM_OK || r != RSP_QR_CODE) { code->valid = false; return e; }
    if (strlen(d) >= 3) {
        for (int i = 0; i < 3; i++) {
            code->digits[i] = (uint8_t)(d[i] - '0');
            code->colors[i] = code->digits[i];
        }
        code->valid = true;
    } else code->valid = false;
    return COMM_OK;
}

CommError_t Communication_DetectColor(MaterialColor_t *color, uint32_t to)
{
    MaixResponse_t r;
    char d[64];
    uint16_t l;
    CommError_t e = Communication_SendAndWait(CMD_DETECT_COLOR, to, &r, d);
    if (e != COMM_OK || r != RSP_COLOR_RESULT) { *color = COLOR_UNKNOWN; return e; }
    char *n = strrchr(d, ',');
    if (n) {
        n++;
        if (strstr(n, "RED")) *color = COLOR_RED;
        else if (strstr(n, "GREEN")) *color = COLOR_GREEN;
        else if (strstr(n, "BLUE")) *color = COLOR_BLUE;
        else *color = COLOR_UNKNOWN;
    } else *color = COLOR_UNKNOWN;
    return COMM_OK;
}

CommError_t Communication_FindMaterial(MaterialColor_t tgt, MaterialPos_t *pos, uint32_t to)
{
    char p[16]; snprintf(p, sizeof(p), "%d", (int)tgt);
    Communication_SendCommandEx(CMD_FIND_MATERIAL, p);
    MaixResponse_t r;
    char d[64];
    uint32_t s = HAL_GetTick();
    while ((HAL_GetTick() - s) < to) {
        if (Communication_HasResponse()) {
            uint16_t l;
            if (Communication_ParseResponse(&r, d, &l) == COMM_OK && r == RSP_MAT_POS) {
                int x, y, c, a;
                if (sscanf(d, "%d,%d,%d,%d", &x, &y, &c, &a) >= 2) {
                    pos->x_mm = x; pos->y_mm = y;
                    pos->color = (c >= 1 && c <= 3) ? (MaterialColor_t)c : COLOR_UNKNOWN;
                    pos->angle_deg = a > 0 ? a : 0;
                    return COMM_OK;
                }
            }
        }
        osDelay(5);
    }
    return COMM_ERR_TIMEOUT;
}

CommError_t Communication_CheckZone(MaterialColor_t c, ZoneInfo_t *z, uint32_t to)
{
    char p[16]; snprintf(p, sizeof(p), "%d", (int)c);
    Communication_SendCommandEx(CMD_CHECK_ZONE, p);
    MaixResponse_t r;
    char d[64];
    uint32_t s = HAL_GetTick();
    while ((HAL_GetTick() - s) < to) {
        if (Communication_HasResponse()) {
            uint16_t l;
            if (Communication_ParseResponse(&r, d, &l) == COMM_OK && r == RSP_ZONE_INFO) {
                int x, y, col;
                if (sscanf(d, "%d,%d,%d", &x, &y, &col) >= 2) {
                    z->x_mm = x; z->y_mm = y; z->color = (MaterialColor_t)col;
                    return COMM_OK;
                }
            }
        }
        osDelay(5);
    }
    return COMM_ERR_TIMEOUT;
}

CommError_t Communication_CheckLane(LaneInfo_t *lane, uint32_t timeout_ms)
{
    Communication_SendCommand(CMD_CHECK_LANE);

    MaixResponse_t rsp;
    char data[64];
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (Communication_HasResponse()) {
            uint16_t len;
            CommError_t err = (CommError_t)Communication_ParseResponse(&rsp, data, &len);
            if (err == COMM_OK && rsp == RSP_LANE_DATA) {
                /* 解析 "offset_mm,on_lane" */
                int off, on;
                if (sscanf(data, "%d,%d", &off, &on) >= 1) {
                    lane->offset_mm = (int16_t)off;
                    lane->on_lane   = (on != 0);
                    return COMM_OK;
                }
            }
        }
        osDelay(5);
    }

    return COMM_ERR_TIMEOUT;
}

bool Communication_IsHealthy(void)
{
    return (g_huart != NULL);
}
