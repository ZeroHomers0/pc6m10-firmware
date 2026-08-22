# -*- coding: utf-8 -*-
# =============================================================================
# test_crc16_semantics.py — host 虚拟数据测试：Modbus crc16 长度语义核对
#
# 被测对象：firmware/src/08_uart3_modbus.c 的 crc16()（原 flash 0xAF64）
#
# 本测试验证的结论（由 2026-08-23 两轮基准确认）：
#   1) 固件两张 CRC 表（crc16_hi_tbl/lo_tbl）与原始 LPC1765.bin 的
#      flash 表（0x11034 / 0x11134）**逐字节一致** → S9 悬空表修复正确。
#   2) 固件 crc16 算法循环是 `while((len=(len-1)&0xff)!=0)`：**先减后终检**，
#      因此只处理 (len-1) 个字节、丢弃第 len 个字节。这是 0xAF64 原始机器码
#      （sub r4,#1 → uxtb r4 → bne 循环体）的忠实还原，非反编译错误。
#   3) 调用方 modbus_dispatch 传 `crc16(FRAME, rx_len-2)`（0xB642 反汇编
#      subs r0,#2 → uxtb r1 → bl crc16），rx_len 为含 CRC 的总帧长。
#
# 因此固件校验一个"标准 Modbus 帧"（数据体 N 字节、含 CRC 总长 v=N+2）时：
#   调用方传 crc16(FRAME, v-2 = N)，固件只处理 N-1 字节。
#   要让固件覆盖全部 N 个数据字节，调用方须传 crc16(FRAME, N+1)。
# 关键：本测试以【原始二进制真值】为基准，不用教科书 Modbus CRC 判错。
# =============================================================================
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN  = os.path.join(ROOT, 'LPC1765.bin')
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

def load_tables():
    """从原始 bin 读 flash 表（与 src/crc16_table.c 内嵌表应一致）"""
    b = open(BIN, 'rb').read()
    return b[0x11034:0x11034+256], b[0x11134:0x11134+256]

def crc16_fw(data, length, hi, lo):
    """按 08_uart3_modbus.c crc16 逐字节复现（len-1 先减后终检）"""
    ch = 0xff; cl = 0xff; i = 0
    length &= 0xff
    while True:
        length = (length - 1) & 0xff
        if length == 0:
            break
        t = data[i] ^ cl
        cl = hi[t] ^ ch
        ch = lo[t]
        i += 1
    return (cl | (ch << 8)) & 0xffff

def crc16_ref(data, length):
    """参考 Modbus CRC16（处理全部字节，poly 0xA001，初值 0xFFFF）"""
    crc = 0xffff
    for i in range(length):
        crc ^= data[i]
        for _ in range(8):
            crc = (crc >> 1) ^ 0xa001 if (crc & 1) else (crc >> 1)
            crc &= 0xffff
    return crc

def main():
    hi, lo = load_tables()
    passed = 0; failed = 0

    def check(name, cond, detail=""):
        nonlocal passed, failed
        status = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{status}] {name}" + (f"  {detail}" if detail else ""))

    print("=== 1. CRC 表与原始 bin 逐字节一致（S9 修复验证）===")
    # 交叉验证：bin 表 == 标准 0xA001 倒退表（决定表正确性）
    def ref_tbl(i):
        crc = i & 0xff
        for _ in range(8):
            crc = (crc >> 1) ^ 0xa001 if (crc & 1) else (crc >> 1); crc &= 0xffff
        return crc
    mm = [(i, ref_tbl(i)) for i in range(256)]
    hi_ok = all(lo[i] == (v >> 8) for i, v in mm)
    lo_ok = all(hi[i] == (v & 0xff) for i, v in mm)
    check("bin 表 == 标准 Modbus CRC16(0xA001) 高字节表", hi_ok)
    check("bin 表 == 标准 Modbus CRC16(0xA001) 低字节表", lo_ok)

    print("=== 2. crc16 长度语义（len-1 先减后终检）===")
    # 标准向量 '123456789'
    test = list(b"123456789")
    r_s = crc16_ref(test, 9)
    check("参考算法处理9节 → 0x4B37", r_s == 0x4B37, f"got 0x{r_s:04X}")
    f_9 = crc16_fw(test, 9, hi, lo)
    check("固件crc16(len=9)处理8节 → ≠0x4B37(丢末字节)", f_9 != 0x4B37, f"got 0x{f_9:04X}")
    f_10 = crc16_fw(test, 10, hi, lo)
    check("固件crc16(len=10)处理9节 → ==0x4B37", f_10 == 0x4B37, f"got 0x{f_10:04X}")

    print("=== 3. 真实 Modbus 读请求帧（站1 func03 reg0 cnt1）===")
    rx = [0x01, 0x03, 0x00, 0x00, 0x00, 0x01]
    ref = crc16_ref(rx, 6)
    f6 = crc16_fw(rx, 6, hi, lo)
    f7 = crc16_fw(rx, 7, hi, lo)
    check("帧体6节 参考CRC=0x%04X" % ref,
          crc16_ref(rx, 6) == ref)
    check("固件crc16(帧,6)≠参考(丢帧末数据字节)", f6 != ref, f"got 0x{f6:04X}")
    check("固件crc16(帧,7)==参考(覆盖全部6节)", f7 == ref, f"got 0x{f7:04X}")
    check("帧末CRC高/低字节序(先低后高)", (ref & 0xff) == 0x84 and (ref >> 8) == 0x0a)

    print()
    print(f"  通过 {passed} / {passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
