# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_modbus_dispatch.py — 真实执行 modbus_dispatch，验证 Modbus 帧处理
#
# 用 Unicorn 执行编译的 modbus_dispatch@0x86E4。把标准 Modbus RTU 请求帧放进
# FRAME(0x100022A4)、设 RX_LEN/RX_STATE/SLAVE_ADDR，验证：
#   * 合法帧（CRC 正确，且 frame[2]==0x10 的 0x10xx 寄存器布局）→ 走 0x03 读分支
#     → uart3_tx_byte(5+cnt*2=7) 发 7 字节响应，TXBUF=[addr,0x03,字节数,数据,CRC]
#   * CRC 错帧 → 异常分支 uart3_tx_byte(5) 发异常响应（功能码置 0x80)
# 为规避 UART 外设硬时序，hook uart3_tx_byte@0x904c：它参数 r0 = "发 n 字节"的计数，
# 实际字节在 TXBUF。hook 跳过函数体（PC=LR），只记录 r0 即得"本次要发的字节数"。
#
# 寄存器寻址约定（本固件特有）：帧体为 [addr,func,0x10,reg_lo,count_hi,count_lo,CRC]，
#   frame[2]==0x10 是 reg 高字节、frame[3]=reg 低字节 → 即 Modbus 寄存区 0x1001..0x103F。
# 注意：请求/响应 CRC 均按固件 crc16 的 len-1 先减后终检语义计算（见 README「关键发现」），
#   不是教科书 Modbus CRC。基准是原始二进制真值，非教材。
# 若 unicorn 不可用 → SKIP。
# =============================================================================
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass
from unicorn import *
from unicorn.arm_const import *

FRAME, RX_LEN, RX_STATE, SLAVE_ADDR, TXBUF = 0x100022A4, 0x10001792, 0x10001790, 0x100016FF, 0x1000236C

# ── 固件 crc16（len-1 先减后终检）模型，仅用于断言自洽，非教材 ──
def _load_tables():
    b = open(os.path.join(ROOT, 'LPC1765.bin'), 'rb').read()
    return b[0x11034:0x11034+256], b[0x11134:0x11134+256]

def crc16_fw(data, length):
    hi, lo = _load_tables()
    ch = 0xff; cl = 0xff; i = 0
    length &= 0xff
    while True:
        length = (length - 1) & 0xff
        if length == 0:
            break
        t = data[i] ^ cl
        cl = hi[t] ^ ch
        ch = lo[t]; i += 1
    return (cl | (ch << 8)) & 0xffff

def load():
    from unicorn_harness import load_firmware, lookup
    return load_firmware(), lookup

def main():
    try:
        import unicorn  # noqa
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    e, lookup = load()
    FUNC_dispatch = lookup('modbus_dispatch')
    FUNC_uart3_tx = lookup('uart3_tx_byte')
    # 映射外设区为可读写 0（避免 UNMAPPED）；读/正常分支不触碰 FIO/UART，仅兜底/不匹配才碰
    for base, ln in [(0x2009C000, 0x4000), (0x40090000, 0x4000), (0x4009C000, 0x1000),
                     (0x4002C000, 0x2000), (0x400FC000, 0x1000), (0xE000E100, 0x2000)]:
        try:
            e.mem_map(base, ln, UC_PROT_ALL)
        except UcError:
            pass

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    tx_count = []
    def hook_tx(uc, addr, size, user):
        # uart3_tx_byte(n)：r0 = 本帧要发的字节数 n；跳过函数体（PC=LR）
        # 实际字节已写入 TXBUF；仅记录 n 即可判定走哪条分支（读=7 / 异常=5）。
        lr = uc.reg_read(UC_ARM_REG_LR)
        tx_count.append(uc.reg_read(UC_ARM_REG_R0) & 0xff)
        uc.reg_write(UC_ARM_REG_PC, lr)
    e.hook_add(UC_HOOK_CODE, hook_tx, begin=FUNC_uart3_tx, end=FUNC_uart3_tx+1)

    def w8(a, v): e.mem_write(a, bytes([v]))
    def r8(a): return e.mem_read(a, 1)[0]

    def send_frame(frame, slave=0x01):
        tx_count.clear()
        for i, b in enumerate(frame):
            e.mem_write(FRAME+i, bytes([b]))
        w8(RX_LEN, len(frame))
        w8(RX_STATE, 5)
        w8(SLAVE_ADDR, slave)
        from unicorn_harness import call
        call(e, FUNC_dispatch)

    # ── 场景1：合法读请求 站1 读 0x1001(#1) 数量1 ──
    #   本固件地址/计数编码特例：帧体 = [addr,func,reg_hi=0x10,reg_lo, cnt_lo, cnt_hi]
    #   reg=frame[3]=0x01(0x1001)；cnt=frame[4]|frame[5]<<8 小端 → frame[4]=0x01,frame[5]=0x00 → cnt=1
    #   crc16(FRAME,v-2=6) 只处理5字节 [01 03 10 01 01] = 0x11D8
    req = bytes([0x01, 0x03, 0x10, 0x01, 0x01, 0x00])
    req_crc = crc16_fw(req, 6)
    frame = req + bytes([req_crc & 0xff, req_crc >> 8])
    # reg 索引 0 → modbus_read_reg 返回 *g_gain_sel（0x10001634）；先写入已知值以便断言
    G_GAIN_SEL = 0x10001634
    w8(G_GAIN_SEL, 0x56)
    send_frame(frame)

    check("读请求帧 CRC 自洽（0x%04X）" % req_crc,
          req_crc == 0x11D8, f"0x{req_crc:04X}")
    check("读分支调用 uart3_tx_byte，发送 7 字节", tx_count == [7],
          f"tx_count={[hex(x) for x in tx_count]}")
    # 响应 TXBUF[0..6] = [addr,0x03,字节数,数据hi,数据lo,crcl,crch]
    txb = [r8(TXBUF+i) for i in range(7)]
    check("响应[0]=地址 0x01", txb[0] == 0x01, f"{hex(txb[0])}")
    check("响应[1]=功能码 0x03", txb[1] == 0x03, f"{hex(txb[1])}")
    check("响应[2]=字节数 0x02（1字=2节）", txb[2] == 0x02, f"{hex(txb[2])}")
    check("响应[3..4]=数据字（大端 0x0056=g_gain_sel）", (txb[3] << 8 | txb[4]) == 0x0056,
          f"数据=0x{txb[3]:02X}{txb[4]:02X}")
    # 响应 CRC：固件 crc16(tx, 3+cnt*2=5) 处理4字节 tx[0..3] → 应为 tx[5]tx[6]
    if len(tx_count) == 1 and tx_count[0] == 7:
        rsp_crc = crc16_fw(txb, 5)
        check("响应 CRC 自洽（固件语义）",
              (rsp_crc & 0xff) == txb[5] and (rsp_crc >> 8) == txb[6],
              f"算0x{rsp_crc:04X} tx[5]={hex(txb[5])} tx[6]={hex(txb[6])}")

    # ── 场景2：合法请求但 CRC 错（用标准 Modbus CRC 糊弄）→ 应走 CRC 异常分支 ──
    def crc16_std(data):
        c = 0xFFFF
        for byte in data:
            c ^= byte
            for _ in range(8):
                if c & 1: c = (c >> 1) ^ 0xA001
                else: c >>= 1
        return c
    bad = req + bytes([crc16_std(req) & 0xff, crc16_std(req) >> 8])
    send_frame(bad)
    check("CRC 错帧 → uart3_tx_byte(5) 异常响应", tx_count == [5],
          f"tx_count={[hex(x) for x in tx_count]}")
    txb = [r8(TXBUF+i) for i in range(5)]
    check("异常响应[1]=功能码|0x80 (0x83)", txb[1] == 0x83, f"{hex(txb[1])}")
    check("异常响应[2]=异常码 0x04(CRC)", txb[2] == 0x04, f"{hex(txb[2])}")

    # ── 场景3：站址不匹配 → 兜底，不发 ──
    send_frame(frame)                  # 正常读 → [7]
    send_frame(frame, slave=0x02)      # 帧[0]=01 != 本站2 → 直接返回，不发
    check("站址不匹配 → 不发送", tx_count == [], f"tx_count={[hex(x) for x in tx_count]}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
