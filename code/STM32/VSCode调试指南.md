# STM32F407 VSCode 现代化开发指南

> **目标**：从 Keil5 迁移到 VSCode，打通 GCC / CMake / clangd / pyOCD / Cortex-Debug / AI 全链路  
> **GitHub**：https://github.com/Jacoleyyyy/Keshe

---

## 目录

- [一、工具链全景图](#一工具链全景图)
- [二、环境安装（每一步都有验证）](#二环境安装每一步都有验证)
  - [2.1 ARM GNU Toolchain（编译工具链）](#21-arm-gnu-toolchain编译工具链)
  - [2.2 CMake + Ninja（构建系统）](#22-cmake--ninja构建系统)
  - [2.3 clangd（语法服务器）](#23-clangd语法服务器)
  - [2.4 pyOCD / OpenOCD（烧录调试）](#24-pyocd--openocd烧录调试)
  - [2.5 VSCode 插件](#25-vscode-插件)
  - [2.6 STM32CubeF4 库文件](#26-stm32cubef4-库文件)
- [三、首次配置（10分钟）](#三首次配置10分钟)
  - [3.1 编译验证](#31-编译验证)
  - [3.2 烧录验证](#32-烧录验证)
  - [3.3 调试验证](#33-调试验证)
- [四、日常操作](#四日常操作)
  - [4.1 编译 / 烧录 / 调试 / 清理](#41-编译--烧录--调试--清理)
  - [4.2 调试界面与快捷键](#42-调试界面与快捷键)
  - [4.3 Watch / 断点 / 外设寄存器](#43-watch--断点--外设寄存器)
  - [4.4 FreeRTOS 线程调试](#44-freertos-线程调试)
- [五、进阶：AI 辅助开发 & 串口诊断](#五进阶ai-辅助开发--串口诊断)
  - [5.1 总体架构](#51-总体架构)
  - [5.2 安装 Claude Code / OpenCode](#52-安装-claude-code--opencode)
  - [5.3 串口接入 AI](#53-串口接入-ai)
  - [5.4 AI 自动日志诊断](#54-ai-自动日志诊断)
  - [5.5 AI 闭环调试工作流](#55-ai-闭环调试工作流)
- [六、常见问题排查](#六常见问题排查)
- [七、与 Keil5 对比速查](#七与-keil5-对比速查)

---

## 一、工具链全景图

```
┌─────────────────────────────────────────────────────────────┐
│                        VSCode IDE                           │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌────────────┐  ┌───────────┐ │
│  │  clangd  │  │  CMake   │  │Cortex-Debug│  │   AI 工具  │ │
│  │ 语法高亮  │  │ 构建系统  │  │  调试前端   │  │ 自动诊断   │ │
│  │ 补全跳转  │  │ Ninja   │  │GDB 图形界面 │  │ 串口日志   │ │
│  └────┬─────┘  └────┬─────┘  └─────┬──────┘  └─────┬─────┘ │
│       │             │              │               │        │
└───────┼─────────────┼──────────────┼───────────────┼────────┘
        │             │              │               │
        ▼             ▼              ▼               ▼
   arm-none-eabi   Ninja /      pyOCD /          Claude Code
      -gcc        MinGW Make    OpenOCD          OpenCode
   (ARM GCC)                     (GDB Server)    (AI Agent)
        │                            │
        ▼                            ▼
   .elf / .hex / .bin           ST-Link V2
        │                            │
        └────────────┬───────────────┘
                     ▼
              STM32F407 MCU
```

**组件职责：**

| 组件 | 角色 | 替代了 Keil5 的什么 |
|------|------|:---:|
| **ARM GNU Toolchain** | C 编译器 + GDB 调试器 | ARMCC + 自带调试器 |
| **CMake + Ninja** | 构建系统 (替代 Makefile) | Keil5 工程 (.uvprojx) |
| **clangd** | 语法服务器 (补全/跳转/诊断) | Keil 内置 IntelliSense |
| **pyOCD** | GDB Server (烧录+调试桥) | ST-Link 驱动 + Flash Download |
| **Cortex-Debug** | VSCode 内调试界面 | Keil 调试窗口 |
| **Claude Code / OpenCode** | AI 辅助 (串口诊断+代码生成+自动修bug) | — 新能力 — |

---

## 二、环境安装（每一步都有验证）

> **前提**：Windows 10/11，64 位  
> **推荐**：使用 Git Bash 作为终端（安装 Git for Windows 时选"Git Bash"）

---

### 2.1 ARM GNU Toolchain（编译工具链）

**下载**：https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

在页面上找到 **"AArch32 bare-metal target (arm-none-eabi)"**，选择：

- Windows (mingw-w64-i686) → `.exe` 安装包
- 推荐版本：**13.3.rel1** 或更新

**安装**：

1. 双击 `.exe` 安装
2. ⚠️ **关键**：安装到最后一步，**必须勾选 "Add path to environment variable"**
3. 安装路径建议：`C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\`

**验证**（打开 Git Bash）：

```bash
arm-none-eabi-gcc --version
# 应输出: arm-none-eabi-gcc.exe (Arm GNU Toolchain ...) 13.3.1 ...

arm-none-eabi-gdb --version
# 应输出: GNU gdb (Arm GNU Toolchain ...) ...

arm-none-eabi-objcopy --version
# 应输出: GNU objcopy ...
```

---

### 2.2 CMake + Ninja（构建系统）

**CMake**：https://cmake.org/download/ — 下载 Windows x64 Installer

```bash
# 安装后验证
cmake --version
# 应输出: cmake version 3.30.x
```

**Ninja**（推荐，增量编译极快）：

```bash
# 方式一: scoop (推荐)
scoop install ninja

# 方式二: 直接下载 exe → 放到 PATH 里
# https://github.com/ninja-build/ninja/releases

# 验证
ninja --version
# 应输出: 1.12.x
```

> 如果没有 Ninja，也可以用 CMake 自带的 MinGW Makefiles：
> ```bash
> cmake -B build -G "MinGW Makefiles"
> ```
> 但 Ninja 速度更快（比 Make 快 30%~50%，尤其增量编译）。

---

### 2.3 clangd（语法服务器）

clangd 提供比 MS C/C++ 插件**快 10 倍**的代码补全、跳转和实时诊断。

**安装**：

```bash
scoop install llvm          # LLVM 全家桶 (包含 clangd)
# 或只装 clangd
# 下载: https://github.com/clangd/clangd/releases
```

**验证**：

```bash
clangd --version
# 应输出: clangd version 19.x.x
```

---

### 2.4 pyOCD / OpenOCD（烧录调试）

**pyOCD**（推荐，Python 生态，更现代，启动快）：

```bash
# 安装 Python 后:
pip install pyocd

# 验证
pyocd --version
# 应输出: pyocd 0.36.x
```

**OpenOCD**（备选，经典方案）：

```bash
scoop install openocd
# 或从 https://github.com/openocd-org/openocd/releases 下载

openocd --version
```

---

### 2.5 VSCode 插件

在 VSCode 扩展市场 `Ctrl+Shift+X` 中搜索安装：

| 插件 | 用途 | 必装 |
|------|------|:---:|
| **Cortex-Debug** (`marus25.cortex-debug`) | ARM MCU 调试核心 | ✅ |
| **clangd** (`llvm-vs-code-extensions.vscode-clangd`) | C 语言语法服务器 | ✅ |
| **CMake Tools** (`ms-vscode.cmake-tools`) | CMake 集成 (可选) | 推荐 |
| **ARM Assembly** (`dan-c-underwood.arm`) | 汇编语法高亮 | 推荐 |
| **LinkerScript** (`zixuanwang.linkerscript`) | 链接脚本语法 | 推荐 |

> ⚠️ **冲突警告**：如果你之前装了 Microsoft C/C++ (`ms-vscode.cpptools`)，clangd 会和它冲突。
> 在 `settings.json` 中我们已经设置了 `"C_Cpp.intelliSenseEngine": "disabled"`，如果不放心，也可以直接在插件管理里**禁用** C/C++ 插件。

---

### 2.6 STM32CubeF4 库文件

这一步和之前 Keil5 教程相同：

```
1. 下载 https://www.st.com/en/embedded-software/stm32cubef4.html
2. 解压后把 Drivers/ 和 Middlewares/ 复制到 code/STM32/ 下

最终结构:
code/STM32/
├── Drivers/
│   ├── CMSIS/Core/Include/
│   ├── CMSIS/Device/ST/STM32F4xx/
│   └── STM32F4xx_HAL_Driver/
├── Middlewares/Third_Party/FreeRTOS/Source/
├── Inc/  (项目头文件)
├── Src/  (项目源文件)
├── CMakeLists.txt
├── .clangd
└── .vscode/
```

**链接脚本**：如果 `STM32F407VETx_FLASH.ld` 不在项目根目录，CMakeLists.txt 会自动回退到 CubeF4 包中的 `STM32F407VE_FLASH.ld`。

**SVD 文件**（外设寄存器描述）：从 CubeF4 包中复制：

```
FROM: Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
      旁边的 SVD 文件 STM32F407.svd
TO:   code/STM32/STM32F407.svd
```

---

## 三、首次配置（10分钟）

### 3.1 编译验证

```bash
# 1. CMake 配置 (只需运行一次)
cd f:/KeShe/code/STM32
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 如果 ninja 不可用, 用 MinGW Make
# cmake -B build -G "MinGW Makefiles"

# 2. 链接 compile_commands.json → clangd 需要它在项目根目录
# (这样 clangd 就知道编译选项和头文件路径)
ln -s build/compile_commands.json compile_commands.json
# Windows 上不能用 ln，直接复制:
cp build/compile_commands.json compile_commands.json

# 3. 编译
cmake --build build -j8
```

输出类似：

```
[100%] Linking C executable smart_carrier.elf
Generating .hex .bin + size report
   text    data     bss     dec     hex filename
   xxxxx    xxxx    xxxx    xxxx    xxxx smart_carrier.elf
```

### 3.2 烧录验证

```bash
# pyOCD (推荐)
pyocd load build/smart_carrier.hex --target stm32f407ve

# 或 OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/smart_carrier.hex verify reset exit"
```

### 3.3 调试验证

1. 接好 ST-Link → MCU 上电
2. 按 `F5`
3. 程序应停在 `main()` 第一行

---

## 四、日常操作

### 4.1 编译 / 烧录 / 调试 / 清理

**快捷方式（推荐）**：

| 操作 | 触发 | 底层命令 |
|------|------|---------|
| 编译 | `Ctrl+Shift+B` | `cmake --build build -j8` |
| 调试（自动编译+烧录+启动） | `F5` | Cortex-Debug → pyOCD |
| 仅烧录 | `Ctrl+Shift+P` → `Tasks: Run Task` → `Flash (pyOCD)` | `pyocd load ...` |
| 清理 | `Ctrl+Shift+P` → `Tasks: Run Task` → `Clean` | `cmake --build build --target clean` |
| 全清理 | `Ctrl+Shift+P` → `Tasks: Run Task` → `Clean All` | `rm -rf build/` |
| 检查工具链 | `Ctrl+Shift+P` → `Tasks: Run Task` → `Check Toolchain` | 依次打印各工具版本 |

**纯命令行（不需要 VSCode）**：

```bash
cd f:/KeShe/code/STM32

# 配置 (首次)
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 编译
cmake --build build -j8

# 烧录
pyocd load build/smart_carrier.hex --target stm32f407ve

# 清理
cmake --build build --target clean
```

### 4.2 调试界面与快捷键

```
┌──────────────────────┬──────────────────────────────────┐
│  VARIABLES (变量)     │  编辑区 (断点、单步执行)          │
│  ├─ Local             │                                  │
│  ├─ Global            │  int main(void) {                │
│  │  g_motors[0]       │      HAL_Init();                │
│  │  g_sys.state    ◄──│──●   SystemClock_Config();  ← 断点
│  └─ Static            │      ...                         │
│                       │                                  │
├──────────────────────┤                                  │
│  WATCH (监视)         │                                  │
│  g_sys.current_step   │                                  │
│  g_lane.offset_mm     │                                  │
├──────────────────────┤                                  │
│  CALL STACK (调用栈)  │                                  │
│  Task_Manager        │                                  │
│  Task_Chassis        │                                  │
├──────────────────────┤                                  │
│  CORTEX PERIPHERALS   │                                  │
│  (外设寄存器视图)      │                                  │
│  ├─ GPIOA             │                                  │
│  ├─ TIM2              │                                  │
│  ├─ USART3            │                                  │
│  └─ ...               │                                  │
├──────────────────────┤                                  │
│  RTOS: FreeRTOS       │                                  │
│  (任务列表+状态)       │                                  │
└──────────────────────┴──────────────────────────────────┘
```

| 快捷键 | 功能 |
|:---:|------|
| `F5` | 启动调试 / 继续运行 |
| `F9` | 设置/取消断点 |
| `F10` | 单步跳过 (Step Over) |
| `F11` | 单步进入 (Step Into) |
| `Shift+F11` | 单步跳出 (Step Out) |
| `Ctrl+Shift+F5` | 重启调试 |
| `Shift+F5` | 停止调试 |
| `Ctrl+F5` | 运行到光标处 |

### 4.3 Watch / 断点 / 外设寄存器

**Watch 常用表达式**：

```
g_sys.state                     # 状态机
g_sys.current_step              # 搬运步骤
g_sys.task_code.digits[0]       # 任务编码
g_motors[0].current_rpm         # 电机转速
g_chassis.pose.x_mm             # 里程计 X
g_lane.on_lane                  # 车道是否在线
g_lane.offset_mm                # 车道偏移
g_sys.has_error                 # 出错标志
```

**条件断点**：右键断点 → Edit Breakpoint → 输入条件表达式，如 `g_sys.state == 10`

**外设寄存器**：Cortex-Debug 自动加载 SVD 文件后，CORTEX PERIPHERALS 面板列出全部外设。

### 4.4 FreeRTOS 线程调试

调试控制台 `Ctrl+Shift+Y` 中输入：

```
-exec info threads
```

显示所有 8 个 FreeRTOS 任务：

```
* 1  Thread ... (Task_Manager : Running)
  2  Thread ... (Task_Chassis : Ready)
  3  Thread ... (Task_Arm : Blocked)
  4  Thread ... (Task_Comm : Blocked)
```

切换任务：`-exec thread 2`

---

## 五、进阶：AI 辅助开发 & 串口诊断

> 这是 VSCode 工具链的终极形态——AI 读串口日志，自动分析问题。

### 5.1 总体架构 — MCP 打通 AI ↔ 硬件

**MCP (Model Context Protocol)** 是 AI 和外设之间的标准协议。配置好 MCP 后，AI 不再需要你手动采集日志——它自己就能读串口、编译、烧录。

```
┌──────────────────────────────────────────────────────────────┐
│              Claude Code (项目根目录启动)                      │
│                                                              │
│  ┌────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │ 代码上下文  │    │ serial MCP   │    │ pyocd MCP    │     │
│  │ (CLAUDE.md │    │              │    │              │     │
│  │  全部源码)  │    │ serial_read  │    │ build        │     │
│  │            │    │ serial_moni- │    │ flash        │     │
│  │            │    │ tor_start    │    │ build & flash│     │
│  │            │    │ serial_send  │    │ check tool-  │     │
│  │            │    │ list_ports   │    │ chain        │     │
│  └─────┬──────┘    └──────┬───────┘    └──────┬───────┘     │
│        │                 │                   │               │
└────────┼─────────────────┼───────────────────┼───────────────┘
         │                 │ (MCP stdio)       │
         ▼                 ▼                   ▼
  项目源码 + 文档     STM32 USART3          ST-Link
                     printf 日志            烧录器
                     USB-UART 线            USB 线
```

**和之前方案的区别**：

| | 之前（无 MCP） | 现在（有 MCP） |
|:---|:---|:---|
| 串口 | 你手动跑 Python 脚本采集 serial.log → AI 读文件 | AI 直接调 `serial_monitor_start` |
| 烧录 | 你手动跑 `pyocd load` | AI 直接调 `flash` |
| 闭环 | 你说一次 → AI 改代码 → 你手动编译烧录跑 → 你看日志贴给 AI → 循环 | AI 改代码 → AI 编译 → AI 烧录 → AI 看串口 → 再改 → 全自动 |

### 5.2 安装 Claude Code

```bash
npm install -g @anthropic-ai/claude-code
claude --version
```

### 5.3 安装 MCP 依赖

```bash
pip install pyserial pyocd mcp
```

> `pyserial` = 串口通信，`pyocd` = ST-Link 烧录，`mcp` = MCP 协议 Python SDK

### 5.4 MCP 配置（项目已配好）

项目中已包含 `.mcp.json` 和两个 MCP Server：

```
f:\KeShe\
├── .mcp.json                        ← Claude Code 自动加载
└── .claude/
    └── mcp-servers/
        ├── serial_mcp.py            ← 串口 MCP (读 printf, 发调试命令, 监控)
        └── pyocd_mcp.py             ← 烧录 MCP (编译, 烧录, 检查工具链)
```

**检查 MCP 是否加载**：在 Claude Code 里输入 `/mcp`，应列出 `serial` 和 `pyocd` 两个 server。

### 5.5 STM32 端：加 printf

在 `main.c` 尾部添加（GCC 版 printf 重定向到 USART3）：

```c
#ifdef __GNUC__
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart3, (uint8_t *)ptr, len, 100);
    return len;
}
#endif
```

在 `task_manager.c` 关键位置加 LOG 宏：

```c
#define LOG(fmt, ...) printf("[%lu] " fmt "\r\n", osKernelGetTickCount(), ##__VA_ARGS__)

// 状态转换
LOG("STATE: %s → %s", STATE_NAMES[g_sys.prev_state], STATE_NAMES[g_sys.state]);

// PID 异常
if (fabsf(m->current_rpm - m->target_rpm) > 50.0f)
    LOG("WARN: M%d RPM err target=%.0f actual=%.0f", m->id, m->target_rpm, m->current_rpm);
```

### 5.6 实际使用示例

**启动 Claude Code**：

```bash
cd f:/KeShe
claude
```

**对话示例 1 — 诊断运行时问题**：

```
你: 接上 ST-Link 和串口线了，帮我编译烧录，然后监控 15 秒串口看有没有报错

AI:
  → 调用 pyocd_mcp: build_and_flash
  → "✅ 编译成功, ✅ 烧录成功"
  → 调用 serial_mcp: serial_list_ports
  → "检测到 COM3 — STMicroelectronics STLink Virtual COM Port"
  → 调用 serial_mcp: serial_connect(port="COM3", baudrate=115200)
  → "已连接 COM3"
  → "现在请启动机器人..."
  → 调用 serial_mcp: serial_monitor_start(duration_s=15)
  → 返回 15 秒内的所有 printf 输出
  → "日志显示 STATE: SCAN_QR → PROCESS_QR 正常,
     但在 FIND_MATERIAL 时 RSP:ERR,timeout
     → 建议检查 MaixCAM 是否在位、config.py 颜色阈值是否正确"
```

**对话示例 2 — 自动调参**：

```
你: 看串口日志发现 offset 偏大 (平均 60mm), 帮我把 Chassis_HybridLaneFollow
    里的 kp 从 2.5 调到 5.0，编译烧录，我再跑一圈

AI:
  → 修改 chassis.c kp=5.0
  → build_and_flash
  → "✅ 已编译烧录, kp 已改为 5.0, 请启动机器人"
```

**对话示例 3 — 模拟 MaixCAM 命令测 STM32**：

```
你: MaixCAM 还没接, 帮我模拟发 CMD:SCAN_QR 给 STM32 看它能不能正确响应

AI:
  → serial_connect("COM3")
  → serial_send("CMD:SCAN_QR")
  → serial_read()
  → "STM32 回复了 CMD:HEARTBEAT, 说明 UART3 通信正常
     (QR 扫描请求需要 MaixCAM 实际处理, 这个回复说明 STM32 在主动发心跳)"
```

### 5.7 MCP 工具速查表

**serial MCP** (`serial_mcp.py`)：

| 工具 | 功能 | 典型用法 |
|------|------|---------|
| `serial_list_ports` | 列出可用串口 | "帮我看接了几个串口" |
| `serial_connect` | 连接指定串口 | `serial_connect("COM3", 115200)` |
| `serial_read` | 读缓冲区已有数据 | 跑完一圈后看累积日志 |
| `serial_monitor_start` | 采集 N 秒实时日志 | `serial_monitor_start(15)` |
| `serial_send` | 向 STM32 发数据 | 模拟 MaixCAM 命令调试 |
| `serial_disconnect` | 断开 | 切换串口前 |

**pyocd MCP** (`pyocd_mcp.py`)：

| 工具 | 功能 |
|------|------|
| `check_toolchain` | 检查 ARM GCC / CMake / Ninja / pyOCD 是否就绪 |
| `cmake_configure` | CMake 初始化 (首次/改 CMakeLists.txt 后) |
| `build` | 编译项目 |
| `flash` | 烧录 hex 到 STM32F407VE |
| `build_and_flash` | 编译 + 烧录 一键 |

### 5.8 AI 自动 PID 调参

> **前提**：STM32 已烧录含 printf 重定向 + 调试命令解析器的固件（当前项目已包含）

**整体流程**：

```
AI 发 TST:0,100,300     → STM32 跑阶跃测试 (电机A, 100RPM, 300ms)
AI 读 serial_monitor    → 收集 CSV 数据
AI 分析阶跃响应曲线      → 计算超调量/上升时间/稳态误差
AI 发 PID:0.08,0.02,0.01 → 写入新参数
Motor_ApplyPIDGains()   → 立即生效 (无需重启)
AI 再发 TST:0,100,300   → 验证效果
...重复 3~5 轮直到收敛
```

**串口调试命令**（通过 `serial_send` 发送）：

| 命令 | 作用 | 示例 |
|------|------|------|
| `PID:?` | 查询当前 PID | `serial_send("PID:?")` |
| `PID:0.08,0.02,0.01` | 设置 Kp/Ki/Kd，立即生效 | `serial_send("PID:0.10,0.025,0.012")` |
| `TST:0,100,300` | 电机A，目标100RPM，测试300ms | `serial_send("TST:1,80,250")` |

**TST 输出格式**（每一行 = 1ms PID 周期）：

```
STEP,0,START,target=100.0,duration=300
STEP,0,299,100.0,0.0,100.0,0.900
STEP,0,298,100.0,18.0,82.0,0.880
...
STEP,0,1,100.0,98.5,1.5,0.150
STEP,0,END,0,0,0,0
^^^^^^^^^  ^^^  ^^^^  ^^^^  ^^^  ^^^
命令     电机 tick 目标  实际  误差 duty
```

**对话示例**：

```
你: 电机A 转速不稳，帮我调一下 PID

AI:
  → serial_send("PID:?")
  → 当前: kp=0.06 ki=0.015 kd=0.008
  → serial_send("TST:0,100,300")
  → serial_monitor_start(5)
  → 收集 300 行 CSV 数据
  → 分析:
     上升时间 45ms, 超调 28%, 稳态误差 ±3RPM, 轻微振荡
     → 超调偏大 → 增大 Kd 抑制超调
     → 新参数: kp=0.07 ki=0.018 kd=0.012
  → serial_send("PID:0.07,0.018,0.012")
  → serial_send("TST:0,100,300")
  → 再测:
     上升时间 52ms, 超调 5%, 稳态误差 ±1RPM, 无振荡 ✅
  → "已收敛。最佳参数: kp=0.07 ki=0.018 kd=0.012
     如需写入固件, 改 motor.c 顶部 g_pid_kp/ki/kd 初始值"
```

---

## 六、常见问题排查

### Q1: `arm-none-eabi-gcc: command not found`

→ ARM GCC 没有加入 PATH。关闭 VSCode 重新打开，或手动添加：

```powershell
# 用 PowerShell 管理员:
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin", "User")
```

### Q2: clangd 没有补全/跳转

→ 检查 `compile_commands.json` 是否在项目根目录：

```bash
ls compile_commands.json
# 如果不存在: cp build/compile_commands.json .
```

→ 检查 clangd 插件是否安装并启用  
→ VSCode 底部状态栏应显示 "clangd" 图标

### Q3: CMake 报错 "Could not find Ninja"

→ 安装 Ninja 或改用 MinGW Makefiles：

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### Q4: pyOCD 报错 "No ST-Link detected"

```bash
# 检查驱动
pyocd list
# 应列出 ST-Link 和 STM32F407VE

# 如果没找到:
# 1. 检查 USB 线
# 2. 用 Zadig 换驱动 (https://zadig.akeo.ie/)
#    → 选 ST-Link → WinUSB
```

### Q5: 调试启动时停在 `Reset_Handler` 不进 `main`

→ 检查 `launch.json` 中 `"runToEntryPoint": "main"`  
→ 检查链接脚本是否正确  
→ 检查启动文件 `startup_stm32f407xx.s` 是否在 CMakeLists.txt 中

### Q6: 断点不生效

→ 确认 CMakeLists.txt 中用了 `-O0 -g3` (无优化+全量调试信息)  
→ 不要在 `while(1)` 空循环内部设断点 (可能被优化掉)

---

## 七、与 Keil5 对比速查

| 组件 | Keil5 | VSCode (本指南) |
|------|-------|----------------|
| 编译器 | ARMCC v5/v6 | arm-none-eabi-gcc 13.x |
| 构建系统 | .uvprojx 工程文件 | CMakeLists.txt + Ninja |
| 语法高亮/补全 | 内置 | clangd (更快, 更准) |
| 烧录 | Flash → Download | pyOCD / OpenOCD |
| 调试 | 内置调试器 | Cortex-Debug + pyOCD GDB Server |
| 外设视图 | Peripherals → System Viewer | SVD + CORTEX PERIPHERALS 面板 |
| FreeRTOS 视图 | RTX Viewer | `-exec info threads` + RTOS 面板 |
| 命令行编译 | Keil 命令行工具 | `cmake --build build -j8` |
| CI/CD | 困难 | 一行 cmake 命令 |
| AI 辅助 | 无 | Claude Code 整合全链路 |
| 串口日志 AI 诊断 | 无 | printf → 串口 → AI 自动分析 |

---

> 🔗 **相关链接**：  
> - ARM GNU Toolchain：https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads  
> - CMake：https://cmake.org/download/  
> - Ninja：https://github.com/ninja-build/ninja/releases  
> - clangd：https://github.com/clangd/clangd/releases  
> - pyOCD：https://pyocd.io/  
> - Cortex-Debug：VSCode 扩展市场搜索  
> - Claude Code：`npm install -g @anthropic-ai/claude-code`  
> - 项目 GitHub：https://github.com/Jacoleyyyy/Keshe  
> - 零基础教程：[教程-从零开始.md](../../教程-从零开始.md)  
> - 项目架构：[CLAUDE.md](../../CLAUDE.md)
