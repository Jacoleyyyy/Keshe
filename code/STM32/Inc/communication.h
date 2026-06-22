/**
 * @file    communication.h
 * @brief   STM32 <-> MaixCAM UART 通信模块
 * @note    协议: "CMD:xxx\r\n" / "RSP:xxx\r\n"
 */

#ifndef __COMMUNICATION_H
#define __COMMUNICATION_H

#include "pin_config.h"
#include "protocol.h"

/* 通信帧就绪标志 (供 Task_Comm 使用) */
extern volatile bool g_frame_ready;

/* 通信超时 */
#define COMM_TIMEOUT_MS         5000

/* 通信错误码 */
typedef enum {
    COMM_OK             = 0,
    COMM_ERR_TIMEOUT    = -1,
    COMM_ERR_CRC        = -2,
    COMM_ERR_BUSY       = -3,
    COMM_ERR_NO_RESP    = -4,
    COMM_ERR_PARSE      = -5,
} CommError_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief  通信模块初始化
 */
void Communication_Init(UART_HandleTypeDef *huart);

/**
 * @brief  发送命令到 MaixCAM
 */
void Communication_SendCommand(MaixCommand_t cmd);

/**
 * @brief  带参数的发送命令
 */
void Communication_SendCommandEx(MaixCommand_t cmd, const char *params);

/**
 * @brief  检查是否有新响应 (非阻塞)
 * @return true=有响应待处理
 */
bool Communication_HasResponse(void);

/**
 * @brief  解析响应消息
 * @param  type    输出: 响应类型
 * @param  data    输出: 数据缓冲区
 * @param  len     输出: 数据长度
 * @return 0=成功, <0=错误
 */
int Communication_ParseResponse(MaixResponse_t *type, char *data, uint16_t *len);

/**
 * @brief  发送命令并等待响应 (阻塞式)
 * @param  cmd       命令
 * @param  timeout_ms 超时 (ms)
 * @param  response_type 输出: 响应类型
 * @param  data      输出: 响应数据
 * @return COMM_OK 或错误码
 */
CommError_t Communication_SendAndWait(MaixCommand_t cmd, uint32_t timeout_ms,
                                       MaixResponse_t *response_type, char *data);

/**
 * @brief  UART 接收回调 (由 HAL 中断调用)
 */
void Communication_RxCallback(uint8_t byte);

/**
 * @brief  通信健康检查
 */
bool Communication_IsHealthy(void);

/**
 * @brief  等待 MaixCAM 就绪
 */
CommError_t Communication_WaitReady(uint32_t timeout_ms);

/**
 * @brief  请求二维码扫描 (阻塞, 返回任务编码)
 */
CommError_t Communication_ScanQR(TaskCode_t *code, uint32_t timeout_ms);

/**
 * @brief  请求颜色检测 (阻塞)
 */
CommError_t Communication_DetectColor(MaterialColor_t *color, uint32_t timeout_ms);

/**
 * @brief  请求寻找物料位置
 */
CommError_t Communication_FindMaterial(MaterialColor_t target, MaterialPos_t *pos,
                                        uint32_t timeout_ms);

/**
 * @brief  请求检查放置区域
 */
CommError_t Communication_CheckZone(MaterialColor_t color, ZoneInfo_t *zone,
                                     uint32_t timeout_ms);

#endif /* __COMMUNICATION_H */
