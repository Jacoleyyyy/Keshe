#!/usr/bin/env python3
"""
pyocd_mcp.py — STM32 烧录/调试 MCP Server (Model Context Protocol)

让 Claude Code 直接编译、烧录 STM32:
  - 编译项目 (调用 CMake)
  - 烧录固件 (调用 pyOCD)
  - 检查工具链状态

协议: Model Context Protocol (stdio transport)
依赖: pip install pyserial (pyocd 自带)
"""

import sys
import json
import subprocess
import os

# ============================================================
# 项目路径 (自动检测)
# ============================================================
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
STM32_DIR   = os.path.join(PROJECT_DIR, "code", "STM32")
BUILD_DIR   = os.path.join(STM32_DIR, "build")
TARGET_HEX  = os.path.join(BUILD_DIR, "smart_carrier.hex")
TARGET_ELF  = os.path.join(BUILD_DIR, "smart_carrier.elf")


# ============================================================
# 工具: build
# ============================================================
def build() -> str:
    """编译 STM32 项目 (CMake --build)"""
    if not os.path.isdir(BUILD_DIR):
        return "错误: build/ 目录不存在, 请先运行 cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug"

    result = subprocess.run(
        ["cmake", "--build", BUILD_DIR, "-j8"],
        capture_output=True, text=True, cwd=STM32_DIR, timeout=120
    )
    if result.returncode == 0:
        # 提取最后几行 (size report)
        lines = result.stdout.strip().split('\n')
        tail = '\n'.join(lines[-8:]) if len(lines) > 8 else result.stdout
        return f"✅ 编译成功\n{tail}"
    else:
        # 提取错误行
        err_lines = [l for l in (result.stdout + result.stderr).split('\n') if 'error:' in l.lower()]
        return f"❌ 编译失败 (返回码 {result.returncode})\n" + '\n'.join(err_lines[-10:])


# ============================================================
# 工具: flash
# ============================================================
def flash() -> str:
    """烧录固件到 STM32F407VE (pyOCD)"""
    if not os.path.isfile(TARGET_HEX):
        return f"错误: 找不到 {TARGET_HEX}, 请先编译"

    result = subprocess.run(
        ["pyocd", "load", TARGET_HEX, "--target", "stm32f407ve", "--frequency", "4000000"],
        capture_output=True, text=True, cwd=STM32_DIR, timeout=30
    )
    if result.returncode == 0:
        return f"✅ 烧录成功\n{result.stdout.strip()[-200:]}"
    else:
        return f"❌ 烧录失败\n{result.stderr.strip()[:500]}"


# ============================================================
# 工具: build_and_flash
# ============================================================
def build_and_flash() -> str:
    """编译并烧录 (一键)"""
    b = build()
    if "失败" in b:
        return b
    f = flash()
    return b + "\n\n" + f


# ============================================================
# 工具: check_toolchain
# ============================================================
def check_toolchain() -> str:
    """检查工具链是否就绪"""
    checks = {}
    for name, cmd in [
        ("arm-none-eabi-gcc", ["arm-none-eabi-gcc", "--version"]),
        ("cmake",              ["cmake", "--version"]),
        ("ninja",              ["ninja", "--version"]),
        ("pyocd",              ["pyocd", "--version"]),
        ("arm-none-eabi-gdb",  ["arm-none-eabi-gdb", "--version"]),
    ]:
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            first_line = r.stdout.strip().split('\n')[0] if r.stdout else r.stderr.strip().split('\n')[0]
            checks[name] = f"✅ {first_line}"
        except FileNotFoundError:
            checks[name] = "❌ 未安装"
        except Exception as e:
            checks[name] = f"⚠️ {e}"

    lines = ["=== 工具链状态 ==="]
    for k, v in checks.items():
        lines.append(f"  {k}: {v}")
    return "\n".join(lines)


# ============================================================
# 工具: cmake_configure
# ============================================================
def cmake_configure() -> str:
    """CMake 配置 (只需运行一次)"""
    result = subprocess.run(
        ["cmake", "-B", BUILD_DIR, "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Debug",
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
        capture_output=True, text=True, cwd=STM32_DIR, timeout=60
    )
    if result.returncode == 0:
        return f"✅ CMake 配置成功\n{result.stdout.strip()[-300:]}"
    else:
        return f"❌ CMake 配置失败\n{result.stderr.strip()[:500]}"


# ============================================================
# MCP 协议处理
# ============================================================
TOOLS = {
    "build":            build,
    "flash":            flash,
    "build_and_flash":  build_and_flash,
    "check_toolchain":  check_toolchain,
    "cmake_configure":  cmake_configure,
}

TOOL_SCHEMAS = {
    "build": {
        "description": "编译 STM32 项目 (cmake --build build -j8)",
        "inputSchema": {"type": "object", "properties": {}}
    },
    "flash": {
        "description": "烧录固件到 STM32F407VE (pyOCD + ST-Link)",
        "inputSchema": {"type": "object", "properties": {}}
    },
    "build_and_flash": {
        "description": "编译 + 烧录 一键完成",
        "inputSchema": {"type": "object", "properties": {}}
    },
    "check_toolchain": {
        "description": "检查 ARM GCC / CMake / Ninja / pyOCD / GDB 是否就绪",
        "inputSchema": {"type": "object", "properties": {}}
    },
    "cmake_configure": {
        "description": "CMake 配置 (首次使用或 CMakeLists.txt 变更后)",
        "inputSchema": {"type": "object", "properties": {}}
    },
}


def handle_request(req: dict) -> dict | None:
    method = req.get("method", "")
    rid = req.get("id")

    if method == "initialize":
        return {
            "jsonrpc": "2.0", "id": rid,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "pyocd-mcp", "version": "1.0.0"}
            }
        }

    if method == "notifications/initialized":
        return None

    if method == "tools/list":
        tools = [{"name": n, **s} for n, s in TOOL_SCHEMAS.items()]
        return {"jsonrpc": "2.0", "id": rid, "result": {"tools": tools}}

    if method == "tools/call":
        name = req["params"]["name"]
        args = req["params"].get("arguments", {})
        func = TOOLS.get(name)
        if not func:
            return {"jsonrpc": "2.0", "id": rid, "error": {"code": -32601, "message": f"Unknown: {name}"}}
        try:
            result = func(**args)
            return {"jsonrpc": "2.0", "id": rid, "result": {"content": [{"type": "text", "text": str(result)}]}}
        except Exception as e:
            return {"jsonrpc": "2.0", "id": rid, "result": {"content": [{"type": "text", "text": f"异常: {e}"}], "isError": True}}

    return {"jsonrpc": "2.0", "id": rid, "error": {"code": -32601, "message": f"Unknown: {method}"}}


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            continue
        resp = handle_request(req)
        if resp:
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
