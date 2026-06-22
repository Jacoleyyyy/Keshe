# 🏗️ 智能搬运机器人 — Smart Carrier Robot

[![Platform](https://img.shields.io/badge/MCU-STM32F407-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)](https://www.freertos.org/)
[![Vision](https://img.shields.io/badge/Vision-MaixCAM%20Pro-orange)](https://wiki.sipeed.com/maixcam-pro)
[![MaixPy](https://img.shields.io/badge/MaixPy-v4-blue)](https://wiki.sipeed.com/maixpy)
[![Encoder](https://img.shields.io/badge/Encoder-GMR%20500PPR-green)](https://github.com/Jacoleyyyy/Keshe)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)

> 《机电智能控制综合设计》课程设计 — 完全自主运行的智能搬运机器人

**功能**：扫描二维码领取搬运任务 → 按编码顺序抓取红/绿/蓝三种物料 → 精确放置到粗加工区色环 → 暂存区三层码垛 → 自动返回启停区

**硬件**：WHEELTEC C30D 2.0 麦轮底盘 · GMR编码器(500PPR) · 4自由度机械臂 · 75mm轮径 · 60:1减速比

---

## 📁 项目结构

```
Keshe/
├── rr.md                     ← 竞赛规则 + 课程设计要求
├── CLAUDE.md                 ← 完整项目架构文档
├── README.md                 ← 本文件
├── 教程-从零开始.md           ← 🔥 小白入门教程 (安装 → 配置 → 调试)
├── .gitignore
│
└── code/
    ├── STM32/                ← 主控制器 (STM32F407 + FreeRTOS)
    │   ├── Inc/  (12 headers)
    │   ├── Src/  (9 sources, ~5700 lines C)
    │   ├── .vscode/          ← VSCode 调试配置
    │   ├── Makefile          ← ARM GCC 编译
    │   └── VSCode调试指南.md
    │
    └── MaixCAM/              ← 视觉子系统 (MaixCAM Pro + MaixPy v4)
        ├── config.py
        ├── qr_detector.py
        ├── color_detector.py
        ├── comm.py
        ├── main.py
        └── README.md         ← MaixPy v4 API 速查

---

## 🚀 快速开始

### 前置条件

| 工具 | 版本 | 说明 |
|------|------|------|
| Keil5 | MDK-ARM 5.x | IDE 和 ARMCC 编译器 |
| STM32CubeF4 | 最新 | HAL 库 + CMSIS + FreeRTOS |
| ARM GCC | 13.x | (可选) VSCode 编译 |
| OpenOCD | 0.12+ | (可选) VSCode 调试 |
| ST-Link | V2 | 调试器/烧录器 |

### 编译 & 烧录 (Keil5)

```
1. 将 code/STM32/Inc/ 和 code/STM32/Src/ 加入 Keil 工程
2. 在 Keil 工程中添加 HAL 库、CMSIS、FreeRTOS 源文件
3. F7 编译 → F8 下载
```

### 编译 & 烧录 (VSCode + ARM GCC)

```bash
cd code/STM32
make -j8        # 编译
make flash      # 烧录
# 或者直接按 F5 启动调试
```

---

## 🧠 系统架构

```
┌─────────────────┐     UART(115200)     ┌──────────────────┐
│   MaixCAM Pro   │ ◄──────────────────► │   STM32F407      │
│                 │   CMD:/RSP: 协议     │                  │
│  • QR码扫描      │                      │  • 8个FreeRTOS任务│
│  • 颜色识别      │                      │  • 19状态状态机   │
│  • 物料定位      │                      │  • 麦轮运动控制   │
│  • 色环检测      │                      │  • 4自由度机械臂  │
│                 │                      │  • 灰度巡线+避障  │
└─────────────────┘                      └──────────────────┘
```

---

## 📋 竞赛流程

```
启动 → 扫QR码 → [原料区→粗加工区]×3 → [粗加工区→暂存区码垛]×3 → 返回启停区 → 完成
```

**任务编码**：三位数字 `1/2/3` = 🔴红 / 🟢绿 / 🔵蓝，表示搬运顺序

---

## 🛠️ 技术栈

| 层级 | 技术 |
|------|------|
| MCU | STM32F407 (ARM Cortex-M4 @ 168MHz) |
| RTOS | FreeRTOS v10 (CMSIS OS v2) |
| 语言 (MCU) | C11 (Keil ARMCC + ARM GCC) |
| 视觉 | MaixCAM Pro + **MaixPy v4** (`maix.camera/maix.image`) |
| 电机驱动 | 双通道PWM (TIM1/TIM9/TIM10/TIM11 @ 10kHz) |
| 编码器 | GMR 500PPR × 4 倍频 (TIM2/3/4/5) |
| 舵机 | TIM8 4通道 100Hz |
| 显示 | OLED SSD1306 SPI (PD11-14) |
| 通信 | UART3 115200 bps (PB10/PB11), CMD:/RSP: 帧协议 |
| 控制 | 增量式/位置式 PID (1kHz 更新率) |
| 调试 | Cortex-Debug + OpenOCD + ST-Link V2 |

---

## 🔧 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 赛场尺寸 | 2400×2400 mm | 正方形平面区域 |
| 车道宽度 | 400 / 450 mm | 灰色车道线 |
| 机器人投影 | ≤ 300×300 mm | 铅垂方向 |
| 机器人高度 | ≤ 400 mm | 含机械臂 |
| 最大线速度 | 300 mm/s | 可调整 |
| 定位容差 | 10 mm | 点对点导航 |
| 避障距离 | 200 mm | 超声波触发 |
| **轮径** | **75 mm** | 麦轮 Mecanum_75 |
| **减速比** | **60:1** | GMR 编码器电机 |
| **编码器精度** | **500 PPR** | ×4倍频 = 2000 CPR |
| **脉冲/轮转** | **120,000** | 500×4×60 |
| **半轮距/半轴距** | **93 / 85 mm** | 麦轮运动学参数 |

---

## 📚 文档导航

- 🔥 **[新手入门教程](教程-从零开始.md)** — 零基础必看！安装→配置→调试全程带飞
- 🏁 [竞赛规则](rr.md) — 评分标准、场地规格、物料编码
- 📖 [完整项目文档](CLAUDE.md) — 架构设计、API、配置速查表
- 📸 [MaixPy v4 API 速查](code/MaixCAM/README.md) — 视觉 API + 颜色阈值标定
- 🔬 [VSCode 调试指南](code/STM32/VSCode调试指南.md) — 从 Keil5 迁移到 VSCode
- 🔌 [引脚定义](code/STM32/Inc/pin_config.h) — 硬件连接 (WHEELTEC C30D GMR版)
- 📡 [通信协议](code/STM32/Inc/protocol.h) — STM32 ↔ MaixCAM 帧协议
- 🎯 [状态机](code/STM32/Src/task_manager.c) — 19 状态自动搬运流程

---

## 👥 团队

课程设计小组项目，详见 [CLAUDE.md](CLAUDE.md)。

---

> ⚠️ **队友上手必读 [CLAUDE.md](CLAUDE.md)**，包含完整架构、配置速查表和调试技巧。
