# -*- coding: utf-8 -*-
"""状态机 entry 的 EEPROM 周期同步门控 A/B（原 BIN 0x49C2-0x49EA）。

原 BIN 两段等价判定都带 retry 计数门控：

  0x49A4 ldr [0x4D14]=0x10001614（持久计数） ; cmp #0x78 ; bne 0x49C2
  0x49AC ldr [0x4D18]=0x10001730（pending）   ; cbnz 0x49C2
  → 设 pending=1、retry=0、同步

  0x49C2 cmp.w #0x12c ; bne 0x49EA
  0x49CC ldr [0x4D18]（pending） ; cmp #2 ; bne 0x49EA
  0x49D4 ldr [0x4D1C]=0x10001734（retry）; cbnz 0x49EA   ← retry 非 0 整块跳过
  → 设 pending=0、retry=1、同步

本测试直接执行 state_machine 的 entry 段，比较原 BIN 与新固件在
(持久计数, pending, retry) 各种组合下的「同步调用次数 + pending/retry 终值」。
重点覆盖 0x12c 分支的 retry 门控：retry != 0 时不得再同步。
"""
import os
import sys
import struct

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0
from verify_firmware_equivalence import machine, run, SYMS

ORIG_STATE = 0x458C
ORIG_PARAM_SYNC = 0x35F2

PERSIST_COUNTER = 0x10001614
SYNC_PENDING = 0x10001730
SYNC_RETRY = 0x10001734
OPERATION_CONFIG = 0x10001628

# entry 段里会走 I2C 与显示；统一跳过，只观察同步门控。
SKIP_OLD = (0x7A8, 0x1C82, 0x1C6C, 0x8F4, 0x94A, 0x7B6)


def execute(is_new, persist, pending, retry):
    uc = machine(is_new)
    sync_entry = SYMS["param_sync_live_to_eeprom"] if is_new else ORIG_PARAM_SYNC
    skip_list = list(SKIP_OLD)
    if is_new:
        skip_list += [SYMS[n] for n in ("Delay", "i2c_delay", "i2c_delay_short",
                                        "disp_data", "disp_cmd", "lcd_ctrl_line")]
    for address in skip_list:
        uc.hook_add(UC_HOOK_CODE,
                    lambda m, a, s, u: m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR)),
                    begin=address, end=address + 1)

    sync_calls = []
    def hook_sync(m, a, s, u):
        sync_calls.append(a)
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
    uc.hook_add(UC_HOOK_CODE, hook_sync, begin=sync_entry, end=sync_entry + 1)

    uc.mem_write(PERSIST_COUNTER, struct.pack("<I", persist))
    uc.mem_write(SYNC_PENDING, struct.pack("<I", pending))
    uc.mem_write(SYNC_RETRY, struct.pack("<I", retry))
    uc.mem_write(OPERATION_CONFIG, bytes((1,)))       # 运行中才会进统计/同步块
    uc.mem_write(0x10001624, struct.pack("<I", 0))    # 无故障
    uc.mem_write(0x10001744, bytes((1, 0, 0)))        # 屏号 1、项号 0
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))   # P0.9 高会被去抖捕获，置输入无效

    uc.reg_write(UC_ARM_REG_R0, 0)
    uc.reg_write(UC_ARM_REG_LR, 0x3FF01)
    run(uc, ORIG_STATE if not is_new else SYMS["state_machine"], max_insn=4_000_000)

    final_pending = struct.unpack("<I", bytes(uc.mem_read(SYNC_PENDING, 4)))[0]
    final_retry = struct.unpack("<I", bytes(uc.mem_read(SYNC_RETRY, 4)))[0]
    return (len(sync_calls), final_pending, final_retry)


def main():
    cases = []
    for persist in (0x77, 0x78, 0x79, 0x12b, 0x12c, 0x12d):
        for pending in (0, 1, 2, 3):
            for retry in (0, 1, 2, 5):
                cases.append((persist, pending, retry))

    failed = 0
    gate_checked = 0
    for case in cases:
        old = execute(False, *case)
        new = execute(True, *case)
        if old != new:
            failed += 1
            print(f"  [FAIL] persist=0x{case[0]:x} pending={case[1]} retry={case[2]} "
                  f"原={old} 新={new}")
            continue
        # 关键断言：0x12c 分支在 retry!=0 时原机不得同步
        if case[0] == 0x12c and case[1] == 2 and case[2] != 0:
            gate_checked += 1
            if old[0] != 0:
                failed += 1
                print(f"  [FAIL] retry={case[2]} 时原机竟同步 {old[0]} 次（门控未生效）")
    print(f"  [{'PASS' if not failed else 'FAIL'}] 周期同步 retry 门控 "
          f"{len(cases)-failed}/{len(cases)}（retry 门控断言 {gate_checked} 例）")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
