"""
qr_detector.py - QR二维码检测模块

功能:
  1. 从相机图像中检测 QR 二维码
  2. 解析三位数字任务编码 (如 "231")
  3. 验证编码合法性 (仅含数字1/2/3)

依赖: MaixPy image 模块
"""

import time
import image
from config import *


class QRDetector:
    """
    QR二维码检测器

    使用 MaixPy 内置的 find_qrcodes() 进行检测
    """

    def __init__(self):
        self.last_code = None
        self.last_time = 0
        self.code_stable_count = 0
        self.min_stable_frames = 3    # 连续N帧相同才认为有效

    def scan(self, img) -> str | None:
        """
        扫描图像中的二维码

        Args:
            img: MaixPy image 对象

        Returns:
            三位数字编码字符串 (如 "231"), 或 None 表示未检测到
        """
        # 使用 MaixPy 内置 QR 检测
        qrcodes = img.find_qrcodes()

        if not qrcodes:
            self.code_stable_count = 0
            return None

        # 取第一个检测到的QR码
        qr = qrcodes[0]
        payload = qr.payload()

        if not payload:
            return None

        # 过滤非三位数字
        payload = payload.strip()
        if not self._is_valid_code(payload):
            return None

        # 稳定性检查
        if payload == self.last_code:
            self.code_stable_count += 1
        else:
            self.code_stable_count = 1
            self.last_code = payload

        if self.code_stable_count >= self.min_stable_frames:
            self.last_time = time.ticks_ms()
            return payload

        return None

    def _is_valid_code(self, code: str) -> bool:
        """
        验证编码合法性

        Args:
            code: 待验证字符串

        Returns:
            True=合法的三位数字编码(每位1-3)
        """
        if len(code) != QR_EXPECTED_LEN:
            return False

        for c in code:
            if c not in ('1', '2', '3'):
                return False

        return True

    def draw_qr_overlay(self, img, code: str):
        """
        在图像上绘制 QR 检测结果 (用于调试/显示)
        """
        qrcodes = img.find_qrcodes()
        for qr in qrcodes:
            rect = qr.rect()
            img.draw_rectangle(rect[0], rect[1], rect[2], rect[3],
                               color=(0, 255, 0), thickness=2)

        if code:
            img.draw_string(10, 10, f"QR: {code}",
                           color=(0, 255, 0), scale=2)

    def reset(self):
        """重置检测状态"""
        self.last_code = None
        self.code_stable_count = 0
