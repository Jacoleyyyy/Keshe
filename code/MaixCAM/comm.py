"""
comm.py — UART 通信模块 (MaixPy v4)

与 STM32 通过 UART 进行 CMD:/RSP: 协议通信

帧格式:
  STM32 → MaixCAM:  "CMD:<command>\r\n"
  MaixCAM → STM32:  "RSP:<type>,<data>\r\n"

API: machine.UART (MicroPython 标准)
"""

import time
from machine import UART
from config import *


class MaixComm:
    """MaixCAM UART 通信管理器"""

    def __init__(self, port=UART_PORT, baudrate=UART_BAUDRATE):
        self.uart = UART(port, baudrate)
        self.rx_buf = bytearray()
        self.current_command = None
        self.current_params = None
        self.command_ready = False
        self.last_heartbeat = time.ticks_ms()
        self.healthy = True

    # -------------------------------------------------------
    # 主更新: 在循环中调用, 检查是否有新命令
    # -------------------------------------------------------
    def update(self) -> bool:
        """
        读取 UART 数据, 检测完整帧

        Returns:
            True = 有新命令待处理
        """
        n = self.uart.any()
        if n > 0:
            data = self.uart.read(n)
            if data:
                self.rx_buf.extend(data)
                if b'\r\n' in self.rx_buf:
                    self._parse_frame()
                    return True
        return False

    def get_command(self) -> tuple:
        """获取当前命令 (command_str, params_str)"""
        cmd = self.current_command
        params = self.current_params
        self.command_ready = False
        return (cmd, params)

    # -------------------------------------------------------
    # 发送响应
    # -------------------------------------------------------
    def send_response(self, rsp_type: str, data: str = ""):
        """发送响应帧"""
        if data:
            msg = f"RSP:{rsp_type},{data}\r\n"
        else:
            msg = f"RSP:{rsp_type}\r\n"
        self.uart.write(msg)

    def send_error(self, msg: str):
        self.send_response(RSP_ERROR, msg)

    def send_ack(self):
        self.send_response(RSP_ACK)

    def send_ready(self):
        self.send_response(RSP_READY_OK)

    def send_qr_code(self, code: str):
        self.send_response(RSP_QR_CODE, code)

    def send_material_pos(self, x_mm: int, y_mm: int, color: int, angle: int):
        self.send_response(RSP_MAT_POS,
                           f"{x_mm},{y_mm},{color},{angle}")

    def send_zone_info(self, x_mm: int, y_mm: int, color: int):
        self.send_response(RSP_ZONE_INFO,
                           f"{x_mm},{y_mm},{color}")

    # -------------------------------------------------------
    # 内部: 帧解析
    # -------------------------------------------------------
    def _parse_frame(self):
        """解析接收缓冲区中的命令帧"""
        try:
            end = self.rx_buf.find(b'\r\n')
            if end < 0:
                return

            frame = self.rx_buf[:end].decode('utf-8', errors='ignore')
            self.rx_buf = self.rx_buf[end + 2:]

            if frame.startswith(CMD_PREFIX):
                content = frame[len(CMD_PREFIX):]
                if ',' in content:
                    cmd, params = content.split(',', 1)
                else:
                    cmd = content
                    params = ""
                self.current_command = cmd.strip()
                self.current_params = params.strip()
                self.command_ready = True

        except Exception as e:
            print(f"[Comm] Parse error: {e}")
            self.rx_buf = bytearray()

    # -------------------------------------------------------
    # 心跳检查
    # -------------------------------------------------------
    def check_heartbeat(self) -> bool:
        now = time.ticks_ms()
        if time.ticks_diff(now, self.last_heartbeat) > 3000:
            self.healthy = False
            return False
        self.healthy = True
        return True
