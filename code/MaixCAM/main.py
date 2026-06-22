"""
main.py — MaixCAM Pro 视觉系统主程序 (MaixPy v4)

智能搬运机器人视觉子系统
功能: QR码读取 / 颜色识别 / 物料定位 / 色环检测

运行环境: MaixCAM Pro + MaixPy v4.x
API: maix.camera / maix.image / maix.display / machine.UART

启动方式:
  1. 将此文件复制到 MaixCAM U盘根目录
  2. 或在 MaixPy IDE 中直接运行
  3. 上电自动执行 main.py
"""

import time
from maix import camera, image, display, app
from config import *
from qr_detector import QRDetector
from color_detector import ColorDetector
from comm import MaixComm


# ============================================================
# 全局对象
# ============================================================
cam  = None        # camera.Camera
disp = None        # display.Display
comm = None        # MaixComm
qr_detector = None # QRDetector
color_detector = None  # ColorDetector


# ============================================================
# 初始化
# ============================================================
def init_all() -> bool:
    """初始化全部模块, 返回 True=成功"""
    global cam, disp, comm, qr_detector, color_detector

    print("=" * 40)
    print("  Smart Carrier — MaixCAM Vision")
    print("=" * 40)

    # 1. 相机
    try:
        cam = camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)
        print(f"[OK] Camera: {CAMERA_WIDTH}x{CAMERA_HEIGHT}")
    except Exception as e:
        print(f"[FAIL] Camera: {e}")
        return False

    # 2. 显示屏
    try:
        disp = display.Display()
        disp.show(cam.read())  # 测试显示
        print("[OK] Display")
    except Exception as e:
        print(f"[WARN] Display: {e}")
        disp = None

    # 3. UART 通信
    try:
        comm = MaixComm(port=UART_PORT, baudrate=UART_BAUDRATE)
        print(f"[OK] UART: port={UART_PORT}, baud={UART_BAUDRATE}")
    except Exception as e:
        print(f"[FAIL] UART: {e}")
        return False

    # 4. 检测器
    qr_detector = QRDetector()
    color_detector = ColorDetector()
    print("[OK] Detectors initialized")

    return True


# ============================================================
# 命令处理函数
# ============================================================

def cmd_scan_qr(timeout_ms=3000):
    """处理 SCAN_QR 命令"""
    print("[CMD] SCAN_QR")
    qr_detector.reset()

    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < timeout_ms:
        img = cam.read()
        code = qr_detector.scan(img)

        if code:
            qr_detector.draw_overlay(img, code)
            show_img(img)
            comm.send_qr_code(code)
            print(f"  -> QR: {code}")
            return

        # 显示扫描中
        img.draw_string(10, 10, "Scanning QR...",
                        image.COLOR_YELLOW, 2)
        show_img(img)
        time.sleep_ms(50)

    # 超时
    print("  -> timeout")
    comm.send_error("QR timeout")


def cmd_detect_color():
    """处理 DETECT_COLOR 命令"""
    print("[CMD] DETECT_COLOR")
    img = cam.read()
    c = color_detector.detect_color(img)
    name = color_detector.COLOR_NAMES.get(c, "UNKNOWN")

    if c > 0:
        # 协议格式: COLOR,<r>,<g>,<b>,<name>
        comm.send_response(RSP_COLOR_RESULT, f"0,0,0,{name}")
        print(f"  -> {name}")
    else:
        comm.send_error("No color detected")
        print("  -> no color")

    img.draw_string(10, 10, f"Color: {name}",
                    image.COLOR_GREEN, 2)
    show_img(img)


def cmd_find_material(params: str):
    """处理 FIND_MATERIAL 命令

    params: 目标颜色代码 "1"/"2"/"3"
    """
    try:
        target = int(params) if params else 1
    except ValueError:
        target = 1

    name = color_detector.COLOR_NAMES.get(target, "???")
    print(f"[CMD] FIND_MATERIAL color={target} ({name})")

    # 多帧检测取最佳
    best = None
    best_area = 0

    for _ in range(5):
        img = cam.read()
        r = color_detector.find_material(img, target)
        if r:
            _, _, area, _ = r
            if area > best_area:
                best_area = area
                best = r
        time.sleep_ms(30)

    if best:
        cx, cy, area, rotation = best
        x_mm, y_mm = color_detector.pixel_to_world(cx, cy)
        comm.send_material_pos(x_mm, y_mm, target, int(rotation))
        print(f"  -> pos=({x_mm},{y_mm})mm angle={rotation:.0f}")

        color_detector.draw_material_overlay(img, best, name)
        img.draw_string(10, 10, f"Found {name}", image.COLOR_GREEN, 2)
        show_img(img)
    else:
        print("  -> not found")
        comm.send_error("Material not found")
        img.draw_string(10, 10, "Not found", image.COLOR_RED, 2)
        show_img(img)


def cmd_check_zone(params: str):
    """处理 CHECK_ZONE 命令"""
    try:
        target = int(params) if params else 1
    except ValueError:
        target = 1

    name = color_detector.COLOR_NAMES.get(target, "???")
    print(f"[CMD] CHECK_ZONE color={target} ({name})")

    img = cam.read()
    zone = color_detector.find_zone(img, target)

    if zone:
        cx, cy, radius, rings = zone
        x_mm, y_mm = color_detector.pixel_to_world(cx, cy)
        comm.send_zone_info(x_mm, y_mm, target)
        print(f"  -> pos=({x_mm},{y_mm})mm rings={rings}")

        color_detector.draw_zone_overlay(img, zone, name)
        img.draw_string(10, 10, f"Zone {name} R={rings}",
                        image.COLOR_GREEN, 2)
        show_img(img)
    else:
        print("  -> not found")
        comm.send_error("Zone not found")
        img.draw_string(10, 10, "Zone not found", image.COLOR_RED, 2)
        show_img(img)


# ============================================================
# 辅助
# ============================================================
def show_img(img):
    """显示图像 (如果 display 可用)"""
    if disp:
        disp.show(img)


# ============================================================
# 主循环
# ============================================================
def main():
    if not init_all():
        print("Init failed! Check hardware.")
        return

    # 发送就绪
    time.sleep_ms(500)
    comm.send_ready()
    print("[INFO] System ready, waiting commands...")

    # 显示就绪画面
    img = cam.read()
    img.draw_string(10, 50, "Vision Ready",
                    image.COLOR_GREEN, 2)
    img.draw_string(10, 80, "Waiting STM32...",
                    image.COLOR_WHITE, 1.5)
    show_img(img)

    # === 主循环 ===
    while not app.need_exit():
        try:
            if comm.update():
                cmd, params = comm.get_command()
                print(f"[RX] {cmd} | {params}")

                if cmd == CMD_SCAN_QR:
                    cmd_scan_qr()

                elif cmd == CMD_DETECT_COLOR:
                    cmd_detect_color()

                elif cmd == CMD_FIND_MATERIAL:
                    cmd_find_material(params)

                elif cmd == CMD_CHECK_ZONE:
                    cmd_check_zone(params)

                elif cmd == CMD_HEARTBEAT:
                    comm.send_ready()
                    comm.last_heartbeat = time.ticks_ms()

                elif cmd == CMD_READY:
                    comm.send_ready()

                else:
                    print(f"[WARN] Unknown command: {cmd}")
                    comm.send_error(f"Unknown:{cmd}")

            else:
                # 空闲: 降低 CPU
                time.sleep_ms(10)

            # 心跳检查
            comm.check_heartbeat()

        except Exception as e:
            print(f"[ERR] Main loop: {e}")
            try:
                comm.send_error(f"Exception:{e}")
            except Exception:
                pass
            time.sleep_ms(200)


# ============================================================
# 入口
# ============================================================
if __name__ == "__main__":
    main()
