#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

import serial


PID_OTA = 0x55AA
SYNC_HOST_TX = 0xBA

ACK_CONTINUE = 0x00
ACK_SUCCESS = 0x01
ACK_FAIL = 0x02


@dataclass
class ParsedFrame:
    sync: int
    protocol_id: int
    payload: bytes


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    poly = 0x1021
    for byte in data:
        for i in range(8):
            bit = (byte >> (7 - i)) & 1
            c15 = (crc >> 15) & 1
            crc = ((crc << 1) & 0xFFFF)
            if c15 ^ bit:
                crc ^= poly
    return crc & 0xFFFF

## 创建protocol_id 进行处理固件传输机
def build_frame(protocol_id: int, payload: bytes, sync: int = SYNC_HOST_TX) -> bytes:
    header = bytearray()
    header.append(0xFE)
    header.append(sync & 0xFF)
    header.extend(protocol_id.to_bytes(2, "big"))
    header.extend(len(payload).to_bytes(2, "big"))
    crc_input = bytes(header[2:]) + payload
    crc = crc16_ccitt_false(crc_input)
    return bytes(header) + payload + crc.to_bytes(2, "little") + b"\x0A\x0D"


def parse_one_frame(buf: bytearray):
    while len(buf) >= 2 and buf[0] != 0xFE:
        del buf[0]

    if len(buf) < 6 or buf[0] != 0xFE:
        return None

    payload_len = int.from_bytes(buf[4:6], "big")
    total_len = 6 + payload_len + 4
    if len(buf) < total_len:
        return None

    frame = bytes(buf[:total_len])
    del buf[:total_len]

    if frame[-2:] != b"\x0A\x0D":
        return "invalid"

    protocol_id = int.from_bytes(frame[2:4], "big")
    payload = frame[6:6 + payload_len]
    got_crc = int.from_bytes(frame[6 + payload_len:8 + payload_len], "little")
    calc_crc = crc16_ccitt_false(frame[2:6 + payload_len])
    if got_crc != calc_crc:
        return "invalid"

    return ParsedFrame(sync=frame[1], protocol_id=protocol_id, payload=payload)


def recv_frame(ser: serial.Serial, timeout_s: float) -> ParsedFrame:
    end = time.time() + timeout_s
    rx = bytearray()
    while time.time() < end:
        chunk = ser.read(256)
        if chunk:
            rx.extend(chunk)
            while True:
                ret = parse_one_frame(rx)
                if ret is None:
                    break
                if ret == "invalid":
                    continue
                return ret
        else:
            time.sleep(0.002)
    raise TimeoutError("接收帧超时")


def wait_ota_status(ser: serial.Serial, timeout_s: float, need_final: bool) -> int:
    end = time.time() + timeout_s
    while time.time() < end:
        frame = recv_frame(ser, max(0.05, end - time.time()))
        if frame.protocol_id != PID_OTA or not frame.payload:
            continue
        st = frame.payload[0]
        if st == ACK_FAIL:
            return ACK_FAIL
        if st == ACK_SUCCESS:
            return ACK_SUCCESS
        if st == ACK_CONTINUE:
            if not need_final:
                return ACK_CONTINUE
            continue
    raise TimeoutError("等待 OTA 状态超时")


def calc_file_crc(fw: bytes, mode: str) -> int:
    if mode == "crc16":
        return crc16_ccitt_false(fw) & 0xFFFF
    return 0


def send_ota(
    ser: serial.Serial,
    fw: bytes,
    file_crc_u32: int,
    chunk_size: int,
    pkt_len: int,
    timeout_s: float,
    progress_cb: Optional[Callable[[int, int], None]] = None,
    log_cb: Optional[Callable[[str], None]] = None,
):
    def _log(msg: str):
        if log_cb:
            log_cb(msg)
        else:
            print(msg)

    file_size = len(fw)
    start_payload = b"\x55\xAA" + file_size.to_bytes(4, "little") + file_crc_u32.to_bytes(4, "little")
    ser.write(build_frame(PID_OTA, start_payload))

    st = wait_ota_status(ser, timeout_s, need_final=False)
    if st != ACK_CONTINUE:
        raise RuntimeError(f"START 后状态异常: 0x{st:02X}")

    if progress_cb:
        progress_cb(0, file_size)
    _log(f"[START] size={file_size} file_crc=0x{file_crc_u32:08X}")

    offset = 0
    while offset < file_size:
        group_len = min(pkt_len, file_size - offset)
        group_end = offset + group_len
        cur = offset
        while cur < group_end:
            n = min(chunk_size, group_end - cur)
            ser.write(build_frame(PID_OTA, fw[cur:cur + n]))
            cur += n

        offset = group_end
        _log(f"[DATA] {offset}/{file_size} ({offset * 100.0 / file_size:.1f}%)")
        if progress_cb:
            progress_cb(offset, file_size)

        if offset < file_size:
            st = wait_ota_status(ser, timeout_s, need_final=False)
            if st != ACK_CONTINUE:
                raise RuntimeError(f"数据组完成后状态异常: 0x{st:02X}")

    if file_size % pkt_len == 0:
        st = wait_ota_status(ser, timeout_s, need_final=False)
        if st != ACK_CONTINUE:
            raise RuntimeError(f"末组对齐后状态异常: 0x{st:02X}")

    ser.write(build_frame(PID_OTA, b"\xAA\x55"))
    st = wait_ota_status(ser, timeout_s, need_final=True)
    if st != ACK_SUCCESS:
        raise RuntimeError(f"END 后未成功，状态: 0x{st:02X}")

    if progress_cb:
        progress_cb(file_size, file_size)
    _log("[DONE] OTA success")


def run_ota(
    port: str,
    baud: int,
    file_path: str,
    chunk: int = 256,
    pkt_len: int = 2048,
    timeout: float = 8.0,
    file_crc_mode: str = "zero",
    progress_cb: Optional[Callable[[int, int], None]] = None,
    log_cb: Optional[Callable[[str], None]] = None,
):
    fw_path = Path(file_path)
    if not fw_path.is_file():
        raise FileNotFoundError(f"文件不存在: {fw_path}")

    if chunk <= 0 or chunk > 512:
        raise ValueError("chunk 必须在 1..512 之间")
    if pkt_len <= 0:
        raise ValueError("pkt_len 必须大于 0")

    fw = fw_path.read_bytes()
    if not fw:
        raise ValueError("升级文件为空")

    file_crc = calc_file_crc(fw, file_crc_mode)
    if log_cb:
        log_cb(f"[INFO] file={fw_path}")
        log_cb(f"[INFO] size={len(fw)} bytes, file_crc=0x{file_crc:08X}, chunk={chunk}, pkt_len={pkt_len}")

    with serial.Serial(port, baud, timeout=0.05) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        send_ota(
            ser=ser,
            fw=fw,
            file_crc_u32=file_crc,
            chunk_size=chunk,
            pkt_len=pkt_len,
            timeout_s=timeout,
            progress_cb=progress_cb,
            log_cb=log_cb,
        )


def main():
    parser = argparse.ArgumentParser(description="JL701 串口 OTA 上位机")
    parser.add_argument("--port", required=True, help="串口号，例如 COM6")
    parser.add_argument("--baud", type=int, default=1000000, help="波特率，默认 1000000")
    parser.add_argument("--file", required=True, help="升级文件路径")
    parser.add_argument("--chunk", type=int, default=256, help="单次 DATA payload 字节数")
    parser.add_argument("--pkt-len", type=int, default=2048, help="设备聚包写入长度")
    parser.add_argument("--timeout", type=float, default=8.0, help="ACK 超时秒数")
    parser.add_argument("--file-crc-mode", choices=["zero", "crc16"], default="zero", help="START 中 file_crc 模式")
    args = parser.parse_args()

    try:
        run_ota(
            port=args.port,
            baud=args.baud,
            file_path=args.file,
            chunk=args.chunk,
            pkt_len=args.pkt_len,
            timeout=args.timeout,
            file_crc_mode=args.file_crc_mode,
        )
        print("[OK] OTA 完成，设备应进入重启流程")
        return 0
    except Exception as e:
        print(f"[ERR] OTA 失败: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
