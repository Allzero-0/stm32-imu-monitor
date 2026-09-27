"""
protocol.py —— 与下位机 (firmware/src/protocol.c) 严格对应的串口帧协议实现

帧格式（小端序）：
    ┌──────┬──────┬──────┬──────┬────────┬──────────┬─────────┬──────┐
    │ 0xAA │ 0x55 │ TYPE │ LEN  │ SEQ(2) │ TICK(4)  │ PAYLOAD │ CRC8 │
    └──────┴──────┴──────┴──────┴────────┴──────────┴─────────┴──────┘

CRC8 多项式 0x07，初值 0x00，覆盖从 TYPE 到 PAYLOAD 结束。

这个模块只依赖 Python 标准库，可以单独跑自检：
    python protocol.py --selftest
"""
import struct
from typing import Iterator, Optional, Tuple, List

# ----------------------------------------------------------------- 常量
FRAME_HEAD0 = 0xAA
FRAME_HEAD1 = 0x55

FRAME_TYPE_DATA = 0x01
FRAME_TYPE_STATUS = 0x02
FRAME_TYPE_LOG = 0x03

FRAME_HDR_SIZE = 10      # 帧头2 + TYPE1 + LEN1 + SEQ2 + TICK4
FRAME_CRC_SIZE = 1
FRAME_MAX_PAYLOAD = 64

FRAME_DATA_PAYLOAD = 28

# 命令字
CMD_CALIBRATE = b'C'
CMD_STATUS = b'V'
CMD_TOGGLE = b'S'
CMD_RESET = b'R'

# 量程（与固件 MPU6050_Init 配置保持一致）
ACCEL_SENS_8G = 4096.0       # LSB/g   @ ±8g
GYRO_SENS_2000 = 16.4        # LSB/(°/s) @ ±2000°/s


# ----------------------------------------------------------------- CRC8
def crc8(data: bytes) -> int:
    """CRC8，多项式 0x07（x^8+x^2+x+1），初值 0x00"""
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


# ------------------------------------------------------------- 组帧（编码）
def build_frame(ftype: int, payload: bytes, seq: int, tick_ms: int) -> bytes:
    """按协议打包一帧"""
    if len(payload) > FRAME_MAX_PAYLOAD:
        raise ValueError(f"payload too long: {len(payload)}")
    head = struct.pack(
        "<BBBBHI",
        FRAME_HEAD0,
        FRAME_HEAD1,
        ftype & 0xFF,
        len(payload) & 0xFF,
        seq & 0xFFFF,
        tick_ms & 0xFFFFFFFF,
    )
    body = head[2:] + payload          # CRC 从 TYPE 开始算
    return head + payload + bytes([crc8(body)])


def build_data_frame(accel, gyro, mpu_temp_x10, angles_x100, adc, chip_temp_x10,
                     seq: int = 0, tick_ms: int = 0) -> bytes:
    """打包数据帧（供模拟器和单元测试使用）"""
    payload = struct.pack(
        "<6h h 3h 3H h",
        *accel,
        *gyro,
        mpu_temp_x10,
        *angles_x100,
        *adc,
        chip_temp_x10,
    )
    assert len(payload) == FRAME_DATA_PAYLOAD, len(payload)
    return build_frame(FRAME_TYPE_DATA, payload, seq, tick_ms)


def build_status_frame(uptime_ms, dropped, overrun, i2c_err, cal_valid,
                       gyro_bias, accel_bias, sysclk, upload_on, mpu_ok,
                       seq: int = 0, tick_ms: int = 0) -> bytes:
    """打包状态帧"""
    payload = struct.pack("<IHHHB3h3hIBB",
                          uptime_ms, dropped, overrun, i2c_err, cal_valid,
                          *gyro_bias, *accel_bias, sysclk, upload_on, mpu_ok)
    return build_frame(FRAME_TYPE_STATUS, payload, seq, tick_ms)


# ------------------------------------------------------------- 解帧（解码）
class FrameParser:
    """
    流式帧解析器：持续喂入串口收到的字节，吐出完整且校验通过的帧。

    设计要点：
      - 逐字节同步帧头，即使中途接入（收到半个帧）也能自动对齐
      - CRC 校验失败直接丢弃，并统计 crc_errors
      - 用 bytearray 缓冲，避免大量小对象分配
    """

    def __init__(self, max_payload: int = FRAME_MAX_PAYLOAD):
        self._buf = bytearray()
        self.max_payload = max_payload
        self.crc_errors = 0
        self.frames_ok = 0
        self.bytes_dropped = 0

    def feed(self, data: bytes) -> Iterator[Tuple[int, int, int, bytes]]:
        """喂入字节，yield (type, seq, tick_ms, payload)"""
        self._buf.extend(data)
        buf = self._buf

        while True:
            # 1. 找帧头
            start = -1
            for i in range(len(buf) - 1):
                if buf[i] == FRAME_HEAD0 and buf[i + 1] == FRAME_HEAD1:
                    start = i
                    break
            if start < 0:
                # 没有帧头：只保留最后一个字节（可能是 0xAA）
                if len(buf) > 1:
                    self.bytes_dropped += len(buf) - 1
                    del buf[:-1]
                return
            if start > 0:
                self.bytes_dropped += start
                del buf[:start]

            # 2. 帧头之后至少要有 TYPE + LEN 才能知道长度
            if len(buf) < 4:
                return
            ftype = buf[2]
            plen = buf[3]
            if plen > self.max_payload:
                # 长度非法，说明是假帧头，丢掉重新开始
                self.bytes_dropped += 2
                del buf[:2]
                continue

            total = FRAME_HDR_SIZE + plen + FRAME_CRC_SIZE
            if len(buf) < total:
                return   # 数据还没收全，等下次

            frame = bytes(buf[:total])
            del buf[:total]

            # 3. CRC 校验：覆盖 TYPE..PAYLOAD，长度 = 8 + plen
            if crc8(frame[2:FRAME_HDR_SIZE + plen]) != frame[total - 1]:
                self.crc_errors += 1
                continue

            seq, tick = struct.unpack("<HI", frame[4:10])
            self.frames_ok += 1
            yield (ftype, seq, tick, frame[10:10 + plen])

    def reset(self) -> None:
        self._buf.clear()


def parse_data(payload: bytes) -> Optional[dict]:
    """解析数据帧 payload -> 物理量字典"""
    if len(payload) < FRAME_DATA_PAYLOAD:
        return None
    (ax, ay, az, gx, gy, gz, mpu_temp, roll, pitch, yaw,
     adc0, adc1, adc2, chip_temp) = struct.unpack("<6hh3h3Hh", payload[:FRAME_DATA_PAYLOAD])

    return {
        "accel_raw": (ax, ay, az),
        "accel_g": (ax / ACCEL_SENS_8G, ay / ACCEL_SENS_8G, az / ACCEL_SENS_8G),
        "gyro_raw": (gx, gy, gz),
        "gyro_dps": (gx / GYRO_SENS_2000, gy / GYRO_SENS_2000, gz / GYRO_SENS_2000),
        "mpu_temp": mpu_temp / 10.0,
        "roll": roll / 100.0,
        "pitch": pitch / 100.0,
        "yaw": yaw / 100.0,
        "adc": (adc0, adc1, adc2),
        "adc_mv": (adc0 * 3300 / 4095, adc1 * 3300 / 4095, adc2 * 3300 / 4095),
        "chip_temp": chip_temp / 10.0,
    }


def parse_status(payload: bytes) -> Optional[dict]:
    """解析状态帧 payload"""
    if len(payload) < 29:
        return None
    (uptime, dropped, overrun, i2c_err, cal_valid,
     bgx, bgy, bgz, bax, bay, baz, sysclk, upload_on, mpu_ok) = \
        struct.unpack("<IHHHB3h3hIBB", payload[:29])
    return {
        "uptime_ms": uptime,
        "dropped": dropped,
        "overrun": overrun,
        "i2c_err": i2c_err,
        "cal_valid": cal_valid,
        "gyro_bias": (bgx, bgy, bgz),
        "accel_bias": (bax, bay, baz),
        "sysclk_hz": sysclk,
        "upload_on": upload_on,
        "mpu_ok": mpu_ok,
    }


# ------------------------------------------------------------------ 自检
def _selftest() -> None:
    print("=== protocol.py 自检 ===")

    # 1. CRC8 已知向量检查：CRC8/ATM("123456789") = 0xF4
    assert crc8(b"123456789") == 0xF4, hex(crc8(b"123456789"))
    print("  [OK] CRC8 标准向量 0xF4")

    # 2. 组帧 -> 解析 往返
    accel = (100, -200, 4096)
    gyro = (16, -33, 82)
    angles = (1234, -567, 8901)
    adc = (2048, 1024, 1800)
    frame = build_data_frame(accel, gyro, 315, angles, adc, 267, seq=7, tick_ms=12345)

    p = FrameParser()
    got: List[tuple] = list(p.feed(frame))
    assert len(got) == 1, f"期望解析出 1 帧，实际 {len(got)}"
    ftype, seq, tick, payload = got[0]
    assert ftype == FRAME_TYPE_DATA
    assert seq == 7 and tick == 12345
    print("  [OK] 数据帧往返解析")

    d = parse_data(payload)
    assert d["accel_raw"] == accel
    assert d["gyro_raw"] == gyro
    assert d["mpu_temp"] == 31.5
    assert d["roll"] == 12.34
    assert abs(d["chip_temp"] - 26.7) < 0.01
    print(f"  [OK] 物理量换算: roll={d['roll']}° mpu={d['mpu_temp']}°C chip={d['chip_temp']}°C")

    # 3. 垃圾数据 + 断帧 + 粘包
    p2 = FrameParser()
    stream = b"\x00\xFF\xAA" + frame[:15]      # 半个帧
    assert list(p2.feed(stream)) == []
    rest = list(p2.feed(frame[15:] + frame))   # 补齐 + 再来一整帧
    assert len(rest) == 2, len(rest)
    print("  [OK] 断帧续接 + 粘包处理")

    # 4. CRC 错误统计
    bad = bytearray(frame)
    bad[-1] ^= 0xFF
    p3 = FrameParser()
    assert list(p3.feed(bytes(bad))) == []
    assert p3.crc_errors == 1
    print("  [OK] CRC 错误检测")

    # 5. 状态帧
    st = build_status_frame(1000, 2, 3, 4, 1, (5, 6, 7), (8, 9, 10), 72000000, 1, 1)
    p4 = FrameParser()
    ftype, _, _, pl = list(p4.feed(st))[0]
    s = parse_status(pl)
    assert ftype == FRAME_TYPE_STATUS and s["sysclk_hz"] == 72000000
    print("  [OK] 状态帧解析")

    print("=== 全部自检通过 ===")


if __name__ == "__main__":
    import sys
    if "--selftest" in sys.argv:
        _selftest()
    else:
        print(__doc__)
        print("用法: python protocol.py --selftest")
