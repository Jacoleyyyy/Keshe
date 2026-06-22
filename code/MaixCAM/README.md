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
| `config.py` | 相机/UART/颜色阈值/检测参数 |
| `qr_detector.py` | QR码检测 (maix.image.Image.find_qrcodes) |
| `color_detector.py` | LAB颜色分割 + 物料定位 + 像素→世界坐标 |
| `comm.py` | UART帧协议: CMD:/RSP: |
| `main.py` | 主循环: 命令分发 + 多帧检测 |
