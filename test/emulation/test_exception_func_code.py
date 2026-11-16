# -*- coding: utf-8 -*-
"""异常应答的功能码按原 BIN 用「加法」生成（func+0x80 按 8 位回绕）。

原 BIN 0xB42C / 0xB4C2 是 `adds r0, #0x80`，不是按位或：
功能码 >= 0x80 时（0x83/0x86/0xFF 等）会回绕成 0x03/0x06/0x7F。
本用例逐帧比较原 BIN 与新 ELF 的实际应答字节，覆盖非法功能码
（异常码 0x01）与 CRC 错误（异常码 0x04）两条路径。
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/verification'))
import verify_firmware_equivalence as V
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def send(body, bad_crc=False):
    """返回原 BIN 与新 ELF 的应答字节。"""
    wire = bytes(body) + struct.pack('<H', crc16(bytes(body)) ^ (0xFFFF if bad_crc else 0))
    out = []
    for new in (False, True):
        uc = V.machine(new)
        uc.mem_write(0x100016FF, b'\x01')
        uc.mem_write(0x10001790, b'\x05')
        uc.mem_write(0x10001792, bytes((len(wire),)))
        uc.mem_write(0x100022A4, wire)
        sent = []
        tx = V.SYMS['uart3_tx_byte'] if new else 0xAE0C

        def hook(m, address, size, user):
            if address == tx:
                sent.append(bytes(m.mem_read(0x1000236C, m.reg_read(UC_ARM_REG_R0))))
                m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

        uc.hook_add(UC_HOOK_CODE, hook)
        V.run(uc, V.SYMS['modbus_dispatch'] if new else 0xB642, max_insn=200000)
        assert uc.reg_read(UC_ARM_REG_PC) == 0x3FF00, '通信路径未正常返回'
        out.append(sent[0] if sent else b'')
    return out


def main():
    checked = 0
    # 非法功能码：原 BIN 回 [addr, func+0x80, 0x01]
    for func in (0x00, 0x04, 0x05, 0x2B, 0x7F, 0x80, 0x83, 0x86, 0x90, 0xFF):
        body = (0x01, func, 0x10, 0x01, 0x00, 0x01)
        old, new = send(body)
        assert old == new, f'非法功能码 0x{func:02X}: OLD={old.hex()} NEW={new.hex()}'
        low = (func + 0x80) & 0xFF
        assert old == bytes((0x01, low, 0x01)) + old[3:], \
            f'非法功能码 0x{func:02X} 应答功能码不符: {old.hex()}'
        checked += 1

    # CRC 错误：原 BIN 回 [addr, func+0x80, 0x04]
    # 注意：功能码过滤在 CRC 校验之前，func>=0x80 会先命中上面的 0x01 分支，
    # 因此这里只用 0x03/0x06/0x10 触发 0x04（CRC 错误）路径。
    for func in (0x03, 0x06, 0x10):
        body = (0x01, func, 0x10, 0x01, 0x00, 0x01)
        old, new = send(body, bad_crc=True)
        assert old == new, f'CRC 异常 0x{func:02X}: OLD={old.hex()} NEW={new.hex()}'
        low = (func + 0x80) & 0xFF
        assert old[1] == low and old[2] == 0x04, \
            f'CRC 异常 0x{func:02X} 应答不符: {old.hex()}'
        checked += 1

    print(f'异常应答功能码 func+0x80：{checked}/{checked} 帧与原 BIN 一致')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
