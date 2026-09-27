#!/usr/bin/env python3
"""
monitor.py —— STM32 IMU 上位机

功能：
  1. 从串口（或内置模拟器）接收下位机数据帧，按协议解析
  2. 内置 HTTP 服务，浏览器打开 http://127.0.0.1:8080 即可看到
     实时曲线 + 3D 姿态立方体 + 数值面板
  3. 一键 CSV 记录，方便事后分析 / 画论文图
  4. 支持向下位机下发命令（校准 / 暂停 / 状态 / 复位）

用法：
  python monitor.py                 # 自动模式：有串口就用串口，没有就进模拟模式
  python monitor.py --port COM3     # 指定串口（Windows）
  python monitor.py --port /dev/ttyUSB0 --baud 115200
  python monitor.py --sim           # 强制模拟模式（无硬件也能演示全链路）
  python monitor.py --list          # 列出可用串口
  python monitor.py --csv           # 启动即开始记录 CSV

说明：之所以做成"内置 HTTP + 浏览器展示"而不是 tkinter/matplotlib 窗口，
      是因为它跨平台、零 GUI 依赖，用手机/平板同局域网也能看，演示更方便。
"""
import argparse
import csv
import json
import math
import os
import random
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

try:
    import serial                      # type: ignore
    import serial.tools.list_ports     # type: ignore
    HAVE_SERIAL = True
except ImportError:
    HAVE_SERIAL = False

from protocol import (  # noqa: E402
    FrameParser, parse_data, parse_status, build_data_frame,
    FRAME_TYPE_DATA, FRAME_TYPE_STATUS, FRAME_TYPE_LOG,
    CMD_CALIBRATE, CMD_STATUS, CMD_TOGGLE, CMD_RESET,
)

HERE = os.path.dirname(os.path.abspath(__file__))
WEB_DIR = os.path.join(HERE, "web")
LOG_DIR = os.path.join(HERE, "logs")

DEFAULT_BAUD = 115200
DEFAULT_HTTP_PORT = 8080


# =====================================================================
#  数据中心：保存最近若干帧 + 统计信息
# =====================================================================
class Hub:
    def __init__(self, maxlen: int = 400):
        self.lock = threading.Lock()
        self.frames = deque(maxlen=maxlen)
        self.logs = deque(maxlen=60)
        self.status: dict = {}
        self.frame_count = 0
        self.seq_last = 0
        self.seq_lost = 0
        self.crc_errors = 0
        self.start_time = time.time()
        self.rate = 0.0
        self._rate_t0 = time.time()
        self._rate_n0 = 0
        self.source_name = "idle"

    def add(self, d: dict, seq: int, tick: int) -> None:
        with self.lock:
            if self.seq_last and seq > self.seq_last:
                gap = seq - self.seq_last - 1
                if 0 < gap < 1000:
                    self.seq_lost += gap
            self.seq_last = seq
            item = {
                "t": tick,
                "roll": d["roll"], "pitch": d["pitch"], "yaw": d["yaw"],
                "ax": d["accel_g"][0], "ay": d["accel_g"][1], "az": d["accel_g"][2],
                "gx": d["gyro_dps"][0], "gy": d["gyro_dps"][1], "gz": d["gyro_dps"][2],
                "temp": d["mpu_temp"],
                "adc": list(d["adc"]),
                "chip": d["chip_temp"],
            }
            self.frames.append(item)
            self.frame_count += 1

            now = time.time()
            if now - self._rate_t0 >= 1.0:
                self.rate = (self.frame_count - self._rate_n0) / (now - self._rate_t0)
                self._rate_t0 = now
                self._rate_n0 = self.frame_count

    def add_log(self, text: str) -> None:
        with self.lock:
            self.logs.append({"t": time.time(), "msg": text})

    def snapshot(self, n: int = 300) -> dict:
        with self.lock:
            frames = list(self.frames)[-n:]
            return {
                "frames": frames,
                "stats": {
                    "source": self.source_name,
                    "frames": self.frame_count,
                    "rate": round(self.rate, 1),
                    "lost": self.seq_lost,
                    "crc": self.crc_errors + getattr(self, "_crc_extra", 0),
                    "uptime": round(time.time() - self.start_time, 1),
                    "recording": getattr(self, "_recording", False),
                },
                "status": self.status,
                "logs": list(self.logs),
            }


HUB = Hub()


# =====================================================================
#  CSV 记录
# =====================================================================
class CsvRecorder:
    FIELDS = ["t_ms", "roll", "pitch", "yaw",
              "ax_g", "ay_g", "az_g",
              "gx_dps", "gy_dps", "gz_dps",
              "mpu_temp", "adc0", "adc1", "adc2", "chip_temp"]

    def __init__(self):
        self.enabled = False
        self.fh = None
        self.writer = None
        self.path = ""

    def start(self) -> str:
        if self.enabled:
            return self.path
        os.makedirs(LOG_DIR, exist_ok=True)
        name = time.strftime("imu_%Y%m%d_%H%M%S.csv")
        self.path = os.path.join(LOG_DIR, name)
        self.fh = open(self.path, "w", newline="", encoding="utf-8")
        self.writer = csv.writer(self.fh)
        self.writer.writerow(self.FIELDS)
        self.enabled = True
        return self.path

    def stop(self) -> None:
        if self.fh:
            self.fh.close()
        self.fh = None
        self.writer = None
        self.enabled = False

    def write(self, item: dict) -> None:
        if not self.enabled or self.writer is None:
            return
        self.writer.writerow([
            item["t"], item["roll"], item["pitch"], item["yaw"],
            item["ax"], item["ay"], item["az"],
            item["gx"], item["gy"], item["gz"],
            item["temp"], item["adc"][0], item["adc"][1], item["adc"][2],
            item["chip"],
        ])


REC = CsvRecorder()


# =====================================================================
#  数据源 1：串口
# =====================================================================
class SerialSource(threading.Thread):
    def __init__(self, port: str, baud: int = DEFAULT_BAUD):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.ser = None
        self.running = True
        self.parser = FrameParser()
        self._send_lock = threading.Lock()

    def run(self) -> None:
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.2)
        except Exception as e:  # noqa: BLE001
            HUB.add_log(f"[error] 打开串口失败 {self.port}: {e}")
            return
        HUB.add_log(f"[info] 已连接 {self.port} @ {self.baud}")
        while self.running:
            try:
                data = self.ser.read(self.ser.in_waiting or 256)
            except Exception as e:  # noqa: BLE001
                HUB.add_log(f"[error] 串口读取异常: {e}")
                break
            if not data:
                continue
            for ftype, seq, tick, payload in self.parser.feed(data):
                self._handle(ftype, seq, tick, payload)
        if self.ser and self.ser.is_open:
            self.ser.close()

    def _handle(self, ftype, seq, tick, payload) -> None:
        if ftype == FRAME_TYPE_DATA:
            d = parse_data(payload)
            if d:
                HUB.add(d, seq, tick)
                REC.write(HUB.frames[-1] if HUB.frames else {})
        elif ftype == FRAME_TYPE_STATUS:
            s = parse_status(payload)
            if s:
                HUB.status = s
        elif ftype == FRAME_TYPE_LOG:
            try:
                HUB.add_log("[mcu] " + payload.decode("ascii", errors="replace"))
            except Exception:  # noqa: BLE001
                pass

    def send_cmd(self, cmd: bytes) -> bool:
        if self.ser and self.ser.is_open:
            with self._send_lock:
                self.ser.write(cmd)
            return True
        return False

    def stop(self) -> None:
        self.running = False


# =====================================================================
#  数据源 2：模拟器（无硬件演示 / 协议联调）
# =====================================================================
class SimSource(threading.Thread):
    """
    生成一个"有人在缓慢晃动板子"的运动，然后按与固件完全相同的协议组帧，
    再走一遍解析流程 —— 这样验证的是完整的协议栈，而不是直接假造数据。
    """

    def __init__(self, hz: int = 100):
        super().__init__(daemon=True)
        self.hz = hz
        self.running = True
        self.parser = FrameParser()
        self.seq = 0
        self.t0 = time.time()
        self.noise = 0.0

    def run(self) -> None:
        HUB.add_log("[info] 模拟模式：数据由本地生成，未连接真实硬件")
        dt = 1.0 / self.hz
        while self.running:
            t = time.time() - self.t0
            # 姿态角（度）
            roll = 25.0 * math.sin(2 * math.pi * 0.25 * t)
            pitch = 18.0 * math.sin(2 * math.pi * 0.17 * t + 0.8)
            yaw = 40.0 * math.sin(2 * math.pi * 0.08 * t)
            # 角速度 = 角度的微分（°/s）
            gx = 25.0 * 2 * math.pi * 0.25 * math.cos(2 * math.pi * 0.25 * t)
            gy = 18.0 * 2 * math.pi * 0.17 * math.cos(2 * math.pi * 0.17 * t + 0.8)
            gz = 40.0 * 2 * math.pi * 0.08 * math.cos(2 * math.pi * 0.08 * t)
            # 加速度：把重力矢量按姿态旋转到机体系（单位 g）
            r, p = math.radians(roll), math.radians(pitch)
            ax = -math.sin(p) * 1.0
            ay = math.sin(r) * math.cos(p) * 1.0
            az = math.cos(r) * math.cos(p) * 1.0
            # 加一点噪声，更接近真实传感器
            self.noise = random.gauss(0, 0.02)
            ax += self.noise
            ay += random.gauss(0, 0.02)
            az += random.gauss(0, 0.02)

            payload = build_data_frame(
                accel=(int(ax * 4096), int(ay * 4096), int(az * 4096)),
                gyro=(int(gx * 16.4), int(gy * 16.4), int(gz * 16.4)),
                mpu_temp_x10=int((36.5 + 2.0 * math.sin(t / 20.0)) * 10),
                angles_x100=(int(roll * 100), int(pitch * 100), int(yaw * 100)),
                adc=(int(2048 + 1500 * math.sin(2 * math.pi * 0.1 * t)),
                     int(1024 + 800 * math.sin(2 * math.pi * 0.05 * t)),
                     1800),
                chip_temp_x10=int((42.0 + math.sin(t / 30.0)) * 10),
                seq=self.seq,
                tick_ms=int(t * 1000),
            )
            self.seq = (self.seq + 1) & 0xFFFF

            # 故意走一遍解析，验证协议栈
            for ftype, seq, tick, pl in self.parser.feed(payload):
                d = parse_data(pl)
                if d:
                    HUB.add(d, seq, tick)
                    REC.write(HUB.frames[-1] if HUB.frames else {})

            # 每 2 秒模拟一次状态帧
            if int(t) % 2 == 0 and random.random() < 0.02:
                HUB.status = {
                    "uptime_ms": int(t * 1000), "dropped": 0, "overrun": 0,
                    "i2c_err": 0, "cal_valid": 1, "gyro_bias": (12, -7, 5),
                    "accel_bias": (3, -9, 11), "sysclk_hz": 72000000,
                    "upload_on": 1, "mpu_ok": 1,
                }

            time.sleep(dt)

    def send_cmd(self, cmd: bytes) -> bool:
        mapping = {CMD_CALIBRATE: "重新校准", CMD_STATUS: "请求状态",
                   CMD_TOGGLE: "切换上传", CMD_RESET: "复位统计"}
        HUB.add_log(f"[sim] 收到命令 {mapping.get(cmd, repr(cmd))}（模拟模式下仅记录）")
        return True

    def stop(self) -> None:
        self.running = False


# =====================================================================
#  HTTP 服务
# =====================================================================
SOURCE = {"obj": None}


class Handler(BaseHTTPRequestHandler):
    """极简静态 + API 服务"""

    def _send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _json(self, obj) -> None:
        self._send(200, json.dumps(obj).encode("utf-8"), "application/json; charset=utf-8")

    def do_GET(self) -> None:  # noqa: N802
        u = urlparse(self.path)
        if u.path in ("/", "/index.html"):
            path = os.path.join(WEB_DIR, "index.html")
            try:
                with open(path, "rb") as f:
                    self._send(200, f.read(), "text/html; charset=utf-8")
            except FileNotFoundError:
                self._send(404, b"index.html not found", "text/plain")
            return
        if u.path == "/api/data":
            q = parse_qs(u.query)
            n = int(q.get("n", ["300"])[0])
            snap = HUB.snapshot(n)
            snap["stats"]["recording"] = REC.enabled
            self._json(snap)
            return
        if u.path == "/api/ports":
            ports = []
            if HAVE_SERIAL:
                ports = [{"device": p.device, "desc": p.description}
                         for p in serial.tools.list_ports.comports()]
            self._json({"ports": ports})
            return
        self._send(404, b"not found", "text/plain")

    def do_POST(self) -> None:  # noqa: N802
        u = urlparse(self.path)
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            body = json.loads(raw.decode("utf-8"))
        except Exception:  # noqa: BLE001
            body = {}

        if u.path == "/api/command":
            cmd = body.get("cmd", "")
            mapping = {"calibrate": CMD_CALIBRATE, "status": CMD_STATUS,
                       "toggle": CMD_TOGGLE, "reset": CMD_RESET}
            b = mapping.get(cmd)
            ok = False
            if b and SOURCE["obj"]:
                ok = SOURCE["obj"].send_cmd(b)
            self._json({"ok": ok})

        elif u.path == "/api/csv":
            enable = bool(body.get("enable"))
            if enable:
                path = REC.start()
                self._json({"ok": True, "recording": True, "path": path})
            else:
                REC.stop()
                self._json({"ok": True, "recording": False})
        else:
            self._send(404, b"not found", "text/plain")

    def log_message(self, fmt, *args) -> None:
        pass   # 关闭默认访问日志，保持控制台干净


# =====================================================================
#  入口
# =====================================================================
def list_ports() -> None:
    if not HAVE_SERIAL:
        print("未安装 pyserial，无法枚举串口。执行: pip install pyserial")
        return
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("未发现可用串口（板子插上了吗？驱动装了吗？）")
        return
    for p in ports:
        print(f"  {p.device:<16} {p.description}")


def main() -> int:
    ap = argparse.ArgumentParser(description="STM32 IMU 上位机")
    ap.add_argument("--port", help="串口号，如 COM3 或 /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    ap.add_argument("--sim", action="store_true", help="强制使用模拟数据源")
    ap.add_argument("--list", action="store_true", help="列出可用串口并退出")
    ap.add_argument("--http", type=int, default=DEFAULT_HTTP_PORT, help="HTTP 服务端口")
    ap.add_argument("--csv", action="store_true", help="启动即开始 CSV 记录")
    args = ap.parse_args()

    if args.list:
        list_ports()
        return 0

    if args.csv:
        print("CSV 记录已开启 ->", REC.start())

    # 选择数据源
    src = None
    if not args.sim:
        if not HAVE_SERIAL:
            print("[warn] 未安装 pyserial，自动进入模拟模式（pip install pyserial 后可连真机）")
        else:
            port = args.port
            if not port:
                ports = list(serial.tools.list_ports.comports())
                if ports:
                    port = ports[0].device
                    print(f"[info] 自动选择第一个串口: {port}")
                else:
                    print("[warn] 未发现串口设备，进入模拟模式")
            if port:
                src = SerialSource(port, args.baud)
                HUB.source_name = f"serial {port}"
    if src is None:
        src = SimSource(hz=100)
        HUB.source_name = "simulator"

    SOURCE["obj"] = src
    src.start()

    srv = ThreadingHTTPServer(("0.0.0.0", args.http), Handler)
    print("=" * 60)
    print(f"  数据源 : {HUB.source_name}")
    print(f"  网页端 : http://127.0.0.1:{args.http}")
    print(f"  CSV    : {REC.path if REC.enabled else '未开启（网页上可一键开启）'}")
    print("  Ctrl+C 退出")
    print("=" * 60)

    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n退出中...")
    finally:
        src.stop()
        if REC.enabled:
            REC.stop()
        srv.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
