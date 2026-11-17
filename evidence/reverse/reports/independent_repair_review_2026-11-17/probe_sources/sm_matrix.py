# -*- coding: utf-8 -*-
"""state_machine 全矩阵差分：菜单 x 项号 x 视图 x 按键 x 参数档。

比较三件事：
  1. disp_* 调用序列（名称 + R0/R1），跳过 Delay 与字形渲染；
  2. 执行后 RAM 末态（0x10000000..0x1000213C），掩掉已授权背光定制槽；
  3. 是否真正执行到返回点（未到即预算不足，判为无效用例）。

用户授权偏离（不纳入比对）：
  - 背光 5000->10000：0x10001770 / 0x10001764
  - 菜单文字/字库：disp_string 的字符串地址相同，字形差异只在渲染内部体现，
    故对 disp_render_char8 / disp_render_char16 整体跳过。
"""
import os
import struct
import sys
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/verification"))
import verify_firmware_equivalence as V  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,  # noqa: E402
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

ORIG_STATE = 0x458C
RET = 0x3FF00
BUDGET = 40_000_000

ORIG_FUNCS = {0xD3C: "disp_string", 0xED0: "disp_uint4", 0xE42: "disp_number3",
              0x13E8: "disp_uint2", 0x11BC: "disp_signed_angle", 0x143C: "disp_fixed_1dec",
              0x1092: "disp_number", 0xF8C: "disp_uint5", 0x12B0: "disp_offset",
              0x0D94: "disp_digit", 0x0992: "disp_clear", 0x427C: "disp_splash_screen",
              0x448A: "disp_screen_static", 0x44C2: "disp_screen_calib"}
RENDER8_OLD = 0xB44
RENDER16_OLD = 0xC3A
DELAY_OLD = 0x7A8

ARGC = {"disp_digit": 1, "disp_clear": 0, "disp_splash_screen": 0,
        "disp_screen_static": 0, "disp_screen_calib": 0}

# 授权偏离槽：背光计时 / 空闲超时
MASK_ADDRS = [0x10001770, 0x10001764]

# 单字节参数（避免把 4 字节写覆盖相邻变量）
BYTE_ADDRS = (0x10001656, 0x10001657, 0x10001658, 0x10001659, 0x1000165A,
              0x1000165B, 0x1000170E, 0x1000170F,
              0x10001711, 0x10001712, 0x10001713, 0x10001714, 0x10001715,
              0x10001716, 0x10001717, 0x10001718, 0x10001722, 0x10001723,
              0x10001724, 0x10001725, 0x10001726, 0x10001785, 0x1000164D,
              0x1000164C, 0x10001710, 0x10001628, 0x10001655)

PARAM_PROFILES = [
    {},
    {0x10001648: 1500},
    {0x10001644: 1500},
    {0x10001628: 3},
    {0x10001634: 2},
    {0x10001634: 3},
    {0x10001657: 3},
    {0x10001657: 2},
    {0x10001658: 3},
    {0x10001658: 1},
    {0x10001659: 3},
    {0x1000165A: 2},
    {0x1000165B: 2},
    {0x10001656: 2},
    {0x10001655: 3},
    {0x10001710: 1},
    {0x10001710: 3},
    {0x10001710: 2, 0x10001634: 2},
    # 故障/运行态档位：状态行、KEY_BACK 跳转与停机清理分支只在故障或运行中触发
    {0x10001624: 0x10},
    {0x10001624: 0x4000},
    {0x10001624: 0x200},
    {0x10001628: 1},
    {0x10001628: 1, 0x10001624: 0x10},
]

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


def resolve(is_new):
    table = {V.SYMS[n]: n for n in ORIG_FUNCS.values() if n in V.SYMS} if is_new else dict(ORIG_FUNCS)
    return table


def skip_addrs(is_new):
    names = ["Delay", "disp_render_char8", "disp_render_char16", "i2c_delay", "i2c_delay_short"]
    if is_new:
        return [V.SYMS[n] for n in names if n in V.SYMS]
    return [DELAY_OLD, RENDER8_OLD, RENDER16_OLD, 0x1C82, 0x1C6C]


# 密码错误提示的双层空转（外层 10000 / 内层 1000，每外层喂狗一次）在原 BIN 里被
# 内联成 10 份副本（0x549e/0x55e0/0x8c88/0x8d40/0x8d8a/0x8dd4/0x8e56/0x8f0c/0x8fda/0x9026，
# 均为 `movw r1,#10000; cmp; bcc <loop>` 的循环尾），新 ELF 里是独立函数
# delay_for_password_feedback（0x3F4E-0x3F8C，`movw r6,#9999; cmp; bls`）。两版逐指令等价。
# 为免 40M 指令预算被它耗尽：原 BIN 在每个 movw 处把外层计数写成 10000（紧随的 cmp/bcc
# 立即不成立而退出循环，只多执行一轮），新 ELF 直接把函数入口跳到 LR。
FASTFWD_OUTER = 0x1000161C
FASTFWD_INNER = 0x10001620
FASTFWD_OLD_SITES = (0x549E, 0x55E0, 0x8C88, 0x8D40, 0x8D8A, 0x8DD4,
                     0x8E56, 0x8F0C, 0x8FDA, 0x9026)
FASTFWD_NEW_FUNC = "delay_for_password_feedback"


def install_fastfwd(uc, is_new):
    if is_new:
        if FASTFWD_NEW_FUNC not in V.SYMS:
            return
        start = V.SYMS[FASTFWD_NEW_FUNC]

        def handler(m, address, size, user):
            m.mem_write(FASTFWD_OUTER, struct.pack("<I", 10000))
            m.mem_write(FASTFWD_INNER, struct.pack("<I", 1000))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

        uc.hook_add(UC_HOOK_CODE, handler, begin=start, end=start + 1)
        return

    def old_handler(m, address, size, user):
        m.mem_write(FASTFWD_OUTER, struct.pack("<I", 10000))
        m.mem_write(FASTFWD_INNER, struct.pack("<I", 1000))

    for site in FASTFWD_OLD_SITES:
        uc.hook_add(UC_HOOK_CODE, old_handler, begin=site, end=site + 1)


def seed(uc, params, stats=0xFA):
    for address, value in params.items():
        n = 1 if address in BYTE_ADDRS else 4
        uc.mem_write(address, struct.pack("<I", value)[:n])
    uc.mem_write(0x10001778, struct.pack("<I", stats))
    uc.mem_write(0x2009C014, struct.pack("<I", 0xFFFFFFFF))
    uc.mem_write(0x2009C034, struct.pack("<I", 0xFFFFFFFF))


def run_case(is_new, menu, item, view, key, params, stats=0xFA):
    uc = V.machine(is_new)
    # 密码页路径会喂狗（WDFEED@0x40000008），machine() 未映射该页
    try:
        uc.mem_map(0x40000000, 0x1000)
    except Exception:  # noqa: BLE001  已映射则忽略
        pass
    table = resolve(is_new)
    calls = []

    def hook(m, address, size, user):
        name = table[address]
        n = ARGC.get(name, 4)
        args = [m.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1,
                                        UC_ARM_REG_R2, UC_ARM_REG_R3)][:n]
        calls.append((name,) + tuple(args))
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    for address in table:
        uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address + 1)
    for address in skip_addrs(is_new):
        uc.hook_add(UC_HOOK_CODE,
                    lambda m, a, s, u: m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR)),
                    begin=address, end=address + 1)

    uc.mem_write(0x10001744, bytes((menu, item, view, 0)))
    seed(uc, params, stats)
    install_fastfwd(uc, is_new)

    state = V.SYMS["state_machine"] if is_new else ORIG_STATE
    uc.reg_write(UC_ARM_REG_R0, key)
    uc.reg_write(UC_ARM_REG_LR, RET)
    reached = [False]

    def stop(m, address, size, user):
        if address == RET:
            reached[0] = True
            m.emu_stop()

    uc.hook_add(UC_HOOK_CODE, stop)
    uc.emu_start(state | 1, 0, count=BUDGET)
    ram = bytearray(uc.mem_read(0x10000000, 0x213C))
    for addr in MASK_ADDRS:
        for i in range(4):
            ram[addr - 0x10000000 + i] = 0
    return calls, bytes(ram), reached[0]


def main():
    menus = list(range(0, 0x20))
    if len(sys.argv) > 1:
        menus = [int(x, 16) for x in sys.argv[1].split(",")]
    # 默认遍历 查看/编辑 两种视图（view=1 才进入各页的编辑分支）
    views = [0, 1]
    if os.environ.get("SM_VIEWS"):
        views = [int(x) for x in os.environ["SM_VIEWS"].split(",")]
    items = list(range(0, 0x11))
    keys = [0, 1, 2, 3, 4, 5, 0x16, 0x0B, 0x17, 0x0E, 0x21]
    # 统计刷新计数（0x10001778）决定帧内走哪条渲染分支：
    #   0xFA → 自增到 0xFB 走整页重绘；0x1F4 → 自增到 0x1F5 走 >0x1F4 的擦除分支。
    stats_list = [0xFA]
    if os.environ.get("SM_STATS"):
        stats_list = [int(x, 16) for x in os.environ["SM_STATS"].split(",")]
    profiles = PARAM_PROFILES
    if os.environ.get("SM_PROFILES"):
        profiles = [PARAM_PROFILES[int(x)] for x in os.environ["SM_PROFILES"].split(",")]
    bad = 0
    cases = 0
    for menu in menus:
        for view in views:
            for item in items:
                for key in keys:
                    for prof in profiles:
                        for st in stats_list:
                            params = dict(BASE)
                            params.update(prof)
                            cases += 1
                            try:
                                a = run_case(False, menu, item, view, key, params, st)
                                b = run_case(True, menu, item, view, key, params, st)
                            except Exception:  # noqa: BLE001
                                print("EXC menu=%#x item=%d view=%d key=%#x st=%#x prof=%r"
                                      % (menu, item, view, key, st, prof))
                                traceback.print_exc()
                                bad += 1
                                continue
                            if not a[2] or not b[2]:
                                print("TRUNC menu=%#x item=%d view=%d key=%#x st=%#x prof=%r reached=%s/%s"
                                      % (menu, item, view, key, st, prof, a[2], b[2]))
                                bad += 1
                                continue
                            if a[0] != b[0]:
                                i = next((i for i, (x, y) in enumerate(zip(a[0], b[0])) if x != y),
                                         min(len(a[0]), len(b[0])))
                                print("CALL menu=%#x item=%d view=%d key=%#x st=%#x prof=%r #%d len=%d/%d old=%s new=%s"
                                      % (menu, item, view, key, st, prof, i, len(a[0]), len(b[0]),
                                         a[0][i] if i < len(a[0]) else None,
                                         b[0][i] if i < len(b[0]) else None))
                                bad += 1
                                continue
                            if a[1] != b[1]:
                                off = next(i for i in range(len(a[1])) if a[1][i] != b[1][i])
                                print("RAM  menu=%#x item=%d view=%d key=%#x st=%#x prof=%r @%#x old=%#x new=%#x"
                                      % (menu, item, view, key, st, prof, 0x10000000 + off,
                                         a[1][off], b[1][off]))
                                bad += 1
                            if bad > 40:
                                print("TRUNCATED")
                                print("cases=%d bad=%d" % (cases, bad))
                                return 1
    print("cases=%d bad=%d" % (cases, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
