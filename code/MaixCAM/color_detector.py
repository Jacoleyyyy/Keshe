"""
color_detector.py - 颜色识别与物料定位模块

功能:
  1. 识别红/绿/蓝三种颜色物料
  2. 定位物料在图像中的位置 (x, y)
  3. 识别粗加工区/暂存区的色环区域
  4. 检测色环环数 (用于放置精度评估)

依赖: MaixPy image 模块
"""

import math
import image
from config import *


class ColorDetector:
    """
    颜色检测器

    使用 LAB 色彩空间的阈值进行颜色分割
    """

    # 颜色名 → 颜色代码映射
    COLOR_MAP = {
        "RED":   1,
        "GREEN": 2,
        "BLUE":  3,
    }

    COLOR_NAMES = {1: "RED", 2: "GREEN", 3: "BLUE"}

    def __init__(self):
        self.img_center_x = CAMERA_WIDTH // 2
        self.img_center_y = CAMERA_HEIGHT // 2
        self.camera_fov_w = 60.0     # 相机水平视场角 (度)
        self.camera_fov_h = 45.0     # 相机垂直视场角 (度)
        self.camera_height_mm = 150.0  # 相机距地面高度 (mm)

    def detect_color(self, img) -> int:
        """
        检测图像中主要颜色

        Args:
            img: MaixPy image 对象

        Returns:
            颜色代码: 1=RED, 2=GREEN, 3=BLUE, 0=UNKNOWN
        """
        # 统计各颜色区域的像素面积
        red_area = self._count_color_area(img, "RED")
        green_area = self._count_color_area(img, "GREEN")
        blue_area = self._count_color_area(img, "BLUE")

        # 找出最大面积的颜色
        areas = [("RED", red_area), ("GREEN", green_area), ("BLUE", blue_area)]
        areas.sort(key=lambda x: x[1], reverse=True)

        if areas[0][1] > 0:
            return self.COLOR_MAP[areas[0][0]]
        return 0

    def find_material(self, img, target_color: int):
        """
        寻找指定颜色的物料并定位

        Args:
            img: MaixPy image 对象
            target_color: 目标颜色代码 (1/2/3)

        Returns:
            (x, y, area, angle) 或 None
            - x, y: 物料中心在图像中的像素坐标
            - area: 物料像素面积
            - angle: 物料主方向角度 (度)
        """
        color_name = self.COLOR_NAMES.get(target_color)
        if not color_name:
            return None

        # 找到颜色blob
        blobs = self._find_color_blobs(img, color_name)

        if not blobs:
            return None

        # 取最大的blob
        blobs.sort(key=lambda b: b.area(), reverse=True)
        blob = blobs[0]

        # 过滤过小/过大的区域
        if blob.area() < MATERIAL_MIN_AREA or blob.area() > MATERIAL_MAX_AREA:
            return None

        x = blob.cx()
        y = blob.cy()
        area = blob.area()

        # 计算角度 (使用 blob 的旋转角)
        # MaixPy blob.rotation() 返回 -180~180 度
        angle = blob.rotation_deg()

        return (x, y, area, angle)

    def find_zone(self, img, target_color: int):
        """
        寻找色环区域 (粗加工区/暂存区的颜色标记)

        Args:
            img: MaixPy image 对象
            target_color: 目标颜色

        Returns:
            (x, y, radius, ring_count) 或 None
            - ring_count: 色环环数 (精度指标)
        """
        color_name = self.COLOR_NAMES.get(target_color)
        if not color_name:
            return None

        # 先找到颜色区域
        blobs = self._find_color_blobs(img, color_name)
        if not blobs:
            return None

        # 找最大的
        blobs.sort(key=lambda b: b.area(), reverse=True)
        blob = blobs[0]

        x = blob.cx()
        y = blob.cy()

        # 估算色环半径
        radius = int(math.sqrt(blob.area() / math.pi))

        # 色环环数检测: 统计从中心向外的颜色过渡次数
        # 简化: 根据区域大小粗略估算
        ring_count = max(1, radius // 10)

        return (x, y, radius, ring_count)

    def pixel_to_world(self, px_x, px_y, img_w=CAMERA_WIDTH, img_h=CAMERA_HEIGHT):
        """
        像素坐标 → 世界坐标 (相对于相机, 单位mm)

        假设: 相机向下拍摄, 已知高度, 通过视场角计算实际位置

        Args:
            px_x: 像素X坐标
            px_y: 像素Y坐标
            img_w: 图像宽度
            img_h: 图像高度

        Returns:
            (x_mm, y_mm) 相对于相机正下方的坐标
        """
        # 像素偏移量 (相对于图像中心)
        dx = px_x - img_w / 2.0
        dy = px_y - img_h / 2.0

        # 每像素对应的实际尺寸
        # 实际宽度 = 2 * H * tan(FOV_h/2)
        real_w = 2.0 * self.camera_height_mm * math.tan(
            math.radians(self.camera_fov_w / 2.0))
        real_h = 2.0 * self.camera_height_mm * math.tan(
            math.radians(self.camera_fov_h / 2.0))

        mm_per_px_x = real_w / img_w
        mm_per_px_y = real_h / img_h

        x_mm = dx * mm_per_px_x
        y_mm = dy * mm_per_px_y

        return (int(x_mm), int(y_mm))

    def _find_color_blobs(self, img, color_name: str):
        """
        查找指定颜色的blob列表

        使用 LAB 颜色空间阈值进行颜色分割
        """
        blobs = []

        if color_name == "RED":
            # 红色有两个阈值区域
            b1 = img.find_blobs([RED_THRESHOLD_1], area_threshold=MATERIAL_MIN_AREA,
                                pixels_threshold=MATERIAL_MIN_AREA,
                                merge=True, margin=5)
            if b1:
                blobs.extend(b1)
            b2 = img.find_blobs([RED_THRESHOLD_2], area_threshold=MATERIAL_MIN_AREA,
                                pixels_threshold=MATERIAL_MIN_AREA,
                                merge=True, margin=5)
            if b2:
                blobs.extend(b2)

        elif color_name == "GREEN":
            b = img.find_blobs([GREEN_THRESHOLD], area_threshold=MATERIAL_MIN_AREA,
                               pixels_threshold=MATERIAL_MIN_AREA,
                               merge=True, margin=5)
            if b:
                blobs.extend(b)

        elif color_name == "BLUE":
            b = img.find_blobs([BLUE_THRESHOLD], area_threshold=MATERIAL_MIN_AREA,
                               pixels_threshold=MATERIAL_MIN_AREA,
                               merge=True, margin=5)
            if b:
                blobs.extend(b)

        return blobs

    def _count_color_area(self, img, color_name: str) -> int:
        """统计指定颜色的总像素面积"""
        blobs = self._find_color_blobs(img, color_name)
        return sum(b.area() for b in blobs) if blobs else 0

    def draw_material_overlay(self, img, pos, color_name: str):
        """
        绘制物料检测叠加层 (用于调试)
        """
        if pos:
            x, y, area, angle = pos
            img.draw_circle(x, y, 5, color=(0, 255, 0), thickness=2)
            img.draw_cross(x, y, color=(255, 0, 0), size=10, thickness=2)
            img.draw_string(x + 10, y - 10, f"{color_name} A:{area}",
                          color=(0, 255, 0), scale=1.5)

    def draw_zone_overlay(self, img, zone_pos, color_name: str):
        """
        绘制区域检测叠加层 (用于调试)
        """
        if zone_pos:
            x, y, radius, rings = zone_pos
            img.draw_circle(x, y, radius, color=(0, 0, 255), thickness=2)
            img.draw_string(x + radius, y, f"{color_name} R:{rings}",
                           color=(0, 0, 255), scale=1.5)
