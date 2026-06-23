# 🏗️ 智能搬运机器人 — Smart Carrier Robot

[![Platform](https://img.shields.io/badge/MCU-STM32F407VE-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f407-8ve.html)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)](https://www.freertos.org/)
[![Vision](https://img.shields.io/badge/Vision-MaixCAM%20Pro-orange)](https://wiki.sipeed.com/maixcam-pro)
[![MaixPy](https://img.shields.io/badge/MaixPy-v4-blue)](https://wiki.sipeed.com/maixpy)
[![Encoder](https://img.shields.io/badge/Encoder-GMR%20500PPR-green)](https://github.com/Jacoleyyyy/Keshe)
[![Build](https://img.shields.io/badge/Build-CMake%2BNinja-cyan)](code/STM32/CMakeLists.txt)
[![AI](https://img.shields.io/badge/AI-MCP%20Ready-purple)](.mcp.json)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)

> 《机电智能控制综合设计》课程设计 — 完全自主运行的智能搬运机器人

**功能**：扫描二维码领取搬运任务 → 按编码顺序抓取红/绿/蓝三种物料 → 精确放置到粗加工区色环 → 暂存区三层码垛 → 自动返回启停区

**硬件**：WHEELTEC C30D 2.0 · STM32F407VE · GMR编码器(500PPR) · 4自由度机械臂 · 75mm轮径 · 60:1减速比  
**AI 能力**：MCP 串口直连 · 自动 PID 调参 · printf 日志实时诊断

---

## 📁 项目结构

```
Keshe/
├── rr.md                     ← 竞赛规则 + 课程设计要求
├── CLAUDE.md                 ← 完整项目架构文档
├── README.md                 ← 本文件
├── 教程-从零开始.md           ← 🔥 小白入门教程 (安装 → 配置 → 调试)
├── .mcp.json                 ← 🆕 MCP Server 配置 (AI 直连硬件)
├── .gitignore
│
├── .claude/
│   └── mcp-servers/
│       ├── serial_mcp.py     ← 🆕 串口 MCP (printf日志/调试命令)
│       └── pyocd_mcp.py      ← 🆕 烧录 MCP (编译/烧录一键)
│
└── code/
    ├── STM32/                ← 主控制器 (STM32F407VE + FreeRTOS)
    │   ├── Inc/  (12 headers)
    │   ├── Src/  (9 sources, ~6000 lines C)
    │   ├── .vscode/          ← VSCode 调试配置
    │   ├── CMakeLists.txt    ← CMake + Ninja 构建系统
    │   ├── .clangd           ← clangd 语言服务器
    │   ├── Makefile          ← ARM GCC 编译 (备选)
    │   └── VSCode调试指南.md
    │
    └── MaixCAM/              ← 视觉子系统 (MaixCAM Pro + MaixPy v4)
        ├── config.py
        ├── qr_detector.py
        ├── color_detector.py
        ├── lane_detector.py  ← 灰色车道检测 (防出界备选)
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

### 编译 & 烧录 (VSCode + CMake + Ninja)

```bash
cd code/STM32
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cp build/compile_commands.json .   # clangd 需要
cmake --build build -j8            # 编译
pyocd load build/smart_carrier.hex --target stm32f407ve  # 烧录
# 或者直接按 F5 启动 Cortex-Debug 调试
```

### 🤖 AI 模式 (Claude Code + MCP)

```bash
cd f:/KeShe && claude    # 启动 AI, MCP 自动加载
# AI 可以: 编译烧录 / 读串口日志 / 自动 PID 调参 / 诊断问题
```

---

## 🧠 系统架构

```
┌─────────────────┐     UART(115200)     ┌──────────────────┐
│   MaixCAM Pro   │ ◄──────────────────► │   STM32F407VE    │
│   MaixPy v4     │   CMD:/RSP: 协议     │   FreeRTOS       │
│                 │                      │                  │
│  • QR码扫描      │    串口直连 (MCP)    │  • 8个FreeRTOS任务│
│  • 颜色识别      │ ◄──────────────────► │  • 19状态状态机   │
│  • 物料定位      │   Claude Code       │  • 麦轮运动控制   │
│  • 车道检测      │   PID:/TST: 命令     │  • 4自由度机械臂  │
│  • 色环检测      │                      │  • 灰度+视觉车道  │
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
| 语言 (MCU) | C11 (Keil ARMCC + ARM GCC 13.x) |
| 视觉 | MaixCAM Pro + MaixPy v4 (`maix.camera/maix.image`) |
| 车道保持 | MaixCAM 视觉 (LAB 阈值) + 5路灰度 (双重保障) |
| 电机驱动 | 双通道PWM (TIM1/TIM9/TIM10/TIM11 @ 10kHz) |
| 编码器 | GMR 500PPR × 4 倍频 (TIM2/3/4/5), 120k 脉冲/转 |
| 舵机 | TIM8 4通道 100Hz (PC6-9) |
| 显示 | OLED SSD1306 SPI |
| 通信 | UART3 115200bps (PB10/11), CMD:/RSP: + PID:/TST: 双协议 |
| PID 调参 | 🆕 运行时可变 + 阶跃测试 + AI 自动收敛 |
| 调试 | Cortex-Debug + pyOCD + clangd + ST-Link V2 |
| AI 集成 | 🆕 MCP Server (serial + pyocd) → Claude Code 闭环 |

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
| 🆕 车道偏移告警 | 80 mm | 视觉车道检测偏移阈值 |
| 🆕 车道掉线判定 | 150 mm | 超过此偏移视为出界 |
| 🆕 PID 默认 Kp/Ki/Kd | 0.06 / 0.015 / 0.008 | 运行时可通过串口动态修改 |

---

## 📚 文档导航

- 🔥 **[新手入门教程](教程-从零开始.md)** — 零基础！安装→配置→调试, 含 AI PID 调参
- 🏁 [竞赛规则](rr.md) — 评分标准、场地规格、物料编码
- 📖 [完整项目文档](CLAUDE.md) — 架构设计、API、配置速查、硬件接线表
- 📸 [MaixPy v4 API 速查](code/MaixCAM/README.md) — 视觉 API + 颜色/车道阈值标定
- 🤖 [VSCode 现代化调试指南](code/STM32/VSCode调试指南.md) — CMake/clangd/pyOCD/MCP/AI PID调参
- 🔧 [MCP Server 配置](.mcp.json) — AI 直连串口+烧录 (serial + pyocd)
- 🔌 [引脚定义](code/STM32/Inc/pin_config.h) — 完整硬件接线表 (WHEELTEC C30D)
- 📡 [通信协议](code/STM32/Inc/protocol.h) — STM32↔MaixCAM + 串口调试命令 (PID:/TST:)
- 🎯 [状态机](code/STM32/Src/task_manager.c) — 19 状态自动搬运流程

---

## 👥 团队

课程设计小组项目，详见 [CLAUDE.md](CLAUDE.md)。

---

> ⚠️ **队友上手必读 [CLAUDE.md](CLAUDE.md)**，包含完整架构、配置速查表和调试技巧。
