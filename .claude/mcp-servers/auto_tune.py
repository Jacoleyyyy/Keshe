#!/usr/bin/env python3
"""
auto_tune.py — 闭环 PID 自动调参 MCP Server

使用方式 (Claude Code 中):
  你: 帮我把电机A PID调好
  AI → serial_connect("COM3") → auto_tune("COM3", motor=0, target_rpm=100, rounds=3)
  → 全自动: 测 → 分析 → 算 → 写 → 再测 → 收敛

协议: Model Context Protocol (stdio transport)
依赖: pip install pyserial
"""

import sys
import json
import time
import math
import threading

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("请安装 pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(1)


# ============================================================
# 阶跃响应分析
# ============================================================
class StepAnalyzer:
    """分析阶跃响应 CSV 数据, 计算特征指标"""

    def __init__(self, target_rpm: float):
        self.target = target_rpm
        self.data = []       # [(tick, actual_rpm, error, duty), ...]

    def feed(self, line: str):
        """喂入一行 STEP CSV"""
        # 格式: STEP,motor,tick,target,actual,error,duty
        if not line.startswith("STEP,"):
            return
        parts = line.split(",")
        if len(parts) < 7 or parts[2] == "END" or parts[2] == "START":
            return
        try:
            tick = int(parts[2])
            actual = float(parts[4])
            error = float(parts[5])
            duty = float(parts[6])
            self.data.append((tick, actual, error, duty))
        except (ValueError, IndexError):
            pass

    def analyze(self) -> dict:
        """
        分析阶跃响应:
          - rise_time: 从 10% 到 90% 的上升时间 (ms)
          - overshoot_pct: 超调百分比
          - settling_time: 进入 ±5% 稳态的时间 (ms)
          - steady_error: 稳态误差 (RPM)
          - peak: 最高转速
        """
        if len(self.data) < 10:
            return {"error": "数据太少, 无法分析"}

        # 按 tick 排序
        self.data.sort(key=lambda x: x[0])

        actuals = [d[1] for d in self.data]
        errors  = [d[2] for d in self.data]
        ticks   = [d[0] for d in self.data]

        peak = max(actuals, key=abs)
        steady = actuals[-1]  # 最后的值

        # 上升时间: 10% → 90%
        t10 = t90 = None
        for t, a, _, _ in self.data:
            if a >= self.target * 0.10 and t10 is None:
                t10 = t
            if a >= self.target * 0.90 and t90 is None:
                t90 = t
        rise_time = (t90 - t10) if (t10 and t90) else None

        # 超调
        overshoot_pct = ((peak - self.target) / self.target * 100) if peak > self.target else 0.0

        # 稳态误差 (取最后 20% 数据的平均值)
        n = max(1, len(actuals) // 5)
        steady_error = abs(sum(actuals[-n:]) / n - self.target)

        # 稳定时间: 从最大值开始, 找第一个进入 ±5% 且不再超出的点
        settling_time = None
        for idx in range(len(self.data)):
            t, a, _, _ = self.data[idx]
            after = actuals[idx:]
            if all(abs(v - self.target) <= self.target * 0.05 for v in after):
                settling_time = t
                break

        # 是否振荡 (误差符号交替 > 3 次)
        osc_count = 0
        last_sign = 0
        for e in errors:
            sign = 1 if e > 0 else (-1 if e < 0 else 0)
            if sign != 0 and sign != last_sign:
                osc_count += 1
                last_sign = sign
        oscillating = osc_count > 6

        return {
            "rise_time_ms": rise_time,
            "overshoot_pct": round(overshoot_pct, 1),
            "settling_time_ms": settling_time,
            "steady_error_rpm": round(steady_error, 2),
            "peak_rpm": round(peak, 1),
            "oscillating": oscillating,
            "data_points": len(self.data),
        }


# ============================================================
# PID 参数优化器
# ============================================================
class PIDOptimizer:
    """基于启发式规则的 PID 参数优化

    规则:
      1. 超调 > 20% → 增大 Kd (微分抑制)
      2. 超调 < 5% 但上升慢 → 增大 Kp
      3. 稳态误差 > 2% → 增大 Ki
      4. 振荡 → 减小 Kp, 增大 Kd
    """

    @staticmethod
    def compute(kp: float, ki: float, kd: float, metrics: dict) -> tuple:
        """根据分析结果计算新的 Kp,Ki,Kd"""
        if "error" in metrics:
            return kp, ki, kd

        new_kp, new_ki, new_kd = kp, ki, kd

        overshoot = metrics.get("overshoot_pct", 0)
        steady_err = metrics.get("steady_error_rpm", 0)
        oscillating = metrics.get("oscillating", False)
        rise_time = metrics.get("rise_time_ms")

        target_rpm = 100.0  # 近似, 用来算百分比

        # 振荡: 大幅降 Kp + 加 Kd
        if oscillating:
            new_kp *= 0.7
            new_kd *= 1.5
            return round(new_kp, 5), round(new_ki, 5), round(new_kd, 5)

        # 超调过大
        if overshoot > 20:
            new_kd *= 1.3
            new_kp *= 0.95
        elif overshoot > 10:
            new_kd *= 1.15
        elif overshoot < 3:
            # 几乎没超调, 可以激进一点
            if rise_time and rise_time > 50:
                new_kp *= 1.1

        # 稳态误差
        if steady_err > 3.0:
            new_ki *= 1.3
        elif steady_err > 1.0:
            new_ki *= 1.1
        elif steady_err < 0.5 and overshoot < 5:
            # 完美, 不动
            pass

        # 上升时间太慢
        if rise_time and rise_time > 80:
            new_kp *= 1.15

        # 限幅
        new_kp = max(0.01, min(0.50, new_kp))
        new_ki = max(0.001, min(0.10, new_ki))
        new_kd = max(0.001, min(0.05, new_kd))

        return round(new_kp, 5), round(new_ki, 5), round(new_kd, 5)


# ============================================================
# 闭环调参主流程
# ============================================================
def auto_tune(port: str, motor: int = 0, target_rpm: float = 100.0,
              step_ms: int = 400, rounds: int = 4, baudrate: int = 115200) -> str:
    """
    全自动 PID 调参

    Args:
        port:      串口号, 如 COM3
        motor:     电机编号 (0=A, 1=B, 2=C, 3=D)
        target_rpm: 目标转速
        step_ms:   每次测试持续毫秒
        rounds:    最大调参轮数
        baudrate:  波特率
    """
    lines = []
    lines.append(f"=== PID 自动调参: 电机{motor}, 目标{target_rpm}RPM, 最多{rounds}轮 ===")

    # 连接
    try:
        ser = serial.Serial(port, baudrate, timeout=0.1)
    except Exception as e:
        return f"串口连接失败: {e}"

    time.sleep(0.2)

    # 查询当前参数
    ser.write(b"PID:?\r\n")
    time.sleep(0.3)
    resp = ser.read(500).decode(errors='ignore')
    lines.append(f"初始参数: {resp.strip()}")

    # 解析当前 Kp/Ki/Kd
    kp, ki, kd = 0.06, 0.015, 0.008  # 默认
    for part in resp.split():
        if part.startswith("kp="):
            try:
                kp = float(part.split("=")[1].rstrip(","))
            except:
                pass
        if part.startswith("ki="):
            try:
                ki = float(part.split("=")[1].rstrip(","))
            except:
                pass
        if part.startswith("kd="):
            try:
                kd = float(part.split("=")[1].rstrip(","))
            except:
                pass

    best_kp, best_ki, best_kd = kp, ki, kd
    best_score = 999

    for round_n in range(rounds):
        lines.append(f"\n--- 第 {round_n+1}/{rounds} 轮 ---")
        lines.append(f"当前: kp={kp:.5f} ki={ki:.5f} kd={kd:.5f}")

        # 1. 发送阶跃测试
        cmd = f"TST:{motor},{int(target_rpm)},{step_ms}\r\n"
        ser.write(cmd.encode())
        time.sleep(0.1)

        # 2. 收集数据
        analyzer = StepAnalyzer(target_rpm)
        deadline = time.time() + (step_ms / 1000.0) + 1.0
        while time.time() < deadline:
            try:
                line = ser.readline()
                if line:
                    decoded = line.decode(errors='ignore').strip()
                    analyzer.feed(decoded)
            except Exception:
                break

        # 3. 分析
        metrics = analyzer.analyze()
        if "error" in metrics:
            lines.append(f"❌ {metrics['error']}")
            break

        lines.append(f"数据点: {metrics['data_points']}")
        lines.append(f"上升时间: {metrics['rise_time_ms']}ms")
        lines.append(f"超调: {metrics['overshoot_pct']}%")
        lines.append(f"稳态误差: {metrics['steady_error_rpm']}RPM")
        lines.append(f"峰值: {metrics['peak_rpm']}RPM")
        if metrics['oscillating']:
            lines.append("⚠️ 检测到振荡")

        # 4. 打分 (越低越好)
        score = (metrics['steady_error_rpm'] * 10 +
                 metrics['overshoot_pct'] * 2 +
                 (metrics['rise_time_ms'] or 100) * 0.1)
        lines.append(f"综合评分: {score:.1f} (越低越好)")

        if score < best_score:
            best_score = score
            best_kp, best_ki, best_kd = kp, ki, kd

        # 5. 判断收敛
        if (metrics['overshoot_pct'] < 8 and
            metrics['steady_error_rpm'] < 2.0 and
            not metrics['oscillating'] and
            (metrics['rise_time_ms'] or 999) < 100):
            lines.append(f"✅ 已收敛! 最佳参数: kp={kp:.5f} ki={ki:.5f} kd={kd:.5f}")
            break

        # 6. 计算新参数
        new_kp, new_ki, new_kd = PIDOptimizer.compute(kp, ki, kd, metrics)
        lines.append(f"调整: kp={kp:.5f}→{new_kp:.5f} ki={ki:.5f}→{new_ki:.5f} kd={kd:.5f}→{new_kd:.5f}")

        # 7. 写入新参数
        set_cmd = f"PID:{new_kp:.5f},{new_ki:.5f},{new_kd:.5f}\r\n"
        ser.write(set_cmd.encode())
        time.sleep(0.2)
        ser.read(500)  # 清掉 ACK

        kp, ki, kd = new_kp, new_ki, new_kd

    # 报告
    lines.append(f"\n=== 调参完成 ===")
    lines.append(f"最佳参数: kp={best_kp:.5f} ki={best_ki:.5f} kd={best_kd:.5f}")
    lines.append(f"写入当前值: kp={kp:.5f} ki={ki:.5f} kd={kd:.5f}")
    lines.append(f"如需固化, 修改 motor.c 中 g_pid_kp/ki/kd 初始值")

    ser.close()
    return "\n".join(lines)


# ============================================================
# MCP 协议
# ============================================================
TOOLS = {"auto_tune": auto_tune}
TOOL_SCHEMAS = {
    "auto_tune": {
        "description": "全自动 PID 调参 — 连接串口, 运行阶跃测试, 分析响应, 自动计算并写入最优 Kp/Ki/Kd, 迭代直到收敛",
        "inputSchema": {
            "type": "object",
            "properties": {
                "port":       {"type": "string", "description": "串口号 (如 COM3)"},
                "motor":      {"type": "integer", "description": "电机编号: 0=A, 1=B, 2=C, 3=D", "default": 0},
                "target_rpm": {"type": "number", "description": "阶跃测试目标转速 (RPM)", "default": 100.0},
                "step_ms":    {"type": "integer", "description": "每次测试持续毫秒", "default": 400},
                "rounds":     {"type": "integer", "description": "最大迭代轮数", "default": 4},
                "baudrate":   {"type": "integer", "description": "波特率", "default": 115200},
            },
            "required": ["port"]
        }
    }
}


def handle_request(req: dict) -> dict | None:
    method = req.get("method", "")
    rid = req.get("id")

    if method == "initialize":
        return {"jsonrpc": "2.0", "id": rid, "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "auto-tune-mcp", "version": "1.0.0"}
        }}
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
