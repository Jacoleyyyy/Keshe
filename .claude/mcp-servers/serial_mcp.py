#!/usr/bin/env python3
"""
serial_mcp.py — 串口 MCP Server (Model Context Protocol)

让 Claude Code 直接与 STM32 的 printf 串口通信:
  - 读取 STM32 运行日志
  - 发送调试命令
  - 实时监控串口输出

协议: Model Context Protocol (stdio transport)
依赖: pip install pyserial mcp
"""

import sys
import json
import time
import threading
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("请安装 pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(1)

# ============================================================
# 配置 (可通过 MCP 工具参数覆盖)
# ============================================================
DEFAULT_PORT     = "COM3"
DEFAULT_BAUDRATE = 115200
READ_TIMEOUT     = 0.1     # 每次读取的超时 (秒)

# 串口全局实例
_ser: serial.Serial | None = None
_stop_monitor = threading.Event()
_monitor_thread: threading.Thread | None = None


# ============================================================
# 工具: serial_connect
# ============================================================
def serial_connect(port: str = DEFAULT_PORT, baudrate: int = DEFAULT_BAUDRATE) -> str:
    """连接串口 (STM32 UART3 → USB-UART)"""
    global _ser
    try:
        if _ser and _ser.is_open:
            _ser.close()
        _ser = serial.Serial(port, baudrate, timeout=READ_TIMEOUT)
        return f"已连接 {port} @ {baudrate} bps"
    except Exception as e:
        return f"连接失败: {e}\n可用端口: {[p.device for p in serial.tools.list_ports.comports()]}"


# ============================================================
# 工具: serial_read
# ============================================================
def serial_read(max_lines: int = 50) -> str:
    """读取串口缓冲区中已有的数据 (非阻塞, 吃掉已有数据后返回)"""
    global _ser
    if not _ser or not _ser.is_open:
        return "错误: 未连接串口, 请先调用 serial_connect"

    lines = []
    start = time.time()
    while len(lines) < max_lines and (time.time() - start) < 1.0:
        try:
            line = _ser.readline()
            if line:
                decoded = line.decode('utf-8', errors='ignore').strip()
                if decoded:
                    lines.append(decoded)
        except Exception:
            break

    if not lines:
        return "(串口无数据)"
    return "\n".join(lines)


# ============================================================
# 工具: serial_monitor_start
# ============================================================
def serial_monitor_start(duration_s: int = 10) -> str:
    """启动串口监控, 采集 duration_s 秒后返回所有日志"""
    global _ser, _stop_monitor, _monitor_thread
    if not _ser or not _ser.is_open:
        return "错误: 未连接串口, 请先调用 serial_connect"

    _stop_monitor.clear()
    lines = []

    def _collect():
        deadline = time.time() + duration_s
        while not _stop_monitor.is_set() and time.time() < deadline:
            try:
                line = _ser.readline()
                if line:
                    decoded = line.decode('utf-8', errors='ignore').strip()
                    if decoded:
                        lines.append(f"[{datetime.now().strftime('%H:%M:%S')}] {decoded}")
            except Exception:
                break

    _monitor_thread = threading.Thread(target=_collect, daemon=True)
    _monitor_thread.start()
    _monitor_thread.join(timeout=duration_s + 2)
    _stop_monitor.set()

    if not lines:
        return "(监控期间无数据)"
    return f"=== 串口监控 {duration_s}s, 共 {len(lines)} 行 ===\n" + "\n".join(lines)


# ============================================================
# 工具: serial_send
# ============================================================
def serial_send(data: str) -> str:
    """向串口发送数据 (例如模拟 MaixCAM 命令测试 STM32)"""
    global _ser
    if not _ser or not _ser.is_open:
        return "错误: 未连接串口"

    if not data.endswith('\r\n'):
        data += '\r\n'
    _ser.write(data.encode('utf-8'))
    return f"已发送: {data.strip()}"


# ============================================================
# 工具: serial_list_ports
# ============================================================
def serial_list_ports() -> str:
    """列出所有可用串口"""
    ports = serial.tools.list_ports.comports()
    if not ports:
        return "未检测到串口设备"
    lines = []
    for p in ports:
        lines.append(f"  {p.device} — {p.description}" + (f" (VID:{p.vid:04X}/PID:{p.pid:04X})" if p.vid else ""))
    return "\n".join(lines)


# ============================================================
# 工具: serial_disconnect
# ============================================================
def serial_disconnect() -> str:
    """断开串口"""
    global _ser
    if _ser and _ser.is_open:
        _ser.close()
        _ser = None
        return "已断开"
    return "未连接"


# ============================================================
# MCP stdio 协议处理
# ============================================================
TOOLS = {
    "serial_connect":        serial_connect,
    "serial_read":           serial_read,
    "serial_monitor_start":  serial_monitor_start,
    "serial_send":           serial_send,
    "serial_list_ports":     serial_list_ports,
    "serial_disconnect":     serial_disconnect,
}

TOOL_SCHEMAS = {
    "serial_connect": {
        "description": "连接 STM32 串口 (USART3 printf 输出)",
        "inputSchema": {
            "type": "object",
            "properties": {
                "port":     {"type": "string", "description": "串口号, 如 COM3"},
                "baudrate": {"type": "integer", "description": "波特率, 默认 115200"}
            }
        }
    },
    "serial_read": {
        "description": "读取串口缓冲区中已有的 printf 日志 (非阻塞, 立即返回)",
        "inputSchema": {
            "type": "object",
            "properties": {
                "max_lines": {"type": "integer", "description": "最大读取行数, 默认 50"}
            }
        }
    },
    "serial_monitor_start": {
        "description": "启动串口监控 N 秒, 采集所有输出后返回 (用于机器人跑一圈后看日志)",
        "inputSchema": {
            "type": "object",
            "properties": {
                "duration_s": {"type": "integer", "description": "监控时长 (秒), 默认 10"}
            }
        }
    },
    "serial_send": {
        "description": "向串口发送数据 (可模拟 MaixCAM 命令测试 STM32 响应)",
        "inputSchema": {
            "type": "object",
            "properties": {
                "data": {"type": "string", "description": "要发送的字符串 (自动加 \\r\\n 结尾)"}
            },
            "required": ["data"]
        }
    },
    "serial_list_ports": {
        "description": "列出电脑上所有可用的串口设备",
        "inputSchema": {"type": "object", "properties": {}}
    },
    "serial_disconnect": {
        "description": "断开当前串口连接",
        "inputSchema": {"type": "object", "properties": {}}
    },
}


def handle_request(req: dict) -> dict:
    """处理单个 JSON-RPC 请求"""
    method = req.get("method", "")
    req_id = req.get("id")

    # initialize
    if method == "initialize":
        return {
            "jsonrpc": "2.0", "id": req_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "serial-mcp", "version": "1.0.0"}
            }
        }

    # initialized
    if method == "notifications/initialized":
        return None  # 不回复 notification

    # tools/list
    if method == "tools/list":
        tools = [{"name": name, **schema} for name, schema in TOOL_SCHEMAS.items()]
        return {"jsonrpc": "2.0", "id": req_id, "result": {"tools": tools}}

    # tools/call
    if method == "tools/call":
        tool_name = req["params"]["name"]
        tool_args = req["params"].get("arguments", {})
        func = TOOLS.get(tool_name)
        if not func:
            return {"jsonrpc": "2.0", "id": req_id, "error": {"code": -32601, "message": f"Unknown tool: {tool_name}"}}

        try:
            result = func(**tool_args)
            return {
                "jsonrpc": "2.0", "id": req_id,
                "result": {"content": [{"type": "text", "text": str(result)}]}
            }
        except Exception as e:
            return {
                "jsonrpc": "2.0", "id": req_id,
                "result": {"content": [{"type": "text", "text": f"工具异常: {e}"}], "isError": True}
            }

    # 未知方法
    return {"jsonrpc": "2.0", "id": req_id, "error": {"code": -32601, "message": f"Unknown method: {method}"}}


def main():
    """stdio MCP 主循环"""
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
