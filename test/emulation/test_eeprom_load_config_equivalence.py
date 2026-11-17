# -*- coding: utf-8 -*-
"""load_config 双银行读取/默认恢复的原 BIN / 新 ELF A/B。"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "test", "support"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1
from unicorn_harness import load_firmware, load_original, lookup

ORIG_LOAD = 0x25DC
ORIG_WRITE = 0x1E88
ORIG_READ = 0x1EBC
RETURN = 0xFF000000
RAM_IMAGE = os.path.join(ROOT, "firmware", "assets", "ram_data_image.bin")


def run(load_fn, load_addr, read_addr, write_addr, eeprom):
    uc = load_fn()
    with open(RAM_IMAGE, "rb") as f:
        uc.mem_write(0x10000000, f.read())
    writes = []

    def read_hook(machine, address, size, user):
        out = machine.reg_read(UC_ARM_REG_R0)
        reg = machine.reg_read(UC_ARM_REG_R1) & 0xFF
        machine.mem_write(out, bytes([eeprom[reg]]))
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    def write_hook(machine, address, size, user):
        data = machine.reg_read(UC_ARM_REG_R0) & 0xFF
        reg = machine.reg_read(UC_ARM_REG_R1) & 0xFF
        writes.append((reg, data))
        machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, read_hook, begin=read_addr, end=read_addr + 1)
    uc.hook_add(UC_HOOK_CODE, write_hook, begin=write_addr, end=write_addr + 1)
    uc.reg_write(UC_ARM_REG_LR, RETURN)
    uc.emu_start(load_addr | 1, RETURN, count=2_000_000)
    assert uc.reg_read(UC_ARM_REG_PC) == RETURN, '配置加载未正常返回，禁止比较中途状态'
    return bytes(uc.mem_read(0x10000000, 0x4000)), writes


def main():
    new_load = lookup("load_config")
    new_read = lookup("i2c_read_reg")
    new_write = lookup("i2c_write_reg")
    base = bytearray(((i * 73 + 19) & 0xFF) for i in range(256))
    cases = []
    valid = bytearray(base)
    valid[5] = valid[6] = 0x55
    valid[7] = valid[8] = 0x66
    cases.append(("双银行魔数有效：全部从EEPROM装载", valid))
    bad_a = bytearray(valid)
    bad_a[5] = bad_a[6] = 0
    cases.append(("银行A无效：回写A默认值", bad_a))
    bad_b = bytearray(valid)
    bad_b[7] = bad_b[8] = 0
    cases.append(("银行B无效：回写B默认值", bad_b))
    bad_both = bytearray(base)
    bad_both[5] = bad_both[6] = bad_both[7] = bad_both[8] = 0
    cases.append(("双银行无效：依次回写两组默认值", bad_both))

    # 只一侧魔数命中（'U'/'f' 只出现在 lo 或只出现在 hi）—— 原机用 `==` 逐字节或，
    # 只有两个魔数都错才走默认回写，边界在这里。
    only_a_lo = bytearray(bad_both)
    only_a_lo[5] = 0x55
    cases.append(("仅银行A lo=0x55：A读取、B回写默认", only_a_lo))
    only_a_hi = bytearray(bad_both)
    only_a_hi[6] = 0x55
    cases.append(("仅银行A hi=0x55：A读取、B回写默认", only_a_hi))
    only_b_hi = bytearray(bad_both)
    only_b_hi[8] = 0x66
    cases.append(("仅银行B hi=0x66：A回写默认、B读取", only_b_hi))
    wrong_magic = bytearray(valid)
    wrong_magic[5] = wrong_magic[6] = 0x54      # 'T' 而非 'U'
    cases.append(("银行A魔数近似值0x54：仍应回写默认", wrong_magic))

    # 随机内容 * 有效魔数：覆盖全部 16 位参数的拼合方向与每一条 live 槽。
    for seed in range(1, 15):
        rnd = bytearray(((i * 1103515245 + seed * 12345) >> 7) & 0xFF for i in range(256))
        rnd[5] = rnd[6] = 0x55
        rnd[7] = rnd[8] = 0x66
        cases.append((f"随机内容 seed={seed}：双银行有效", rnd))

    failed = 0
    for name, image in cases:
        old_ram, old_writes = run(load_original, ORIG_LOAD, ORIG_READ, ORIG_WRITE, image)
        new_ram, new_writes = run(load_firmware, new_load, new_read, new_write, image)
        ok = old_ram == new_ram and old_writes == new_writes
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}  writes={len(old_writes)}")
        if not ok:
            if old_writes != new_writes:
                print(f"    写序列不一致：原={old_writes[:8]} 新={new_writes[:8]}")
            else:
                diff = next((i for i, (a, b) in enumerate(zip(old_ram, new_ram)) if a != b), -1)
                print(f"    RAM首差异：0x{0x10000000 + diff:08X}")
        failed += not ok
    print(f"\n  通过 {len(cases)-failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
