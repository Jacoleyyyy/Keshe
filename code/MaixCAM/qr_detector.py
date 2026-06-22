"""
qr_detector.py — QR二维码检测模块 (MaixPy v4)

API: maix.image.Image.find_qrcodes()

返回: QRCode 对象列表
  - qr.payload()  → 解码后的字符串
  - qr.x(), qr.y() → 左上角坐标
  - qr.corners()  → 四个角点坐标 [(x0,y0), (x1,y1), (x2,y2), (x3,y3)]

参考: https://wiki.sipeed.com/maixpy/doc/en/vision/qrcode.html
"""

import time
from maix import image


class QRDetector:
    """QR二维码检测器 (含多帧稳定验证)"""

    def __init__(self):
        self.last_code = None
        self.stable_count = 0
        self.min_stable = 3       # 连续3帧相同才认为有效

    def scan(self, img) -> str | None:
        """
        扫描图像中的二维码

        Args:
            img: maix.image.Image 对象

        Returns:
            三位数字编码字符串 (如 "231"), 或 None
        """
        qrcodes = img.find_qrcodes()

        if not qrcodes:
            self.stable_count = 0
            return None

        # 取第一个 QR 码
        qr = qrcodes[0]
        payload = qr.payload()

        if not payload:
            self.stable_count = 0
            return None

        payload = payload.strip()

        # 验证合法性 (三位数字, 每位1-3)
        if not self._is_valid(payload):
            self.stable_count = 0
            return None

        # 稳定性检查
        if payload == self.last_code:
            self.stable_count += 1
        else:
            self.stable_count = 1
            self.last_code = payload

        if self.stable_count >= self.min_stable:
            return payload

        return None

    def _is_valid(self, code: str) -> bool:
        """验证: 长度=3, 仅含'1'/'2'/'3'"""
        if len(code) != 3:
            return False
        for c in code:
            if c not in ('1', '2', '3'):
                return False
        return True

    def draw_overlay(self, img, code: str = None):
        """在图像上绘制 QR 检测框和文字"""
        qrcodes = img.find_qrcodes()
        for qr in qrcodes:
            corners = qr.corners()
            for i in range(4):
                p0 = corners[i]
                p1 = corners[(i + 1) % 4]
                img.draw_line(p0[0], p0[1], p1[0], p1[1],
                              image.COLOR_GREEN, 2)
            if code:
                img.draw_string(qr.x(), qr.y() - 15,
                                f"QR:{code}", image.COLOR_GREEN, 1.5)

    def reset(self):
        self.last_code = None
        self.stable_count = 0
