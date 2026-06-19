# STM32F407 VSCode 调试完整指南

> 从 Keil5 迁移到 VSCode + ARM GCC + OpenOCD + Cortex-Debug

---

## 一、环境安装（一次性操作）

### 1.1 安装 ARM GCC 工具链

下载地址：https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

选择 **Windows (mingw-w64-i686) AArch32 bare-metal target (arm-none-eabi)** 版本。

安装时**必须勾选** "Add to PATH"，或安装后手动将 `bin/` 加入系统 PATH。

```powershell
# 验证安装
arm-none-eabi-gcc --version
# 应输出: arm-none-eabi-gcc (Arm GNU Toolchain ...) 13.x.x
```

### 1.2 安装 OpenOCD

下载地址：https://github.com/openocd-org/openocd/releases

或使用 Scoop/Chocolatey：

```powershell
# 方式一: scoop (推荐)
scoop install openocd

# 方式二: 直接下载 zip，解压到 C:\openocd，加入 PATH
```

验证：

```powershell
openocd --version
```

### 1.3 安装 Make (Windows)

```powershell
# 使用 scoop
scoop install make

# 或者装 MSYS2 / Git for Windows 自带的 make
```

### 1.4 安装 VSCode 插件

在 VSCode 中按 `Ctrl+Shift+X`，搜索安装：

| 插件 | 用途 | 必装 |
|------|------|:---:|
| **Cortex-Debug** | STM32 调试核心 (支持 OpenOCD/J-Link) | ✅ |
| **C/C++** (Microsoft) | C语言语法高亮、智能提示、问题匹配 | ✅ |
| **ARM Assembly** | 汇编语法高亮 (看启动文件) | 推荐 |
| **LinkerScript** | 链接脚本语法高亮 (.ld 文件) | 推荐 |

---

## 二、获取 HAL 库和 FreeRTOS（从 STM32CubeF4）

你之前用 Keil5 肯定已经有 STM32CubeF4 的库了。在 VSCode 工程目录下建软链接或直接复制：

```
code/STM32/
├── Drivers/
│   ├── CMSIS/
│   │   ├── Core/
│   │   │   └── Include/          ← core_cm4.h 等
│   │   └── Device/ST/STM32F4xx/
│   │       ├── Include/           ← stm32f407xx.h, system_stm32f4xx.h
│   │       └── Source/Templates/
│   │           └── system_stm32f4xx.c
│   └── STM32F4xx_HAL_Driver/
│       ├── Inc/
│       └── Src/
├── Middlewares/
│   └── Third_Party/
│       └── FreeRTOS/
│           └── Source/            ← FreeRTOS 内核源文件
├── startup_stm32f407xx.s          ← 启动文件 (从 Keil 工程复制)
├── STM32F407VGTx_FLASH.ld        ← 链接脚本
├── STM32F407.svd                  ← 外设寄存器描述文件
└── Makefile                       ← 已生成
```

### 获取 SVD 文件

SVD 文件让 Cortex-Debug 能显示外设寄存器（类似 Keil 的 System Viewer）：

- 从 STM32CubeF4 包中找：`Drivers/CMSIS/SVD/STM32F407.svd`
- 或从 Keil 安装目录找：`Keil_v5/ARM/Pack/Keil/STM32F4xx_DFP/.../CMSIS/SVD/`
- 或直接下载：https://github.com/posborne/cmsis-svd

---

## 三、调试操作方法

### 3.1 编译项目

```
Ctrl+Shift+B  →  选择 "Build STM32"
```

或在终端直接跑：

```powershell
cd f:\KeShe\code\STM32
make -j8
```

### 3.2 启动调试（最常用）

1. 接好 ST-Link (SWD 四线: VCC/GND/SWDIO/SWCLK)
2. MCU 上电
3. 按 **`F5`** 启动调试

VSCode 会自动：
1. **编译** → `Build STM32` 任务
2. **启动 OpenOCD** → 连接 ST-Link
3. **下载固件** → 烧录到 Flash
4. **复位 MCU** → 停在 `main()` 入口
5. 打开调试面板

### 3.3 调试界面说明

```
┌──────────────────────┬──────────────────────────────────┐
│  VARIABLES (变量)     │  编辑区 (断点、单步执行)          │
│  ├─ Local             │                                  │
│  ├─ Global            │  int main(void) {                │
│  │  g_motors[0]       │      HAL_Init();                │
│  │  g_sys.state    ◄──│──●   SystemClock_Config();  ← 断点
│  ├─ Static            │      MX_GPIO_Init();             │
│  └─ Registers         │      ...                         │
│     R0-R15, SP, PC    │                                  │
├──────────────────────┤                                  │
│  WATCH (监视)         │                                  │
│  g_sys.current_step   │                                  │
│  g_sys.target_color   │                                  │
├──────────────────────┤                                  │
│  CALL STACK (调用栈)  │                                  │
│  Task_Manager        │                                  │
│  Task_Chassis        │                                  │
│  ...                 │                                  │
├──────────────────────┤                                  │
│  CORTEX PERIPHERALS   │                                  │
│  (外设寄存器视图)      │                                  │
│  ├─ GPIOA             │                                  │
│  ├─ TIM2              │                                  │
│  ├─ USART3            │                                  │
│  └─ ...               │                                  │
└──────────────────────┴──────────────────────────────────┘
```

### 3.4 常用快捷键

| 快捷键 | 操作 | Keil5 对应 |
|--------|------|-----------|
| `F5` | 启动调试 / 继续运行 | `F5` (同 Keil) |
| `F11` | 单步进入 (Step Into) | `F11` |
| `F10` | 单步跳过 (Step Over) | `F10` |
| `Shift+F11` | 单步跳出 (Step Out) | `Ctrl+F11` |
| `F9` | 设置/取消断点 | `F9` |
| `Ctrl+Shift+F5` | 重启调试 | `Ctrl+Shift+F5` |

### 3.5 用 Watch 监视变量

在 `WATCH` 面板点 `+`，输入：

```
g_sys.state                 → 当前状态机状态
g_sys.current_step          → 当前步骤
g_sys.task_code.digits      → 任务编码 (3个数字)
g_motors[0].current_rpm     → 左前电机转速
g_motors[1].current_rpm     → 右前电机转速
g_chassis.pose.x_mm         → 当前X坐标
g_chassis.pose.y_mm         → 当前Y坐标
g_chassis.pose.yaw_deg      → 当前偏航角
g_sys.has_error             → 是否有错误
```

---

## 四、FreeRTOS 线程调试

Cortex-Debug 对 FreeRTOS 有原生支持：

### 4.1 查看所有任务

在调试控制台输入：

```
-exec info threads
```

会列出所有 8 个 FreeRTOS 任务及其状态：

```
* 1  Thread 536871936 (Task_Manager : Running)
  2  Thread 536872448 (Task_Chassis : Ready)
  3  Thread 536872960 (Task_Arm : Blocked)
  4  Thread 536873472 (Task_Comm : Blocked)
  ...
```

### 4.2 切换到特定任务的调用栈

```
-exec thread 2
```

### 4.3 查看任务栈用水位（检测栈溢出）

在 Watch 里监视：

```
uxTaskGetStackHighWaterMark(g_task_handle_manager)
```

---

## 五、调试常见场景

### 5.1 调试二维码读取流程

```c
// 在 task_manager.c 的 STATE_SCAN_QR 中设断点
case STATE_SCAN_QR:
{
    CommError_t err = Communication_ScanQR(&g_sys.task_code, 5000);
    // ← 在此设断点, 用 Watch 看 g_sys.task_code.digits
}
```

### 5.2 调试麦轮运动学

```c
// 在 chassis.c 的 Chassis_InverseKinematics 中设断点
void Chassis_InverseKinematics(float vx, float vy, float wz, float rpm[4])
{
    // ← 在此设断点, Watch rpm[0..3] 查看4轮转速分配
}
```

### 5.3 调试 PID 调速

```c
// 在 motor.c 的 Motor_PIDUpdate 中设断点
m->current_rpm = (float)enc_diff / (float)PULSE_PER_REV * 60000.0f;
// ← Watch: m->target_rpm, m->current_rpm, m->pid.error, m->pid.output
```

### 5.4 调试 UART 通信

在 `communication.c` 的 `Communication_ParseResponse` 设断点，查看 `type_str` 值和 `data` 缓冲区内容。

---

## 六、烧录（不调试）

如果只想烧录不想调试：

```
Ctrl+Shift+P → Run Task → Flash (ST-Link)
```

或者终端：

```powershell
make flash
```

---

## 七、常见问题

### Q1: "arm-none-eabi-gcc: command not found"

→ ARM GCC 没有加入 PATH。检查安装路径，或手动设置 `cortex-debug.armToolchainPath`。

### Q2: 编译报错 "stm32f4xx_hal.h: No such file"

→ HAL 库路径不对。检查 `Makefile` 中的 `ST_HAL_DIR` 和 `CMSIS_CORE` 路径，确保指到实际存在的目录。

### Q3: 调试启动时 "Error: open failed"

→ 检查 ST-Link 驱动：
```powershell
# 用 Zadig 换驱动 (WinUSB 或 libusb-win32)
# 下载: https://zadig.akeo.ie/
```

### Q4: 断点不生效

→ 确认编译时用了 `-O0` (不要优化)，且链接脚本正确。

### Q5: FreeRTOS 任务视图是空的

→ 确认 `launch.json` 里有 `"rtos": "FreeRTOS"`。某些 OpenOCD 版本可能需要添加：

```tcl
# 在 openocd 启动参数中加入
-c "stm32f4x.cpu configure -rtos FreeRTOS"
```

### Q6: 我能同时用 Keil5 和 VSCode 调试吗？

可以。两种工具可以共存：
- **Keil5**：保持原有 `.uvprojx` 工程，用于你熟悉的调试场景
- **VSCode**：新工作流，按 `F5` 启动 ARM GCC 编译 + OpenOCD 调试

代码是同一份（`Inc/` 和 `Src/`），只是编译工具链不同。ARMCC(Keil) 和 arm-none-eabi-gcc 编译选项略有差异，但标准C代码兼容。

---

## 八、与 Keil5 操作对比速查

| 操作 | Keil5 | VSCode |
|------|-------|--------|
| 编译 | `F7` / Project→Build | `Ctrl+Shift+B` |
| 启动调试 | `Ctrl+F5` 或菜单 | `F5` |
| 单步执行 | `F10` / `F11` | `F10` / `F11` |
| 设置断点 | 点行号左边 / `F9` | `F9` |
| 运行到光标 | `Ctrl+F10` | 右键→Run to Cursor |
| 查看外设寄存器 | Peripherals→System Viewer | CORTEX PERIPHERALS 面板 |
| 查看变量 | Watch 窗口 | WATCH 面板 |
| 查看内存 | Memory 窗口 | 调试控制台 `-exec x/16x 0x20000000` |
| 串口输出 | Serial Window | 另开终端 `putty` / 内置终端 |

---

> **提示**：建议先用 Keil5 确认硬件和基础功能正常，再用 VSCode 进行日常开发和调试。首次切换到 VSCode 时可能需要解决库路径问题，耐心调整 `Makefile` 里的路径即可。
