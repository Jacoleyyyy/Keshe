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

**链接脚本**：如果 `STM32F407VGTx_FLASH.ld` 不在项目根目录，CMakeLists.txt 会自动回退到 CubeF4 包中的 `STM32F407VG_FLASH.ld`。

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
pyocd load build/smart_carrier.hex --target stm32f407vg

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
pyocd load build/smart_carrier.hex --target stm32f407vg

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

### 5.1 总体架构

```
┌───────────────────────────────────────────────────────────┐
│                    AI Agent (Claude Code / OpenCode)       │
│                                                           │
│  ┌─────────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │  代码上下文      │  │  串口日志流   │  │  调试器接口   │ │
│  │  (项目全部源码)   │  │  (printf/LOG) │  │  (GDB MI)    │ │
│  └────────┬────────┘  └──────┬───────┘  └──────┬───────┘ │
│           │                  │                  │          │
│           ▼                  ▼                  ▼          │
│  AI 读取全部代码    AI 看 printf 输出    AI 设断点/读变量    │
│  理解上下文         自动发现异常         自动排查           │
│                                                           │
│  输出: "电机C转速异常, 建议检查编码器接线或加大KP到0.08"    │
└───────────────────────────────────────────────────────────┘
         │                    │
         ▼                    ▼
    STM32F407           USB-UART 串口
```

### 5.2 安装 Claude Code / OpenCode

**Claude Code**（Anthropic 官方 CLI AI 编程工具）：

```bash
npm install -g @anthropic-ai/claude-code
claude --version
```

在项目根目录启动：

```bash
cd f:/KeShe
claude
```

AI 将自动读取 CLAUDE.md、所有源码、.gitignore 等 → 直接对话式编程。

**OpenCode**（开源替代，支持本地模型）：

```bash
# https://github.com/opencode-ai/opencode
# 支持 Ollama / LM Studio 等本地大模型
pip install opencode
```

### 5.3 串口接入 AI

**核心思路**：让 printf 日志同时输出到串口和文件，AI 读取文件来诊断。

**第一步：在 STM32 上加 printf 重定向**

在 `main.c` 尾部添加（或用独立 `debug_console.c`）：

```c
/* __GNUC__ 版本的 printf 重定向 → USART3 */
#ifdef __GNUC__
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)ptr, len, 100);
    return len;
}
#endif

/* 在 main() 初始化末尾加 */
printf("\r\n[BOOT] Smart Carrier v1.0\r\n");
printf("[BOOT] System Clock: %lu MHz\r\n", SystemCoreClock / 1000000);
```

**第二步：在状态机关键节点加 LOG**

在 `task_manager.c` 中：

```c
#define LOG(fmt, ...)  printf("[%lu] " fmt "\r\n", osKernelGetTickCount(), ##__VA_ARGS__)

// 状态转换时
LOG("STATE: %s → %s", STATE_NAMES[g_sys.prev_state], STATE_NAMES[g_sys.state]);

// 电机 PID 异常
if (fabsf(m->current_rpm - m->target_rpm) > 50.0f)
    LOG("WARN: Motor%d RPM偏差过大 target=%.0f actual=%.0f", m->id, m->target_rpm, m->current_rpm);

// 视觉响应超时
if (err != COMM_OK) LOG("ERR: MaixCAM timeout cmd=%d", cmd);
```

**第三步：串口转文件**

用 Python 脚本或 `screen`/`minicom` 的日志功能：

```bash
# 方式一: Python 脚本 (推荐, 跨平台)
python -c "
import serial, sys
ser = serial.Serial('COM3', 115200)
with open('serial.log', 'a') as f:
    while True:
        line = ser.readline().decode(errors='ignore')
        sys.stdout.write(line)
        f.write(line)
" > serial.log

# 方式二: PowerShell (Windows)
# 用 Putty 的 "All session output" 日志功能
```

### 5.4 AI 自动日志诊断

**场景一：运行时出错，AI 分析日志**

```
你: 机器人跑完第二步就卡住了，日志在 serial.log

AI 读取 serial.log 后:
"从日志看:
  [1240] STATE: MOVE_RAW → FIND_MAT
  [3200] ERR: MaixCAM timeout cmd=3     ← 关键!
  [3201] STATE: FIND_MAT → ERROR

  问题: FIND_MATERIAL 命令超时。可能原因:
  1. MaixCAM 的串口没接好 (检查 PB10/PB11 接线)
  2. MaixCAM 没上电或 main.py 没运行
  3. config.py 中颜色阈值不对，找不到物料
  建议: 先在 MaixCAM 屏幕上确认 color_detector 能看到物料"
```

**场景二：让 AI 自动编码+烧录+看日志**

```
你: 帮我在每次电机启动时打印 RPM，编译烧录，然后跑一遍看日志

AI:
1. 修改 motor.c 的 Motor_SetTargetRPM → 添加 printf
2. cmake --build build -j8
3. pyocd load build/smart_carrier.hex --target stm32f407vg
4. 启动串口日志采集脚本
5. "请你现在启动机器人，我会看串口输出..."
6. [等机器人跑完]
7. 读取 serial.log → 分析 → 输出结论
```

### 5.5 AI 闭环调试工作流

```
┌─────────────────────────────────────────────────────┐
│               AI 闭环调试工作流                       │
│                                                     │
│  1. 你说: "机器人向左边偏, 帮我调车道修正"            │
│                                                     │
│  2. AI 分析 chassis.c 的 Chassis_HybridLaneFollow()  │
│     → 发现 kp=2.5 可能太小                          │
│                                                     │
│  3. AI 修改 kp=4.0 → 编译 → 烧录                     │
│                                                     │
│  4. 你启动机器人 → AI 看串口日志                     │
│     printf: "Lane offset=35mm"                      │
│     (之前是 60mm偏移 → kp=2.5修正不够)               │
│                                                     │
│  5. AI 继续调大 kp=5.0 → 编译 → 烧录                 │
│     printf: "Lane offset=12mm" ← 好了!              │
│                                                     │
│  6. "已修正, kp 建议值 5.0, 偏移从 60mm 降到 12mm"   │
└─────────────────────────────────────────────────────┘
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
# 应列出 ST-Link 和 STM32F407VG

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
