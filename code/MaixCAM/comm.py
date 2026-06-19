"""
comm.py - UART 通信模块

与 STM32 主控制器通过 UART 进行命令-响应通信

帧格式:
  STM32 → MaixCAM:  "CMD:<command>[,params]\r\n"
  MaixCAM → STM32:  "RSP:<type>[,data...]\r\n"

协议定义参见 STM32 端 protocol.h
"""

import time
from machine import UART
from config import *


class MaixComm:
    """
    MaixCAM UART 通信管理器

    负责:
      1. 接收 STM32 命令
      2. 解析命令
      3. 发送响应
    """

    def __init__(self, port=UART_PORT, baudrate=UART_BAUDRATE):
        self.uart = UART(port, baudrate)
        self.uart.init(baudrate, bits=8, parity=None, stop=1)

        self.rx_buf = bytearray()
        self.command_ready = False
        self.current_command = None
        self.current_params = None

        self.last_heartbeat = time.ticks_ms()
        self.healthy = True

    def update(self):
        """
        读取 UART 数据, 检查是否收到完整命令

        应在主循环中定期调用

        Returns:
            True=有新命令待处理
        """
        # 读取可用字节
        n = self.uart.any()
        if n > 0:
            data = self.uart.read(n)
            if data:
                self.rx_buf.extend(data)

                # 检查是否收到完整帧 (以 \r\n 结尾)
                if self.rx_buf.find(b'\r\n') >= 0:
                    self._parse_frame()
                    return True

        return False

    def get_command(self) -> tuple[str, str]:
        """
        获取当前命令和参数

        Returns:
            (command, params) 例如 ("SCAN_QR", "") 或 ("FIND_MATERIAL", "1")
        """
        cmd = self.current_command
        params = self.current_params
        self.command_ready = False
        return (cmd, params)

    def send_response(self, rsp_type: str, data: str = ""):
        """
        发送响应到 STM32

        Args:
            rsp_type: 响应类型 (如 "QR", "COLOR", "ACK")
            data: 响应数据 (可选)
        """
        if data:
            msg = f"RSP:{rsp_type},{data}\r\n"
        else:
            msg = f"RSP:{rsp_type}\r\n"

        self.uart.write(msg)

    def send_error(self, error_msg: str):
        """发送错误响应"""
        self.send_response(RSP_ERROR, error_msg)

    def send_ack(self):
        """发送确认"""
        self.send_response(RSP_ACK)

    def send_ready(self):
        """发送就绪信号"""
        self.send_response(RSP_READY_OK)

    def send_qr_code(self, code: str):
        """发送二维码编码"""
        self.send_response(RSP_QR_CODE, code)

    def send_color_result(self, r: int, g: int, b: int, name: str):
        """发送颜色检测结果"""
        self.send_response(RSP_COLOR_RESULT, f"{r},{g},{b},{name}")

    def send_material_pos(self, x_mm: int, y_mm: int, color: int, angle: int):
        """发送物料位置"""
        self.send_response(RSP_MAT_POS, f"{x_mm},{y_mm},{color},{angle}")

    def send_zone_info(self, x_mm: int, y_mm: int, color: int):
        """发送区域信息"""
        self.send_response(RSP_ZONE_INFO, f"{x_mm},{y_mm},{color}")

    def _parse_frame(self):
        """解析接收到的命令帧"""
        try:
            # 找到帧结束位置
            end_idx = self.rx_buf.find(b'\r\n')
            if end_idx < 0:
                return

            frame = self.rx_buf[:end_idx].decode('utf-8', errors='ignore')
            self.rx_buf = self.rx_buf[end_idx + 2:]  # 移除已处理部分

            # 解析 "CMD:<command>[,params]"
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

            elif frame.startswith("HEARTBEAT"):
                # 收到心跳, 回复就绪
                self.send_ready()
                self.last_heartbeat = time.ticks_ms()

        except Exception as e:
            print(f"Parse error: {e}")
            self.rx_buf = bytearray()  # 丢弃损坏数据

    def check_heartbeat(self):
        """
        检查心跳超时

        Returns:
            True=通信正常
        """
        now = time.ticks_ms()
        if time.ticks_diff(now, self.last_heartbeat) > 3000:
            self.healthy = False
            return False
        self.healthy = True
        return True
