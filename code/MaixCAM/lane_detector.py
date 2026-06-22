"""
lane_detector.py — MaixCAM Pro 车道检测模块 (MaixPy v4)

功能:
  1. 检测灰色车道区域 (基于 LAB 色彩空间)
  2. 计算车道中心相对于相机中心的偏移量
  3. 判断是否偏离车道

原理:
  灰色车道在 LAB 空间中: L 中等 (30~75), A/B 接近 0 (低饱和度)
  白色/黄色出界区域: L 很高 (>80)
  所以用中等亮度 + 低饱和度的 blob 找到车道区域,
  计算其质心与图像中心的横向偏移。

API: maix.image.Image.find_blobs()
"""

from maix import image
from config import *
import math


class LaneDetector:
    """
    车道检测器

    使用 find_blobs 检测灰色车道区域, 计算偏移量
    """

    def __init__(self):
        self.img_w = CAMERA_WIDTH
        self.img_h = CAMERA_HEIGHT
        self.camera_height_mm = 150.0
        self.camera_fov_w = 60.0

        # 车道宽度 (mm) — 400mm 或 450mm
        self.lane_width_mm = 400.0

        # 历史偏移 (低通滤波)
        self.offset_history = 0.0
        self.filter_alpha = 0.7     # 滤波系数 (越大越灵敏)

        # 掉线检测
        self.lost_frames = 0
        self.max_lost_frames = 3    # 连续N帧丢失才报警

    # -------------------------------------------------------
    # 主检测
    # -------------------------------------------------------
    def check_lane(self, img):
        """
        检测车道位置

        Args:
            img: maix.image.Image 对象

        Returns:
            LaneResult 或 None
        """
        result = LaneResult()

        # 1. 在图像下半部分寻找灰色车道区域
        blobs = self._find_lane_blobs(img)

        if not blobs:
            self.lost_frames += 1
            if self.lost_frames >= self.max_lost_frames:
                result.on_lane = False
                result.offset_mm = 0
                result.warning = "No lane detected"
                return result
            # 还没到报警阈值, 返回上次结果
            result.on_lane = True
            result.offset_mm = int(self.offset_history)
            result.lane_width_px = 0
            return result

        self.lost_frames = 0
        result.on_lane = True

        # 2. 取面积最大的 blob (车道区域)
        blobs.sort(key=lambda b: b[4], reverse=True)
        blob = blobs[0]
        x, y, w, h, area, cx, cy, _ = blob

        # 3. 计算中心偏移 (像素)
        img_center_x = self.img_w / 2.0
        offset_px = cx - img_center_x          # + = 偏右 (车道在右边 → 机器人偏左)

        # 4. 像素偏移 → mm 偏移
        offset_mm = self._pixel_to_mm(offset_px)

        # 5. 低通滤波平滑
        self.offset_history = (self.filter_alpha * offset_mm +
                               (1.0 - self.filter_alpha) * self.offset_history)

        result.offset_mm = int(self.offset_history)
        result.lane_width_px = w
        result.lane_blob = blob

        # 6. 判断是否严重偏移 (车道边缘接近图像边缘)
        margin_left = cx - x          # blob 左边界距中心
        margin_right = (x + w) - cx   # blob 右边界距中心
        if margin_left < w * 0.15 or margin_right < w * 0.15:
            result.warning = "Near edge"

        return result

    # -------------------------------------------------------
    # 调试: 在图像上绘制检测结果
    # -------------------------------------------------------
    def draw_overlay(self, img, result):
        """绘制车道检测叠加层"""
        if result is None:
            return

        # 绘制车道 blob
        if result.lane_blob:
            x, y, w, h, _, cx, cy, _ = result.lane_blob
            img.draw_rect(x, y, w, h, image.COLOR_GREEN, 2)
            img.draw_cross(int(cx), int(cy), image.COLOR_RED, 10, 2)

        # 绘制图像中心线
        cx = self.img_w // 2
        img.draw_line(cx, self.img_h // 2 + 30, cx, self.img_h // 2 - 30,
                      image.COLOR_WHITE, 1)

        # 绘制偏移信息
        status = "ON LANE" if result.on_lane else "OFF LANE!"
        color = image.COLOR_GREEN if result.on_lane else image.COLOR_RED
        img.draw_string(10, 10, f"Lane: {status}", color, 1.5)
        img.draw_string(10, 30,
                        f"Offset: {result.offset_mm}mm",
                        image.COLOR_YELLOW, 1.5)

        if result.warning:
            img.draw_string(10, 50, result.warning, image.COLOR_RED, 1.5)

    # -------------------------------------------------------
    # 内部方法
    # -------------------------------------------------------
    def _find_lane_blobs(self, img):
        """
        在图像中寻找灰色车道 blob

        搜索区域: 图像下半部分 (靠近机器人的地面)
        """
        # 仅搜索图像下半部分 (ROI)
        roi_y = self.img_h // 2
        roi_h = self.img_h - roi_y
        roi = [0, roi_y, self.img_w, roi_h]

        blobs = img.find_blobs(
            [GREY_LANE_THRESHOLD],
            roi=roi,
            area_threshold=LANE_MIN_AREA,
            pixels_threshold=LANE_MIN_PIXELS,
            merge=True
        )
        return blobs

    def _pixel_to_mm(self, px_offset: float) -> float:
        """像素偏移 → 毫米偏移"""
        real_w = (2.0 * self.camera_height_mm *
                  math.tan(math.radians(self.camera_fov_w / 2.0)))
        mm_per_px = real_w / self.img_w
        return px_offset * mm_per_px


class LaneResult:
    """车道检测结果"""
    def __init__(self):
        self.on_lane = True         # 是否在车道上
        self.offset_mm = 0          # 中心偏移 (mm), + = 偏右
        self.lane_width_px = 0      # 检测到的车道宽度 (像素)
        self.warning = ""           # 告警信息 ("Near edge", "No lane detected", "")
        self.lane_blob = None       # 车道 blob 引用 (调试用)
