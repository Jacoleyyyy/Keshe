# MaixCAM Pro 视觉子系统 (MaixPy v4)

## API 速查

| 模块 | 导入 | 用途 |
|------|------|------|
| 相机 | `from maix import camera` | `camera.Camera(w, h)` → `.read()` |
| 图像 | `from maix import image` | `.find_blobs()` / `.find_qrcodes()` / 绘制 |
| 显示 | `from maix import display` | `display.Display()` → `.show(img)` |
| 串口 | `from machine import UART` | `UART(port, baud)` → `.read()` / `.write()` |
| 系统 | `from maix import app` | `app.need_exit()` — 程序退出检测 |

## find_blobs 返回格式

```python
blobs = img.find_blobs([threshold], area_threshold=200, pixels_threshold=200)
# 每个 blob = [x, y, w, h, area, cx, cy, rotation]
#             0  1  2  3   4    5   6     7
```

## find_qrcodes 返回格式

```python
qrs = img.find_qrcodes()
# qr.payload()  → 解码字符串
# qr.x(), qr.y() → 左上角
# qr.corners() → [(x0,y0), (x1,y1), (x2,y2), (x3,y3)]
```

## LAB 颜色阈值标定

使用 MaixCAM 内置「找色块」应用进行标定:
1. 开机 → 桌面 → 找色块 App
2. 选择 Red/Green/Blue 预设
3. 对准物料 → 点击屏幕自动设置阈值
4. 记录 L/A/B 四组数值 → 填入 config.py

## 文件说明

| 文件 | 功能 |
|------|------|
| `config.py` | 相机/UART/颜色阈值/车道检测参数 |
| `qr_detector.py` | QR码检测 (maix.image.Image.find_qrcodes) |
| `color_detector.py` | LAB颜色分割 + 物料定位 + 像素→世界坐标 |
| `lane_detector.py` | 灰色车道检测 (备选, 视觉车道保持防出界) |
| `comm.py` | UART帧协议: CMD:/RSP: |
| `main.py` | 主循环: 命令分发 + 多帧检测 + 车道查询 |

## 车道检测协议

```
STM32 发送:  CMD:CHECK_LANE\r\n
MaixCAM 回复: RSP:LANE,<offset_mm>,<on_lane>\r\n
```

## STM32 串口调试命令 (USART3, 115200)

以下命令发给 STM32 而非 MaixCAM，用于运行时 PID 调参：

| 命令 | 功能 |
|------|------|
| `PID:?\r\n` | 查询当前 Kp/Ki/Kd |
| `PID:0.07,0.018,0.012\r\n` | 设置参数，立即生效 |
| `TST:0,100,300\r\n` | 阶跃测试 (电机A, 100RPM, 300ms)，输出 CSV |

> 详见 [CLAUDE.md](../../CLAUDE.md) §十五
