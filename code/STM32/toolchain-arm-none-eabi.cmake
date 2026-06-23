# ARM Cortex-M4 交叉编译工具链文件
# 用法: cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-none-eabi.cmake

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 工具链路径 (winget 安装默认路径)
set(TOOLCHAIN_ROOT "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/12.2 mpacbti-rel1/bin")
set(TRIPLE arm-none-eabi-)

set(CMAKE_C_COMPILER    "${TOOLCHAIN_ROOT}/${TRIPLE}gcc.exe"       CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER  "${TOOLCHAIN_ROOT}/${TRIPLE}g++.exe"       CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER  "${TOOLCHAIN_ROOT}/${TRIPLE}gcc.exe"       CACHE FILEPATH "")
set(CMAKE_OBJCOPY       "${TOOLCHAIN_ROOT}/${TRIPLE}objcopy.exe"   CACHE FILEPATH "")
set(CMAKE_SIZE          "${TOOLCHAIN_ROOT}/${TRIPLE}size.exe"      CACHE FILEPATH "")

set(CMAKE_C_COMPILER_WORKS 1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")

# 关键: 禁止 CMake 的 C 链接器测试 (它会产生 Windows PE 错误)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
