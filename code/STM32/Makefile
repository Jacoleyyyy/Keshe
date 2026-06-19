# ================================================================
# Makefile - 智能搬运机器人 STM32F407 (ARM GCC)
#
# 用法:
#   make             编译所有
#   make clean       清理
#   make flash       烧录 (ST-Link)
#   make debug       启动 OpenOCD 调试服务器
# ================================================================

# 目标 MCU
TARGET        := smart_carrier
MCU           := STM32F407VG
CPU           := cortex-m4
FPU           := fpv4-sp-d16
FLOAT-ABI     := hard

# 工具链
PREFIX        := arm-none-eabi-
CC            := $(PREFIX)gcc
CXX           := $(PREFIX)g++
AS            := $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY       := $(PREFIX)objcopy
OBJDUMP       := $(PREFIX)objdump
SIZE          := $(PREFIX)size
GDB           := $(PREFIX)gdb

# 工程路径
PROJECT_DIR   := $(shell pwd)
INC_DIR       := $(PROJECT_DIR)/Inc
SRC_DIR       := $(PROJECT_DIR)/Src
BUILD_DIR     := $(PROJECT_DIR)/build

# CMSIS / HAL 库路径 (根据你的实际安装位置调整)
ST_HAL_DIR    := $(PROJECT_DIR)/Drivers/STM32F4xx_HAL_Driver
CMSIS_CORE    := $(PROJECT_DIR)/Drivers/CMSIS/Core
CMSIS_DEVICE  := $(PROJECT_DIR)/Drivers/CMSIS/Device/ST/STM32F4xx
FREERTOS_DIR  := $(PROJECT_DIR)/Middlewares/Third_Party/FreeRTOS

# CMSIS-FreeRTOS 适配层
CMSIS_RTOS_DIR := $(PROJECT_DIR)/Middlewares/Third_Party/FreeRTOS/CMSIS_RTOS_V2

# 启动文件
STARTUP_FILE  := $(PROJECT_DIR)/startup_stm32f407xx.s
LINKER_SCRIPT := $(PROJECT_DIR)/STM32F407VGTx_FLASH.ld

# ============================================================
# 源文件
# ============================================================
C_SOURCES := \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/pid.c \
    $(SRC_DIR)/motor.c \
    $(SRC_DIR)/servo.c \
    $(SRC_DIR)/sensor.c \
    $(SRC_DIR)/display.c \
    $(SRC_DIR)/chassis.c \
    $(SRC_DIR)/communication.c \
    $(SRC_DIR)/task_manager.c

# HAL 库源文件 (需要从 STM32CubeF4 包中获取)
HAL_SOURCES := \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_cortex.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_gpio.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_rcc.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_tim.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_tim_ex.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_uart.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_i2c.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_dma.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_pwr.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_flash.c \
    $(ST_HAL_DIR)/Src/stm32f4xx_hal_rcc_ex.c

# FreeRTOS 源文件
FREERTOS_SOURCES := \
    $(FREERTOS_DIR)/Source/croutine.c \
    $(FREERTOS_DIR)/Source/event_groups.c \
    $(FREERTOS_DIR)/Source/list.c \
    $(FREERTOS_DIR)/Source/queue.c \
    $(FREERTOS_DIR)/Source/stream_buffer.c \
    $(FREERTOS_DIR)/Source/tasks.c \
    $(FREERTOS_DIR)/Source/timers.c \
    $(FREERTOS_DIR)/Source/CMSIS_RTOS_V2/cmsis_os2.c \
    $(FREERTOS_DIR)/Source/portable/MemMang/heap_4.c \
    $(FREERTOS_DIR)/Source/portable/GCC/ARM_CM4F/port.c

# 系统文件
SYSTEM_SOURCES := \
    $(CMSIS_DEVICE)/Source/Templates/system_stm32f4xx.c

# 所有源文件汇总
ALL_SOURCES    := $(C_SOURCES) $(HAL_SOURCES) $(FREERTOS_SOURCES) $(SYSTEM_SOURCES)
ALL_ASM        := $(STARTUP_FILE)

# ============================================================
# 头文件路径
# ============================================================
INC_DIRS := \
    -I$(INC_DIR) \
    -I$(ST_HAL_DIR)/Inc \
    -I$(CMSIS_CORE)/Include \
    -I$(CMSIS_DEVICE)/Include \
    -I$(FREERTOS_DIR)/Source/include \
    -I$(FREERTOS_DIR)/Source/CMSIS_RTOS_V2 \
    -I$(FREERTOS_DIR)/Source/portable/GCC/ARM_CM4F

# ============================================================
# 编译选项
# ============================================================
CPU_FLAGS    := -mcpu=$(CPU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT-ABI)
C_FLAGS      := $(CPU_FLAGS) -std=gnu11 -O0 -g3 -Wall -fdata-sections -ffunction-sections
C_DEFINES    := -DUSE_HAL_DRIVER -D$(MCU) -DSTM32F407xx
ASM_FLAGS    := $(CPU_FLAGS) -g3
LD_FLAGS     := $(CPU_FLAGS) -specs=nosys.specs -specs=nano.specs \
                -Wl,--gc-sections -T$(LINKER_SCRIPT) \
                -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

# ============================================================
# 规则
# ============================================================
OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(ALL_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ALL_ASM:.s=.o)))

vpath %.c $(sort $(dir $(ALL_SOURCES)))
vpath %.s $(sort $(dir $(ALL_ASM)))

.PHONY: all clean flash debug

all: $(BUILD_DIR)/$(TARGET).elf size

# 编译 C 文件
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "  CC    $<"
	@$(CC) -c $(C_FLAGS) $(C_DEFINES) $(INC_DIRS) $< -o $@

# 汇编 ASM 文件
$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@echo "  AS    $<"
	@$(AS) -c $(ASM_FLAGS) $< -o $@

# 链接
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	@echo "  LD    $(TARGET).elf"
	@$(CC) $(OBJECTS) $(LD_FLAGS) -o $@

# 十六进制文件 (用于烧录)
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	@$(OBJCOPY) -O ihex $< $@
	@echo "  HEX   $@"

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	@$(OBJCOPY) -O binary $< $@
	@echo "  BIN   $@"

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

size: $(BUILD_DIR)/$(TARGET).elf
	@$(SIZE) $<

# 清理
clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean done."

# 烧录 (使用 OpenOCD + ST-Link)
flash: $(BUILD_DIR)/$(TARGET).elf
	@echo "Flashing via ST-Link..."
	openocd -f interface/stlink.cfg \
	        -f target/stm32f4x.cfg \
	        -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

# 启动调试服务器 (OpenOCD)
debug-server:
	@echo "Starting OpenOCD debug server on :3333..."
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
