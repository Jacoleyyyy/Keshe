# ================================================================
# CMakeLists.txt — 智能搬运机器人 STM32F407 (ARM GCC + CMake)
#
# 前置条件: ARM GNU Toolchain (arm-none-eabi-gcc) 在 PATH 中
#
# 用法:
#   cmake -B build -G Ninja                        # 配置 (Ninja 构建)
#   cmake -B build -G "MinGW Makefiles"             # 配置 (MinGW Make)
#   cmake --build build -j8                         # 编译
#   cmake --build build --target flash              # 烧录 (pyOCD)
#   cmake --build build --target flash-openocd      # 烧录 (OpenOCD)
#   cmake --build build --target clean              # 清理
# ================================================================
cmake_minimum_required(VERSION 3.20)
project(smart_carrier C ASM)

# ============================================================
# 工具链配置
# ============================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # GNU extensions

# 交叉编译
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TRIPLE arm-none-eabi-)
set(CMAKE_C_COMPILER    ${TRIPLE}gcc)
set(CMAKE_CXX_COMPILER  ${TRIPLE}g++)
set(CMAKE_ASM_COMPILER  ${TRIPLE}gcc)
set(CMAKE_OBJCOPY       ${TRIPLE}objcopy)
set(CMAKE_SIZE          ${TRIPLE}size)

# MCU 参数
set(MCU        STM32F407VE)
set(CPU        cortex-m4)
set(FPU        fpv4-sp-d16)
set(FLOAT_ABI  hard)

# ============================================================
# 编译选项
# ============================================================
add_compile_options(
    -mcpu=${CPU}
    -mthumb
    -mfpu=${FPU}
    -mfloat-abi=${FLOAT_ABI}
    -O0 -g3                          # 调试: 无优化 + 全量调试信息
    -Wall -Wextra
    -fdata-sections -ffunction-sections
)

add_link_options(
    -mcpu=${CPU} -mthumb -mfpu=${FPU} -mfloat-abi=${FLOAT_ABI}
    -specs=nosys.specs -specs=nano.specs
    -Wl,--gc-sections
)

add_definitions(
    -DUSE_HAL_DRIVER
    -D${MCU}
    -DSTM32F407xx
)

# ============================================================
# 库路径 (需从 STM32CubeF4 复制)
# ============================================================
set(CMSIS_CORE    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Core)
set(CMSIS_DEVICE  ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx)
set(HAL_DIR       ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver)
set(FREERTOS_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source)

# ============================================================
# 头文件路径
# ============================================================
set(INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMSIS_CORE}/Include
    ${CMSIS_DEVICE}/Include
    ${HAL_DIR}/Inc
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/CMSIS_RTOS_V2
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
)

# ============================================================
# 应用源文件
# ============================================================
file(GLOB APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/pid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/motor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/sensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/chassis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/communication.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/task_manager.c
)

# ============================================================
# HAL 库源文件
# ============================================================
set(HAL_SOURCES
    ${HAL_DIR}/Src/stm32f4xx_hal.c
    ${HAL_DIR}/Src/stm32f4xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_tim.c
    ${HAL_DIR}/Src/stm32f4xx_hal_tim_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_uart.c
    ${HAL_DIR}/Src/stm32f4xx_hal_dma.c
    ${HAL_DIR}/Src/stm32f4xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32f4xx_hal_flash.c
)

# ============================================================
# CMSIS 系统文件
# ============================================================
set(CMSIS_SOURCES
    ${CMSIS_DEVICE}/Source/Templates/system_stm32f4xx.c
)

# ============================================================
# FreeRTOS 源文件
# ============================================================
set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/event_groups.c
    ${FREERTOS_DIR}/stream_buffer.c
    ${FREERTOS_DIR}/CMSIS_RTOS_V2/cmsis_os2.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
)

# ============================================================
# 启动文件
# ============================================================
set(STARTUP_FILE ${CMSIS_DEVICE}/Source/Templates/gcc/startup_stm32f407xx.s)

# ============================================================
# 链接脚本
# ============================================================
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VETx_FLASH.ld)
# 如果没有独立的 .ld 文件, 使用 CubeF4 提供的
if(NOT EXISTS ${LINKER_SCRIPT})
    set(LINKER_SCRIPT ${CMSIS_DEVICE}/Source/Templates/gcc/linker/STM32F407VE_FLASH.ld)
endif()

# ============================================================
# 可执行文件
# ============================================================
add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${HAL_SOURCES}
    ${CMSIS_SOURCES}
    ${FREERTOS_SOURCES}
    ${STARTUP_FILE}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${INC_DIRS})

target_link_options(${PROJECT_NAME} PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
)

# ============================================================
# 编译后生成 .hex / .bin
# ============================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating .hex .bin + size report"
)

# ============================================================
# 烧录任务: pyOCD (推荐)
# ============================================================
add_custom_target(flash
    COMMAND pyocd load ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
            --target stm32f407ve
            --frequency 4000000
    COMMENT "Flashing via pyOCD (ST-Link)..."
    DEPENDS ${PROJECT_NAME}
)

# ============================================================
# 烧录任务: OpenOCD (备选)
# ============================================================
add_custom_target(flash-openocd
    COMMAND openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
            -c "program ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex verify reset exit"
    COMMENT "Flashing via OpenOCD (ST-Link)..."
    DEPENDS ${PROJECT_NAME}
)

# ============================================================
# 清理 (CMake 自带 clean, 这里显式声明以便 task.json 调用)
# ============================================================
# cmake --build build --target clean 即可, 无需自定义
