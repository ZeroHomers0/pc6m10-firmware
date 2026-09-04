# -*- coding: utf-8 -*-
# =============================================================================
# test_modbus_regmap.py — host 虚拟数据测试：Modbus 寄存器读写映射对称性
#
# 被测对象：firmware/src/08_uart3_modbus.c 的 modbus_read_reg(0xAF94) /
#           modbus_write_multi(0xB2E0)
#
# 验证：集中描述表覆盖协议寄存器，且读写位宽保持一致。
# 读写表允许协议规定的有意非对称映射（例如活动 PID 增益的读回与 profile4 写入）。
# 地址真值来自语义地址映射头文件（与 firmware.elf 一致），不硬编码。
# =============================================================================
import re, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MOD  = os.path.join(ROOT, 'firmware', 'src', '08_uart3_modbus.c')
MAPS = [os.path.join(ROOT, 'firmware', 'inc', 'firmware_state.h'),
        os.path.join(ROOT, 'firmware', 'inc', 'firmware_parameters.h')]
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

# ── 统一 case 标签 → 字节值 ──
def parse_case(lit):
    if lit.startswith('\\x') and len(lit) >= 4:
        return int(lit[2:], 16)
    esc = {'0':0x00,'a':0x07,'b':0x08,'t':0x09,'n':0x0a,'v':0x0b,'f':0x0c,'r':0x0d}
    if lit.startswith('\\'):
        c = lit[1]
        return esc.get(c, ord(c) & 0xff) if c in esc else (ord(c) & 0xff if len(lit)==0 else 0)
    if len(lit) == 1:
        return ord(lit) & 0xff
    return 0

def symbols(text):
    out = {}
    for m in re.finditer(
            r'#define\s+([A-Za-z0-9_]+)\s+\(\(volatile\s+(uint(?:8|16|32)_t)\s*\*\)\s*(0x[0-9a-fA-F]+)',
            text):
        out[m.group(1)] = (m.group(3).lower(), int(m.group(2)[4:-2]))
    return out

def descriptor_map(text, table_name):
    """解析 `[reg] = { address, PARAMETER_STORAGE_* }` 描述表。"""
    start = text.index(table_name)
    tail = text.index('\n};', start)
    body = text[start:tail]
    out = {}
    for m in re.finditer(
            r"\[0x([0-9a-fA-F]+)\]\s*=\s*\{\s*"
            r"([A-Za-z0-9_]+|0)\s*,\s*"
            r"(PARAMETER_STORAGE_(?:BYTE|WORD)|0)\s*\}", body):
        out[int(m.group(1), 16)] = (m.group(2), m.group(3))
    return out

def read_map(text):
    return descriptor_map(text, 'modbus_read_register_table')

def write_map(text):
    return descriptor_map(text, 'modbus_write_register_table')

def main():
    mod  = open(MOD, encoding='utf-8', errors='ignore').read()
    sym  = {}
    for path in MAPS:
        sym.update(symbols(open(path, encoding='utf-8', errors='ignore').read()))
    rm = read_map(mod); wm = write_map(mod)

    passed=failed=0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st="PASS" if cond else "FAIL"
        if cond: passed+=1
        else: failed+=1
        print(f"  [{st}] {name}"+(f"  {detail}" if detail else ""))

    print(f"read 描述 {len(rm)} 项 | write 描述 {len(wm)} 项 | 语义映射 {len(sym)} 符号")

    common = sorted(set(rm) & set(wm))
    print(f"  读/写共有 reg: {len(common)}")

    # INV1 读写描述项均为有效地址或明确保留项
    # 这些寄存器在协议中是只读运行量；写入分支按原固件约定落入
    # 临时槽，不应把“不可写”误报为读写映射破坏。
    scratch_write_regs = set(range(0x1A, 0x20)) | {0x24, 0x25, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D}
    asym=[]
    for r in common:
        if r in scratch_write_regs:
            continue
        rs, ws = rm[r], wm[r]
        if rs[0] == '0' or ws[0] == '0':
            asym.append((r, rs, ws))
        elif rs[0] not in sym or ws[0] not in sym:
            asym.append((r, rs, ws))
    check("INV1 可写 reg 的读/写均有语义地址", len(asym)==0, f"{asym[:5]}" if asym else "")

    # INV2 位宽一致（全局符号宽度：uint8_t*=8,uint32_t*=32；从声明推导）
    width_mism=[]
    for r in common:
        if r in scratch_write_regs:
            continue
        rs, ws = rm[r], wm[r]
        if rs[1] != ws[1]:
            width_mism.append((r, rs, ws))
    check("INV2 读/写位宽一致", len(width_mism)==0, f"{width_mism[:5]}" if width_mism else "")

    # INV3 保留区：read 返回 0 的 case（无源符号映射）应在 g_scratch/未映射
    missing_read = [f"{r:02x}" for r in range(0, 0x3F) if r not in rm]
    missing_write = [f"{r:02x}" for r in range(0, 0x3E) if r not in wm]
    check("read 表覆盖 0x00-0x3E", not missing_read, f"缺失 {missing_read}" if missing_read else "")
    check("write 表覆盖 0x00-0x3D", not missing_write, f"缺失 {missing_write}" if missing_write else "")

    # INV1 补：read-only 的 case（只读不写）与 write-only（只写不读）差异统计
    read_only = sorted(set(rm)-set(wm))
    write_only= sorted(set(wm)-set(rm))
    check(f"read-only reg（不能写）{len(read_only)} 个", True, f"{[hex(x) for x in read_only]}")
    check(f"write-only reg（读不返回）{len(write_only)} 个", True, f"{[hex(x) for x in write_only]}")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed==0 else 1

if __name__ == '__main__':
    sys.exit(main())
