"""
main.py - MaixCAM Pro 主程序

智能搬运机器人视觉系统主循环

流程:
  1. 初始化相机、显示、UART通信
  2. 进入主循环:
     - 读取 STM32 命令
     - 根据命令类型执行对应视觉任务
     - 返回处理结果

运行环境: MaixPy (MicroPython on MaixCAM Pro)
"""

import time
import sensor
import image
import lcd
from machine import UART

from config import *
from qr_detector import QRDetector
from color_detector import ColorDetector
from comm import MaixComm

# ============================================================
# 初始化
# ============================================================

def init_camera():
    """初始化相机"""
    print("[MaixCAM] Initializing camera...")
    try:
        sensor.reset()
        sensor.set_pixformat(sensor.RGB565)
        sensor.set_framesize(sensor.QVGA)  # 320x240
        sensor.set_vflip(True)              # 垂直翻转 (根据安装方向调整)
        sensor.set_hmirror(False)           # 水平镜像
        sensor.skip_frames(time=2000)       # 等待相机稳定

        # 自动设置
        sensor.set_auto_gain(True)
        sensor.set_auto_whitebal(True)
        sensor.set_auto_exposure(True)

        print("[MaixCAM] Camera initialized OK")
        return True
    except Exception as e:
        print(f"[MaixCAM] Camera init error: {e}")
        return False


def init_display():
    """初始化LCD显示"""
    try:
        lcd.init()
        lcd.rotation(2)  # 根据安装方向调整
        lcd.clear()
        lcd.draw_string(10, 10, "Smart Carrier Vision", lcd.WHITE, lcd.BLACK)
        lcd.draw_string(10, 30, "Initializing...", lcd.WHITE, lcd.BLACK)
        print("[MaixCAM] Display initialized OK")
        return True
    except Exception as e:
        print(f"[MaixCAM] Display init error: {e}")
        return False


def init_comm():
    """初始化UART通信"""
    comm = MaixComm(port=UART_PORT, baudrate=UART_BAUDRATE)
    print(f"[MaixCAM] UART init: port={UART_PORT}, baud={UART_BAUDRATE}")
    return comm


# ============================================================
# 命令处理
# ============================================================

def handle_scan_qr(img, qr_detector, comm):
    """
    处理 SCAN_QR 命令

    流程:
      1. 连续多帧扫描 QR 码
      2. 检测成功后返回编码
      3. 超时后返回错误
    """
    print("[MaixCAM] Scanning QR...")
    qr_detector.reset()

    timeout_ms = 3000
    start_time = time.ticks_ms()

    while time.ticks_diff(time.ticks_ms(), start_time) < timeout_ms:
        img = sensor.snapshot()
        code = qr_detector.scan(img)

        if code:
            # 绘制检测结果
            qr_detector.draw_qr_overlay(img, code)
            lcd.display(img)

            # 发送结果
            comm.send_qr_code(code)
            print(f"[MaixCAM] QR detected: {code}")
            return

        # 显示扫描状态
        img.draw_string(10, 10, "Scanning QR...", color=(255, 255, 0), scale=2)
        lcd.display(img)
        time.sleep_ms(50)

    # 超时
    print("[MaixCAM] QR scan timeout")
    comm.send_error("QR timeout")


def handle_detect_color(img, color_detector, comm):
    """
    处理 DETECT_COLOR 命令

    检测当前相机视野中的主要颜色
    """
    print("[MaixCAM] Detecting color...")

    color = color_detector.detect_color(img)
    color_name = color_detector.COLOR_NAMES.get(color, "UNKNOWN")

    if color > 0:
        comm.send_color_result(255, 255, 255, color_name)
        print(f"[MaixCAM] Color detected: {color_name}")
    else:
        comm.send_error("No color detected")

    # 显示结果
    color_detector.draw_material_overlay(img, None, color_name)
    lcd.display(img)


def handle_find_material(img, target_color, color_detector, comm):
    """
    处理 FIND_MATERIAL 命令

    在视野中寻找指定颜色的物料并定位

    Args:
        target_color: 目标颜色代码 (1=红, 2=绿, 3=蓝)
    """
    print(f"[MaixCAM] Finding material: color={target_color}")

    # 多帧检测以提高稳定性
    best_result = None
    best_area = 0

    for _ in range(5):  # 5帧检测
        img = sensor.snapshot()
        result = color_detector.find_material(img, target_color)

        if result:
            x, y, area, angle = result
            if area > best_area:
                best_area = area
                best_result = result

        time.sleep_ms(30)

    if best_result:
        x, y, area, angle = best_result
        # 像素坐标 → 世界坐标
        x_mm, y_mm = color_detector.pixel_to_world(x, y)

        comm.send_material_pos(x_mm, y_mm, target_color, int(angle))

        color_name = color_detector.COLOR_NAMES.get(target_color, "???")
        print(f"[MaixCAM] Material found: ({x_mm},{y_mm})mm, {color_name}, angle={angle:.0f}")

        # 显示
        color_detector.draw_material_overlay(img, best_result, color_name)
        img.draw_string(10, 10, f"Found {color_name}", color=(0, 255, 0), scale=2)
        lcd.display(img)
    else:
        print(f"[MaixCAM] Material not found")
        comm.send_error("Material not found")

        img.draw_string(10, 10, "Material not found", color=(255, 0, 0), scale=2)
        lcd.display(img)


def handle_check_zone(img, target_color, color_detector, comm):
    """
    处理 CHECK_ZONE 命令

    检查放置区域的色环位置和颜色

    Args:
        target_color: 目标颜色代码
    """
    print(f"[MaixCAM] Checking zone: color={target_color}")

    zone = color_detector.find_zone(img, target_color)

    if zone:
        x, y, radius, ring_count = zone
        x_mm, y_mm = color_detector.pixel_to_world(x, y)

        comm.send_zone_info(x_mm, y_mm, target_color)

        color_name = color_detector.COLOR_NAMES.get(target_color, "???")
        print(f"[MaixCAM] Zone found: ({x_mm},{y_mm})mm, rings={ring_count}")

        # 显示
        color_detector.draw_zone_overlay(img, zone, color_name)
        img.draw_string(10, 10, f"Zone {color_name} R={ring_count}",
                       color=(0, 255, 0), scale=2)
        lcd.display(img)
    else:
        print("[MaixCAM] Zone not found")
        comm.send_error("Zone not found")

        img.draw_string(10, 10, "Zone not found", color=(255, 0, 0), scale=2)
        lcd.display(img)


# ============================================================
# 主循环
# ============================================================

def main():
    """主函数"""

    print("=" * 40)
    print("  Smart Carrier - MaixCAM Vision System")
    print("  智能搬运机器人 视觉系统")
    print("=" * 40)

    # 初始化各模块
    if not init_camera():
        print("Camera init failed! Halted.")
        return

    if not init_display():
        print("Display init failed! Continuing...")

    comm = init_comm()

    # 初始化检测器
    qr_detector = QRDetector()
    color_detector = ColorDetector()

    # 发送就绪信号
    time.sleep_ms(500)
    comm.send_ready()
    print("[MaixCAM] System ready. Waiting for commands...")

    # 显示就绪状态
    try:
        img = sensor.snapshot()
        img.draw_string(10, 50, "Vision System Ready",
                       color=(0, 255, 0), scale=2)
        img.draw_string(10, 80, "Waiting for STM32...",
                       color=(255, 255, 255), scale=1.5)
        lcd.display(img)
    except Exception:
        pass

    # 主循环
    while True:
        try:
            # 获取相机图像
            img = sensor.snapshot()

            # 检查UART新命令
            if comm.update():
                cmd, params = comm.get_command()

                print(f"[MaixCAM] Command received: {cmd}, params: {params}")

                # 根据命令分发
                if cmd == CMD_SCAN_QR:
                    handle_scan_qr(img, qr_detector, comm)

                elif cmd == CMD_DETECT_COLOR:
                    handle_detect_color(img, color_detector, comm)

                elif cmd == CMD_FIND_MATERIAL:
                    # 解析目标颜色参数
                    try:
                        target_color = int(params) if params else 1
                    except ValueError:
                        target_color = 1
                    handle_find_material(img, target_color, color_detector, comm)

                elif cmd == CMD_CHECK_ZONE:
                    try:
                        target_color = int(params) if params else 1
                    except ValueError:
                        target_color = 1
                    handle_check_zone(img, target_color, color_detector, comm)

                elif cmd == CMD_HEARTBEAT:
                    comm.send_ready()
                    print("[MaixCAM] Heartbeat ACK")

                elif cmd == CMD_READY:
                    comm.send_ready()

                else:
                    print(f"[MaixCAM] Unknown command: {cmd}")
                    comm.send_error(f"Unknown: {cmd}")

            else:
                # 无命令时的空闲显示
                # 每500ms刷新一次显示
                if time.ticks_ms() % 500 < 10:
                    img.draw_string(10, 100, "Idle...",
                                  color=(200, 200, 200), scale=1.5)
                    lcd.display(img)

            # 周期检查
            comm.check_heartbeat()
            time.sleep_ms(10)

        except Exception as e:
            print(f"[MaixCAM] Main loop error: {e}")
            try:
                comm.send_error(f"Internal: {str(e)}")
            except Exception:
                pass
            time.sleep_ms(100)


# ============================================================
# 入口
# ============================================================
if __name__ == "__main__":
    main()
