#!/bin/bash
# ================================================================
#  智能搬运机器人 — 一键环境配置脚本
#  跑一次，全部就绪，然后 VSCode 按 F5 就能调试
# ================================================================
set -e

RED='\033[31m'; GREEN='\033[32m'; YELLOW='\033[33m'; CYAN='\033[36m'; NC='\033[0m'
info()  { echo -e "${CYAN}[INFO]${NC} $1"; }
ok()    { echo -e "${GREEN}[OK]${NC}   $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()   { echo -e "${RED}[ERR]${NC}  $1"; }

echo "============================================================"
echo "  智能搬运机器人 — 开发环境一键安装"
echo "  STM32F407VE + FreeRTOS + CMake + clangd + pyOCD"
echo "============================================================"
echo ""

# ============================================================
# 1. CMake
# ============================================================
if command -v cmake &>/dev/null; then
    ok "CMake $(cmake --version | head -1 | awk '{print $3}')"
else
    info "安装 CMake..."
    winget install --silent Kitware.CMake 2>/dev/null || {
        warn "winget 安装 CMake 失败, 请手动下载: https://cmake.org/download/"
    }
    # 刷新 PATH
    export PATH="/c/Program Files/CMake/bin:$PATH"
    command -v cmake &>/dev/null && ok "CMake 安装成功" || err "CMake 安装失败, 请手动安装后重新运行"
fi

# ============================================================
# 2. Ninja
# ============================================================
if command -v ninja &>/dev/null; then
    ok "Ninja $(ninja --version)"
else
    info "安装 Ninja..."
    winget install --silent Ninja-build.Ninja 2>/dev/null || {
        warn "winget 安装 Ninja 失败, 请手动下载: https://github.com/ninja-build/ninja/releases"
    }
    export PATH="/c/Program Files/Ninja:$PATH"
    command -v ninja &>/dev/null && ok "Ninja 安装成功" || err "Ninja 安装失败"
fi

# ============================================================
# 3. ARM GNU Toolchain ← 最关键的
# ============================================================
GCC_EXPECTED="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi"
if command -v arm-none-eabi-gcc &>/dev/null; then
    ok "ARM GCC $(arm-none-eabi-gcc --version | head -1 | awk '{print $NF}')"
else
    # 检查常见安装路径
    if ls "$GCC_EXPECTED"/*/bin/arm-none-eabi-gcc.exe 2>/dev/null; then
        GCC_DIR=$(ls -d "$GCC_EXPECTED"/*/bin 2>/dev/null | head -1)
        export PATH="$GCC_DIR:$PATH"
        ok "ARM GCC 找到: $GCC_DIR"
    else
        warn "ARM GNU Toolchain 未安装"
        echo ""
        echo "  >>> 请手动下载安装 (勾选 'Add to PATH'):"
        echo "  https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
        echo "  选: Windows (mingw-w64-i686) AArch32 bare-metal target (arm-none-eabi)"
        echo "  推荐版本: 13.3.rel1"
        echo ""
        echo "  安装完成后重新运行本脚本"
        echo ""
        read -p "  安装完成后按 Enter 继续..."
    fi
fi

# ============================================================
# 4. pyOCD
# ============================================================
if command -v pyocd &>/dev/null; then
    ok "pyOCD $(pyocd --version 2>&1 | head -1)"
else
    info "安装 pyOCD..."
    pip install pyocd -q 2>/dev/null && ok "pyOCD 安装成功" || err "pyOCD 安装失败"
fi

# ============================================================
# 5. pyserial (MCP 串口)
# ============================================================
python -c "import serial" 2>/dev/null && ok "pyserial OK" || {
    info "安装 pyserial..."
    pip install pyserial -q 2>/dev/null && ok "pyserial 安装成功"
}

# ============================================================
# 6. arm-none-eabi-gdb
# ============================================================
command -v arm-none-eabi-gdb &>/dev/null && ok "ARM GDB OK" || {
    warn "ARM GDB 未找到 (与 GCC 工具链捆绑安装)"
}

# ============================================================
# 7. 编译验证
# ============================================================
echo ""
info "验证最终状态..."
echo ""

check() {
    command -v "$1" &>/dev/null && echo -e "  ${GREEN}✅${NC} $1" || echo -e "  ${RED}❌${NC} $1 (缺失)"
}
check "arm-none-eabi-gcc"
check "arm-none-eabi-gdb"
check "cmake"
check "ninja"
check "pyocd"
check "python"

echo ""

# 如果所有关键工具就绪, 直接编译
if command -v arm-none-eabi-gcc &>/dev/null && command -v cmake &>/dev/null && command -v ninja &>/dev/null; then
    STM32_DIR="$(dirname "$0")/code/STM32"
    cd "$STM32_DIR"

    info "开始编译..."
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 2>&1 | tail -3
    cp -f build/compile_commands.json . 2>/dev/null
    cmake --build build -j8 2>&1

    if [ -f "build/smart_carrier.elf" ]; then
        echo ""
        ok "============================================"
        ok "  环境配置完成! 编译成功!"
        ok "============================================"
        echo ""
        echo "  接下来:"
        echo "  1. 用 VSCode 打开: f:/KeShe/code/STM32/"
        echo "  2. 按 Ctrl+Shift+B → 编译"
        echo "  3. 接 ST-Link → 按 F5 → 调试"
        echo "  4. 或命令行: cd f:/KeShe && claude (AI 模式)"
        echo ""
    else
        err "编译有错误, 请检查上方输出"
    fi
else
    warn "部分工具缺失, 请安装后重新运行本脚本"
    echo "  缺失的工具需要手动安装 (见上方提示)"
fi
