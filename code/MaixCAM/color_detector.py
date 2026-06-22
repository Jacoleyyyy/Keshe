"""
color_detector.py — 颜色识别与物料定位 (MaixPy v4)

API: maix.image.Image.find_blobs()

find_blobs 返回格式: 每个 blob 是一个列表
  [x, y, w, h, area, cx, cy, rotation]
    x, y    — 包围框左上角
    w, h    — 包围框宽高
    area    — 面积 (像素数)
    cx, cy  — 质心坐标
    rotation — 旋转角度 (度)

参考: https://wiki.sipeed.com/maixpy/doc/en/vision/find_blobs.html
"""

import math
from maix import image
from config import *


class ColorDetector:
    """颜色检测器 — LAB 色彩空间阈值分割"""

    COLOR_MAP   = {"RED": 1, "GREEN": 2, "BLUE": 3}
    COLOR_NAMES = {1: "RED", 2: "GREEN", 3: "BLUE"}

    def __init__(self):
        self.img_w = CAMERA_WIDTH
        self.img_h = CAMERA_HEIGHT
        # 相机参数 (根据实际安装高度和镜头调整)
        self.camera_height_mm = 150.0
        self.camera_fov_w = 60.0    # 水平视场角
        self.camera_fov_h = 45.0    # 垂直视场角

    # -------------------------------------------------------
    # 颜色检测
    # -------------------------------------------------------
    def detect_color(self, img) -> int:
        """检测图像中占比最大的颜色，返回颜色代码 1/2/3 (0=无)"""
        red_area   = self._color_area(img, "RED")
        green_area = self._color_area(img, "GREEN")
        blue_area  = self._color_area(img, "BLUE")

        best = max(
            (red_area,   1),
            (green_area, 2),
            (blue_area,  3),
            key=lambda x: x[0]
        )
        return best[1] if best[0] > 0 else 0

    # -------------------------------------------------------
    # 物料定位
    # -------------------------------------------------------
    def find_material(self, img, target_color: int):
        """
        寻找指定颜色的物料

        Args:
            img: maix.image.Image
            target_color: 1=红, 2=绿, 3=蓝

        Returns:
            (cx, cy, area, rotation) 或 None
        """
        name = self.COLOR_NAMES.get(target_color)
        if not name:
            return None

        blobs = self._find_blobs(img, name)

        if not blobs:
            return None

        # 取面积最大的 blob
        blobs.sort(key=lambda b: b[4], reverse=True)  # b[4] = area
        blob = blobs[0]

        x, y, w, h, area, cx, cy, rotation = blob

        if area < MATERIAL_MIN_AREA or area > MATERIAL_MAX_AREA:
            return None

        return (cx, cy, area, rotation)

    # -------------------------------------------------------
    # 色环/区域检测
    # -------------------------------------------------------
    def find_zone(self, img, target_color: int):
        """
        寻找色环区域 (用于粗加工区/暂存区精确放置)

        Returns:
            (cx, cy, radius, ring_count) 或 None
        """
        name = self.COLOR_NAMES.get(target_color)
        if not name:
            return None

        blobs = self._find_blobs(img, name)
        if not blobs:
            return None

        blobs.sort(key=lambda b: b[4], reverse=True)
        blob = blobs[0]

        x, y, w, h, area, cx, cy, rotation = blob
        radius = int(math.sqrt(area / math.pi))
        ring_count = max(1, radius // 10)

        return (cx, cy, radius, ring_count)

    # -------------------------------------------------------
    # 像素坐标 → 世界坐标
    # -------------------------------------------------------
    def pixel_to_world(self, px_x: float, px_y: float) -> tuple:
        """
        像素坐标 → 相对于相机中心的世界坐标 (mm)

        假设相机垂直向下拍摄
        """
        dx = px_x - self.img_w / 2.0
        dy = px_y - self.img_h / 2.0

        # 每像素对应实际尺寸
        real_w = 2.0 * self.camera_height_mm * math.tan(
            math.radians(self.camera_fov_w / 2.0))
        real_h = 2.0 * self.camera_height_mm * math.tan(
            math.radians(self.camera_fov_h / 2.0))

        mm_per_px_x = real_w / self.img_w
        mm_per_px_y = real_h / self.img_h

        return (int(dx * mm_per_px_x), int(dy * mm_per_px_y))

    # -------------------------------------------------------
    # 内部: 颜色 blob 查找
    # -------------------------------------------------------
    def _find_blobs(self, img, color_name: str) -> list:
        """查找指定颜色的所有 blob"""
        all_blobs = []

        if color_name == "RED":
            blobs = img.find_blobs(
                [RED_THRESHOLD_1],
                area_threshold=MATERIAL_MIN_AREA,
                pixels_threshold=MATERIAL_MIN_PIXELS,
                merge=True
            )
            if blobs:
                all_blobs.extend(blobs)

        elif color_name == "GREEN":
            blobs = img.find_blobs(
                [GREEN_THRESHOLD],
                area_threshold=MATERIAL_MIN_AREA,
                pixels_threshold=MATERIAL_MIN_PIXELS,
                merge=True
            )
            if blobs:
                all_blobs.extend(blobs)

        elif color_name == "BLUE":
            blobs = img.find_blobs(
                [BLUE_THRESHOLD],
                area_threshold=MATERIAL_MIN_AREA,
                pixels_threshold=MATERIAL_MIN_PIXELS,
                merge=True
            )
            if blobs:
                all_blobs.extend(blobs)

        return all_blobs

    def _color_area(self, img, color_name: str) -> int:
        """统计指定颜色的总像素面积"""
        blobs = self._find_blobs(img, color_name)
        return sum(b[4] for b in blobs) if blobs else 0

    # -------------------------------------------------------
    # 调试绘制
    # -------------------------------------------------------
    def draw_material_overlay(self, img, pos, color_name: str):
        """绘制物料检测结果"""
        if not pos:
            return
        cx, cy, area, rotation = pos
        img.draw_circle(int(cx), int(cy), 5, image.COLOR_GREEN, 2)
        img.draw_cross(int(cx), int(cy), image.COLOR_RED, 10, 2)
        img.draw_string(int(cx) + 10, int(cy) - 10,
                        f"{color_name}", image.COLOR_GREEN, 1.5)

    def draw_zone_overlay(self, img, zone, color_name: str):
        """绘制色环区域检测结果"""
        if not zone:
            return
        cx, cy, radius, rings = zone
        img.draw_circle(int(cx), int(cy), radius, image.COLOR_BLUE, 2)
        img.draw_string(int(cx) + radius, int(cy),
                        f"{color_name} R:{rings}", image.COLOR_BLUE, 1.5)
