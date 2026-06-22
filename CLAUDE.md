# 智能搬运机器人 — 项目文档

> **课程**：《机电智能控制综合设计》  
> **GitHub**：https://github.com/Jacoleyyyy/Keshe  
> **硬件平台**：轮趣科技麦轮小车 + 矽速科技 MaixCAM Pro  
> **MCU**：STM32F407 (Cortex-M4, 168MHz)  
> **RTOS**：FreeRTOS (CMSIS OS v2)  
> **硬件版本**：WHEELTEC C30D 2.0 GMR编码器版 (75mm轮/60:1/500PPR)

---

## 一、项目结构

```
f:\KeShe\
├── rr.md                          ← 课程设计需求文档 (赛题/规则/硬件链接)
├── CLAUDE.md                      ← 本文档 (项目说明、架构、使用指南)
├── README.md                      ← GitHub README
│
└── code/
    ├── STM32/                     ← 主控制器 — 底盘、机械臂、传感器、调度
    │   ├── Inc/                   ← 12个头文件
    │   │   ├── FreeRTOSConfig.h   #   FreeRTOS 配置 (任务优先级、栈、队列)
    │   │   ├── pin_config.h       #   引脚定义 (电机/舵机/传感器/UART/I2C)
    │   │   ├── protocol.h         #   通信协议 + 状态枚举 + 场地坐标 + 运动参数
    │   │   ├── main.h             #   全局句柄声明
    │   │   ├── pid.h              #   PID 控制器接口 (增量式/位置式)
    │   │   ├── motor.h            #   直流电机接口 (编码器+PID调速)
    │   │   ├── servo.h            #   舵机接口 (机械臂4自由度+预设姿态)
    │   │   ├── sensor.h           #   传感器接口 (灰度巡线/超声波/按钮)
    │   │   ├── display.h          #   OLED 显示接口 (SSD1306 I2C)
    │   │   ├── chassis.h          #   麦轮运动学 + 里程计 + 导航
    │   │   ├── communication.h    #   UART 通信接口 (CMD:/RSP: 协议)
    │   │   └── task_manager.h     #   系统上下文 + 8个FreeRTOS任务声明
    │   │
    │   ├── Src/                   ← 9个源文件, 5668行
    │   │   ├── pid.c              #   PID 核心算法实现
    │   │   ├── motor.c            #   4电机驱动 (PWM+编码器+1kHz PID)
    │   │   ├── servo.c            #   4舵机控制 (角度映射/平滑运动/姿态表)
    │   │   ├── sensor.c           #   灰度巡线 / 超声波测距 / 按钮
    │   │   ├── display.c          #   OLED SSD1306 驱动 + 6行状态界面
    │   │   ├── chassis.c          #   ★ 麦轮逆运动学 + 里程计 + 点对点导航
    │   │   ├── communication.c    #   UART 帧解析 + QR/颜色/物料/区域请求
    │   │   ├── task_manager.c     #   ★★ 19状态机 + 8个FreeRTOS任务 (923行)
    │   │   └── main.c             #   系统入口 (时钟/外设/任务创建)
    │   │
    │   ├── .vscode/               ← VSCode 调试配置
    │   │   ├── launch.json        #   3种调试配置 (OpenOCD / J-Link / Attach)
    │   │   ├── tasks.json         #   编译/清理/烧录 任务
    │   │   ├── c_cpp_properties.json  # IntelliSense 智能提示
    │   │   └── settings.json      #   编辑器 + Cortex-Debug 设置
    │   ├── Makefile               #   ARM GCC 编译脚本
    │   └── VSCode调试指南.md       #   VSCode 调试完整教程
    │
    └── MaixCAM/                   ← 视觉子系统 — QR检测、颜色识别、车道检测
        ├── config.py              #   相机/UART/颜色/车道阈值参数
        ├── qr_detector.py         #   QR码检测 (多帧稳定+编码校验)
        ├── color_detector.py      #   LAB色彩分割 / 物料定位 / 色环检测
        ├── lane_detector.py       #   灰色车道检测 (备选, 防出界)
        ├── comm.py                #   UART 协议通信
        └── main.py                #   主循环 (命令分发+多帧检测)
```

## 二、系统架构

```
                            ┌──────────────────────────┐
                            │     MaixCAM Pro           │
                            │     MaixPy (MicroPython)  │
                            │                           │
                            │  ┌─────────────────────┐  │
                            │  │ main.py             │  │
                            │  │  命令分发主循环       │──│──┐
                            │  ├─────────────────────┤  │  │
                            │  │ qr_detector.py      │  │  │
                            │  │  QR码检测 (多帧稳定)  │  │  │
                            │  ├─────────────────────┤  │  │
                            │  │ color_detector.py   │  │  │
                            │  │  颜色分割+物料定位    │  │  │
                            │  │  色环检测+像素→世界   │  │  │
                            │  ├─────────────────────┤  │  │
                            │  │ comm.py             │  │  │
                            │  │  UART帧协议解析      │  │  │
                            │  └─────────────────────┘  │  │
                            └────────────┬─────────────┘  │
                                         │               │
                                 UART (115200)           │
                              CMD:xxx\r\n                │
                              RSP:type,data...\r\n       │
                                         │               │
                            ┌────────────▼─────────────┐  │
                            │     STM32F407             │  │
                            │     FreeRTOS              │  │
                            │                           │  │
                            │  8个并发任务:              ◄──┘
                            │                           │
                            │  ┌─ Task_Manager (P5) ──┐ │
                            │  │ 19状态机, 核心调度     │ │
                            │  │ 信号量+队列+互斥锁     │ │
                            │  │ step 0-6 自动推进     │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Chassis (P8) ──┐ │
                            │  │ 1kHz PID 速度调节     │ │
                            │  │ 麦轮里程计更新         │ │
                            │  │ 运动控制同步           │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Comm (P5) ────┐ │
                            │  │ UART 心跳维护         │ │
                            │  │ 命令帧收发管理        │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Arm (P3) ─────┐ │
                            │  │ 机械臂动作序列执行    │ │
                            │  │ 码垛层高自适应        │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Sensor (P3) ──┐ │
                            │  │ 5路灰度巡线 50Hz     │ │
                            │  │ 超声波测距            │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Obstacle (P5) ─┐ │
                            │  │ 超声波避障监控 20Hz   │ │
                            │  │ 紧急制动+绕行         │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Display (P1) ──┐ │
                            │  │ OLED 界面刷新 2Hz     │ │
                            │  │ 状态/编码/进度条/计时 │ │
                            │  └──────────────────────┘ │
                            │                           │
                            │  ┌─ Task_Monitor (P1) ──┐ │
                            │  │ LED心跳 2Hz          │ │
                            │  │ 通信健康检查          │ │
                            │  └──────────────────────┘ │
                            └───────────────────────────┘
```

## 三、竞赛流程状态机

启动后按以下 19 个状态自动推进，无需人工干预：

```
┌── START ──→ 按下蓝色启停区按钮
│
├─ [1]  IDLE                 等待启动按钮
├─ [2]  WAIT_MAIX_READY      等待视觉系统就绪 (心跳通信)
├─ [3]  MOVE_TO_QR           导航到二维码板 (麦轮全向移动)
├─ [4]  SCAN_QR              读取三位编码 (重试机制)
├─ [5]  PROCESS_QR           解析任务编码, 确定搬运顺序
│
│  ╔══ 第一阶段 ══ 原料区 → 粗加工区 (3次循环) ══╗
│  ║                                               ║
├─ [6]  MOVE_TO_RAW          到原料区
├─ [7]  FIND_MATERIAL        视觉定位目标颜色物料
├─ [8]  APPROACH_MATERIAL    微调靠近物料
├─ [9]  PICK_MATERIAL        机械臂抓取 (预抓→抓→收缩)
├─ [10] TRANSPORT_TO_ROUGH   运输到粗加工区
├─ [11] APPROACH_ROUGH       视觉确认色环位置
├─ [12] PLACE_ROUGH          精确放置 (色环环数精度)
│  ║    ↑ 重复3次 (按QR编码顺序) ↑               ║
│  ╚═══════════════════════════════════════════════╝
│
│  ╔══ 第二阶段 ══ 粗加工区 → 暂存区码垛 (3次循环) ══╗
│  ║                                                   ║
├─ [13] MOVE_TO_ROUGH_PICK   回到粗加工区取物料
├─ [14] PICK_FROM_ROUGH      从粗加工区拾取
├─ [15] TRANSPORT_TO_TEMP    运输到暂存区
├─ [16] APPROACH_TEMP        视觉确认码垛位置
├─ [17] STACK_TEMP           码垛 (3层逐层抬高机械臂)
│  ║    ↑ 重复3次 (相同顺序) ↑                       ║
│  ╚═══════════════════════════════════════════════════╝
│
├─ [18] RETURN_START         返回蓝色启停区
└─ [19] COMPLETE             停止 + 长蜂鸣 + OLED 显示完成
                          ↑
                          │ (如果有错误)
                          ▼
                      STATE_ERROR → 急停 + SOS蜂鸣
```

## 四、任务编码与颜色映射

| 数字 | 颜色 | 物料 |
|:---:|:---:|------|
| `1` | 🔴 红 | 红色塑料件 |
| `2` | 🟢 绿 | 绿色塑料件 |
| `3` | 🔵 蓝 | 蓝色塑料件 |

**示例**：
- 编码 `123` → 红 → 绿 → 蓝 顺序搬运
- 编码 `321` → 蓝 → 绿 → 红 顺序搬运

## 五、通信协议 (STM32 ↔ MaixCAM)

**帧格式**：`"前缀:类型[,参数...]\r\n"`

### STM32 → MaixCAM 命令
```
CMD:HEARTBEAT\r\n         心跳检测
CMD:READY\r\n             查询就绪
CMD:SCAN_QR\r\n           请求扫描二维码
CMD:DETECT_COLOR\r\n      请求颜色识别
CMD:FIND_MATERIAL,1\r\n   寻找颜色=1(红色)的物料
CMD:CHECK_ZONE,2\r\n      检查颜色=2(绿色)的区域
```

### MaixCAM → STM32 响应
```
RSP:READY\r\n             就绪
RSP:QR,231\r\n            QR码=231
RSP:COLOR,255,0,0,RED\r\n 颜色=红色
RSP:MAT,150,200,1,45\r\n  物料位置(x,y,颜色,角度)
RSP:ZONE,100,50,2\r\n     区域信息(x,y,颜色)
RSP:ACK\r\n               确认
RSP:ERR,timeout\r\n       错误
```

## 六、麦轮运动学

### 逆向运动学 (指令 → 4轮转速)

```
ω_FL = (1/R) * ( Vy + Vx + (Lx+Ly) * ωz )
ω_FR = (1/R) * ( Vy - Vx - (Lx+Ly) * ωz )
ω_RL = (1/R) * ( Vy - Vx + (Lx+Ly) * ωz )
ω_RR = (1/R) * ( Vy + Vx - (Lx+Ly) * ωz )
```

其中 R=32.5mm(轮半径), Lx=110mm, Ly=105mm

### 正向运动学 (编码器 → 里程计)

```
Vx = (R/4) * ( ω_FL - ω_FR - ω_RL + ω_RR )
Vy = (R/4) * ( ω_FL + ω_FR + ω_RL + ω_RR )
ωz = (R/(4*(Lx+Ly))) * ( ω_FL - ω_FR + ω_RL - ω_RR )
```

## 七、机械臂预设姿态

| 姿态 | 用途 | 腰(S1) | 大臂(S2) | 小臂(S3) | 手爪(S4) |
|------|------|:---:|:---:|:---:|:---:|
| IDLE | 空闲/行驶 | 90° | 20° | 10° | 5°(闭合) |
| SCAN | 扫描二维码 | 90° | 60° | 30° | 5° |
| PRE_PICK | 预抓取 | 90° | 80° | 60° | 60°(张开) |
| PICK | 抓取夹紧 | 90° | 85° | 65° | 5°(闭合) |
| CARRY | 收缩运输 | 90° | 50° | 40° | 5° |
| PLACE | 粗加工放置 | 90° | 80° | 60° | 30° |
| STACK | 暂存区码垛 | 90° | 90° | 80° | 30° |
| STACK+1 | 码垛第2层 | 90° | 100° | 80° | 30° |
| STACK+2 | 码垛第3层 | 90° | 110° | 80° | 30° |

## 八、传感器配置

| 传感器 | 数量 | 用途 | 更新率 |
|--------|:---:|------|:---:|
| 灰度传感器 | 5路 | 灰色车道线检测 (掉线/偏左/偏右/交叉口) | 50Hz |
| 超声波 | 1路 | 前方障碍物检测 (触发距离200mm) | 20Hz |
| 编码器 | 4路 | 电机转速测量 (11线×4倍频×30减速比) | 1kHz |
| 启动按钮 | 1个 | 一键启动 | 轮询 |

## 九、场地坐标 (单位: mm)

以启停区中心为坐标原点 `(0, 0)`，赛场 2400×2400mm：

```
                    Y+
                    ↑
                  700│  ← 暂存区 (0, -700) 580×150mm
                    │
                  300│  ← 粗加工区 (0, -300) 580×150mm
                    │
          ←─────────┼─────────→ X
           -400     │     400
                    │
                  600│  ← 原料区 (0, 600) 现场指定
                    │
               QR板 │  (400, 0) 大致位置
                    │

      启停区 (0,0) 300×300mm 蓝色
```

## 十、编译与调试

### Keil5 用户

直接用 Keil5 打开原有 `.uvprojx` 工程，代码路径指向 `code/STM32/Inc/` 和 `code/STM32/Src/`。

### VSCode 用户

详见 [VSCode调试指南.md](code/STM32/VSCode调试指南.md)，核心步骤：

```powershell
# 1. 安装工具链
arm-none-eabi-gcc --version    # ARM GCC 13.x
openocd --version              # OpenOCD
make --version                 # GNU Make

# 2. 获取 HAL 库和 FreeRTOS 源码
#    从 STM32CubeF4 包复制到 Drivers/ 和 Middlewares/

# 3. 在 VSCode 中按 F5 即可编译+烧录+调试
```

### 两种环境共存

Keil5 和 VSCode 可以**同时使用同一份代码** (`Inc/` + `Src/`)，互不影响：
- **Keil5**：你用熟悉的界面调试
- **VSCode**：GCC 编译 + OpenOCD 调试
- 代码一致，两个环境都能编译

## 十一、FreeRTOS 任务通信机制

```
任务管理器 (Task_Manager)
    │
    ├── 信号量 sem_maix_done   ← MaixCAM 响应完成信号
    ├── 信号量 sem_motion_done ← 运动到位信号
    ├── 信号量 sem_arm_done    ← 机械臂动作完成信号
    │
    ├── 队列 queue_cmd   (16槽) → Task_Arm 接收机械臂指令
    ├── 队列 queue_state (8槽)  → 状态变更广播
    │
    ├── 互斥锁 mutex_state      保护状态变量
    └── 互斥锁 mutex_pose       保护位姿变量
```

## 十二、配置文件速查

| 需求 | 文件 | 修改内容 |
|------|------|----------|
| 修改任务优先级 | `Inc/FreeRTOSConfig.h` | `PRIO_TASK_*` |
| 修改引脚分配 | `Inc/pin_config.h` | 各 `#define` |
| 修改运动速度 | `Inc/protocol.h` | `MAX_LINEAR_SPEED_MM_S` 等 |
| 修改避障距离 | `Inc/protocol.h` | `OBSTACLE_DISTANCE_MM` |
| 修改颜色阈值 | `../MaixCAM/config.py` | `RED_THRESHOLD` 等 |
| 修改场地坐标 | `Inc/protocol.h` | `QR_X_MM`, `RAW_AREA_X_MM` 等 |
| 修改PID参数 | `Src/motor.c` | `MOTOR_KP`, `MOTOR_KI`, `MOTOR_KD` |
| 修改舵机角度 | `Inc/servo.h` / `Src/servo.c` | 姿态表中的角度值 |

## 十三、关键注意事项

1. **供电**：全程使用同一个锂电池，比赛期间不可更换
2. **通信隔离**：比赛过程不能通过任何方式与机器人通信
3. **车道限制**：机器人只能在灰色车道行驶，进入其他颜色区域（除启停区）比赛结束
4. **码垛要求**：暂存区只能堆叠在已有物料上，不能放在空位上
5. **两次机会**：每个小组有两次运行机会，取最好成绩

---

> **首次上手建议流程**：
> 1. 读 `rr.md` → 理解竞赛规则
> 2. 读本文件 → 理解代码架构
> 3. 用 Keil5 编译下载 → 确认硬件功能正常
> 4. 阅读 `task_manager.c` → 理解业务流程
> 5. 修改 `protocol.h` 场地坐标 → 适配实际赛道
> 6. 修改 `config.py` 颜色阈值 → 适配实际光照
> 7. 按 `VSCode调试指南.md` → 搭建 VSCode 调试环境
