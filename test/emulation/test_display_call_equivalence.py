# -*- coding: utf-8 -*-
"""第三轮审计修复的显示调用契约与原 BIN 对照测试。

覆盖第二轮审计发现的五类差异（这些差异不体现在 SRAM/GPIO 上，原 A/B 矩阵又把
disp_* 整体 skip，所以必须直接比较「显示函数调用序列」）：

  1. 参数页电压/电流限值：范围≥限值才画数值并补单位（列 0xf），否则画 DISABLED；
  2. 软起/软停时间用 disp_uint4（4 列），不是 disp_number3；
  3. 参数菜单进入 PID 页时 gain_a/gain_b 用 disp_uint4，不是 disp_uint2；
  4. 枚举值越界（control_mode/control_method/emergency_stop >2）时原厂不画该行；
  5. 首页 350 拍刷新第一行只在 control_method 0/1/2 时绘制。

另含两处控制量差异：control_mode==2 时不重算 target_amplitude / manual_scaled_output；
control_method>=3 时原厂直接跳过手动 ±1。
"""
import os
import struct
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_LR, UC_ARM_REG_PC,
)
from verify_firmware_equivalence import machine, run, SYMS

ORIG_STATE = 0x458C
RETURN = 0x3FF01

ORIG_FUNCS = {0xD3C: "disp_string", 0xED0: "disp_uint4", 0xE42: "disp_number3",
              0x13E8: "disp_uint2", 0x11BC: "disp_signed_angle", 0x143C: "disp_fixed_1dec",
              0x1092: "disp_number", 0xF8C: "disp_uint5", 0x12B0: "disp_offset",
              0x0D94: "disp_digit", 0x0992: "disp_clear", 0x427C: "disp_splash_screen"}
NEW_NAMES = tuple(ORIG_FUNCS.values())
ARGC = {"disp_digit": 1, "disp_clear": 0, "disp_splash_screen": 0}


def resolve(is_new):
    return {SYMS[name]: name for name in NEW_NAMES if name in SYMS} if is_new else dict(ORIG_FUNCS)


def skip_hooks(uc, table):
    def skip(m, address, size, user):
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
    for address in table:
        uc.hook_add(UC_HOOK_CODE, skip, begin=address, end=address + 1)


BASE = {
    0x10001624: 0, 0x10001628: 0, 0x1000162C: 50, 0x10001630: 50, 0x10001634: 0,
    0x10001638: 1000, 0x1000163C: 1000, 0x10001640: 1000, 0x10001644: 500,
    0x10001648: 800, 0x1000164C: 3, 0x1000164D: 2, 0x10001650: 5, 0x10001654: 10,
    0x10001657: 0, 0x10001659: 0, 0x1000165B: 0, 0x10001660: 90,
    0x1000170E: 10, 0x1000170F: 20, 0x10001710: 0,
    0x10001711: 1, 0x10001712: 2, 0x10001713: 3, 0x10001714: 4,
    0x10001715: 5, 0x10001716: 6, 0x10001717: 7, 0x10001718: 8,
    0x10001722: 50, 0x10001723: 40, 0x10001724: 30, 0x10001725: 20, 0x10001726: 10,
    0x1000172C: 0, 0x10001730: 0, 0x10001734: 0, 0x10001785: 0,
    0x100015A8: 500, 0x100015D0: 500, 0x10001788: 61, 0x10001774: 7,
    0x100015D8: 137, 0x100015D4: 9,
}


def trace(is_new, menu, item, view, stats, params, key=0):
    uc = machine(is_new)
    table = resolve(is_new)
    calls = []

    def hook(m, address, size, user):
        name = table[address]
        args = [m.reg_read(UC_ARM_REG_R0), m.reg_read(UC_ARM_REG_R1),
                m.reg_read(UC_ARM_REG_R2), m.reg_read(UC_ARM_REG_R3)]
        calls.append((name,) + tuple(args[:ARGC.get(name, 4)]))
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    for address in table:
        uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address + 1)
    uc.mem_write(0x10001744, bytes((menu, item, view, 0)))
    uc.mem_write(0x10001778, struct.pack("<I", stats))
    for address, value in params.items():
        uc.mem_write(address, struct.pack("<I", value))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))
    state = SYMS["state_machine"] if is_new else ORIG_STATE
    uc.reg_write(UC_ARM_REG_R0, key)
    uc.reg_write(UC_ARM_REG_LR, RETURN)
    run(uc, state, max_insn=3_000_000)
    return calls


def state_snapshot(is_new, menu, key, params):
    uc = machine(is_new)
    table = resolve(is_new)
    skip_hooks(uc, table)
    uc.mem_write(0x10001744, bytes((menu, 0, 0, 0)))
    uc.mem_write(0x10001778, struct.pack("<I", 0xFA))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))
    for address, value in params.items():
        uc.mem_write(address, struct.pack("<I", value))
    state = SYMS["state_machine"] if is_new else ORIG_STATE
    uc.reg_write(UC_ARM_REG_R0, key)
    uc.reg_write(UC_ARM_REG_LR, RETURN)
    run(uc, state, max_insn=3_000_000)
    return bytes(uc.mem_read(0x10000000, 0x2200))


def check_trace(cases):
    failed = 0
    for case in cases:
        old = trace(False, *case)
        new = trace(True, *case)
        if old != new:
            failed += 1
            first = next((i for i, (a, b) in enumerate(zip(old, new)) if a != b),
                         min(len(old), len(new)))
            print("  [FAIL] trace case=%s OLD=%s NEW=%s"
                  % (case[:3], old[first] if first < len(old) else None,
                     new[first] if first < len(new) else None))
    print("  [%s] 显示调用契约 %d/%d" % ("PASS" if not failed else "FAIL",
                                      len(cases) - failed, len(cases)))
    return failed


def check_state(cases):
    failed = 0
    for menu, key, extra in cases:
        params = dict(BASE)
        params.update(extra)
        old = state_snapshot(False, menu, key, params)
        new = state_snapshot(True, menu, key, params)
        if old != new:
            failed += 1
            offset = next(i for i, (a, b) in enumerate(zip(old, new)) if a != b)
            print("  [FAIL] state case=(%#x, %#x, %s) addr=0x%08X OLD=0x%02X NEW=0x%02X"
                  % (menu, key, extra, 0x10000000 + offset, old[offset], new[offset]))
    print("  [%s] 控制量终态 %d/%d" % ("PASS" if not failed else "FAIL",
                                   len(cases) - failed, len(cases)))
    return failed


def main():
    trace_cases = []
    variants = ({}, {0x10001648: 1500}, {0x10001644: 1500}, {0x10001628: 3},
                {0x10001634: 3}, {0x10001657: 3}, {0x10001655: 3})
    for item in range(16):
        for extra in variants:
            params = dict(BASE)
            params.update(extra)
            trace_cases.append((3, item, 0, 0xFA, params, 0))
    for item in range(9):
        for extra in ({}, {0x10001710: 1}, {0x10001710: 3}, {0x10001634: 2}):
            params = dict(BASE)
            params.update(extra)
            trace_cases.append((2, item, 0, 0xFA, params, 1))
    for method in range(4):
        params = dict(BASE)
        params[0x10001655] = method
        params[0x10001768] = 0x15D
        trace_cases.append((1, 0, 0, 0xFA, params, 0))

    state_cases = []
    for mode in (0, 1, 2):
        for method in (0, 1, 2, 3):
            extra = {0x10001634: mode, 0x10001655: method,
                     0x10001638: 2000, 0x10001788: 61, 0x10001774: 7,
                     0x100015D8: 137, 0x100015D4: 9}
            state_cases.append((1, 0, extra))

    failed = check_trace(trace_cases) + check_state(state_cases)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
