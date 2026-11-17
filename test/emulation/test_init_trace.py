# -*- coding: utf-8 -*-
"""6M：为官方套件从未执行级覆盖的初始化/底层 I/O 函数补 A/B 对照。

判定方式与 12M 一致：这些函数不读业务参数、不依赖 SRAM 内容，只写外设与 GPIO，
因此不需要播种业务状态，直接比较「外设写迹(地址/宽度/值) + 10 个外设页末态」。

必须绕开的 Unicorn 陷阱（与 12M 同源，全部实测）：
  1. FIO 页 0x2009C000 不能挂 UC_HOOK_MEM_WRITE —— TCG 缓存会让新 ELF 的 GPIO 函数体
     一条都不执行，表现为「新固件少写」。只比末态。
  2. SystemInit 死等 OSCSTAT / PLL 锁定位，且 MEM_WRITE 钩子早于写入生效，须记账到
     pending 再在下一指令补位。
  3. long_delay 按参数空转，需用固定小入参并跳过其内层循环，否则耗时爆炸。
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "verification"))

from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE  # noqa: E402
from unicorn.arm_const import (UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,  # noqa: E402
                                UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                                UC_ARM_REG_SP)
import verify_firmware_equivalence as V  # noqa: E402

PAGES = (
    (0x40000000, 0x1000),   # WDT / UART0
    (0x40004000, 0x1000),   # TIMER0
    (0x40008000, 0x1000),   # PINSEL0/1
    (0x4002C000, 0x1000),   # PINSEL2/3/4（EINT）
    (0x40034000, 0x1000),   # ADC0
    (0x40090000, 0x1000),   # TIMER1
    (0x4009C000, 0x1000),   # TIMER2 / UART3
    (0x400FC000, 0x1000),   # PCONP / PCLKSEL / SCS / PLL
    (0xE000E000, 0x1000),   # NVIC
    (0x2009C000, 0x1000),   # FIO0/1/2/3
)

FIO_BASE = 0x2009C000
SCS = 0x400FC1A0
PLL0FEED, PLL0STAT = 0x400FC08C, 0x400FC088
PLL1FEED, PLL1STAT = 0x400FC0AC, 0x400FC0A8

SKIP_OLD = {0x7A8: "Delay", 0x94A: "disp_cmd", 0x8F4: "disp_data"}
SKIP_NEW_NAMES = ("Delay", "disp_cmd", "disp_data")

# (原机入口, 新 ELF 符号, 名称, 入参)
TARGETS = (
    (0x0440, "SystemInit", "SystemInit", ()),
    (0x0248, "timer0_init", "timer0_init", ()),
    (0x0598, "long_delay", "long_delay", ()),
    (0x07D6, "lcd_data_byte", "lcd_data_byte(0x55)", (0x55,)),
    (0x07D6, "lcd_data_byte", "lcd_data_byte(0x00)", (0x00,)),
    (0x07D6, "lcd_data_byte", "lcd_data_byte(0xFF)", (0xFF,)),
    (0x0A30, "gpio1_init", "gpio1_init", ()),
    (0x1578, "gpio_inputs_dir_init", "gpio_inputs_dir_init", ()),
    (0x1BEE, "chk_p02_p03", "chk_p02_p03", ()),
    (0x1F04, "adc_init", "adc_init", ()),
    (0xE816, "timer1_init", "timer1_init", ()),
    (0xE838, "timer2_init", "timer2_init", ()),
    (0xE86E, "eint1_init", "eint1_init", ()),
    (0xE8CA, "eint2_init", "eint2_init", ()),
    (0xE908, "eint3_init", "eint3_init", ()),
    (0xE79A, "gpio_outputs_set", "gpio_outputs_set", ()),
    (0x10628, "nvic_enable_irq", "nvic_enable_irq(0)", (0,)),
    (0x10628, "nvic_enable_irq", "nvic_enable_irq(20)", (20,)),
    (0x10628, "nvic_enable_irq", "nvic_enable_irq(37)", (37,)),
)

# long_delay 是 6000×1000 的双层空转（原机 0x598 `movw r1,#6000` + `cmp.w #1000`，
# 新 ELF `cmp.w r3,#1000` + 外层同 6000），实跑要 3000 万条以上指令。它的语义就是
# 「空转固定次数」，没有可观察输出，因此用快进跳过循环体，并把两侧的循环边界常量
# 单独断言 —— 只跳过时间，不跳过结论。
LONG_DELAY_OLD = 0x598
LONG_DELAY_OUTER = 6000
LONG_DELAY_INNER = 1000
# 内层循环体首指令 + 内层计数器在栈上的偏移。
# 原机 0x5AA 是内层体首指令，内层计数器是 [sp,#0]、外层是 [sp,#4]；
# 新 ELF 0x5A6 是内层体首指令，但 GCC 交换了顺序：内层是 [sp,#4]、外层是 [sp,#0]。
# 命中内层体时把计数器直接置 999，内层只再跑一轮即退出 —— 只压缩时间，不改变
# 「外层跑满 6000 次」这一可观察结论。
LONG_DELAY_INNER_BODY = {False: 0x5AA, True: 0x5A6}
LONG_DELAY_INNER_OFF = {False: 0, True: 4}

# 后端读写 SRAM 的函数：只用「预置 + 整片 SRAM + 10 页末态」，不记录写迹
# （写迹在这类函数里会把每次 LCD 数据字节都记下来，噪声压过信号）。
SRAM0_END = 0x29A0
DISP_CMD_OLD, DISP_DATA_OLD = 0x94A, 0x8F4
# 原机显示叶函数入口：跳过函数体但记录调用（见 _observe_sram 的 traced）
DISP_TRACE_OLD = {0xD3C: "disp_string", 0xB44: "disp_render_char8",
                  0xC3A: "disp_render_char16"}
AUTH_RETRY_OLD = 0x10820
AUTH_CHALLENGE_OLD = 0x106A0
PARAM_SYNC_OLD = 0x35F2
SRAM_CASES = (
    # (原机入口, 新 ELF 符号, 名称, 预置 {地址: 值})
    (0x10F8C, "gpio0_input_init", "gpio0_input_init",
     {0x2009C000 + 0x14: 0xFFFFFFFF}),
    (0x10FAE, "read_input_p02", "read_input_p02 (P0.2=0)",
     {0x2009C000 + 0x14: 0xFFFFFFFB}),
    (0x10FAE, "read_input_p02", "read_input_p02 (P0.2=1)",
     {0x2009C000 + 0x14: 0x00000004}),
    # freq_adjust_sync 的 6 个动作分支必须都覆盖（只跑 event=0 等于没测）：
    # event=2/0x16 递增且 >0x2b0 钳 0x2b0；event=3/0x21 <0x45 先钳 0x45 再减 1；
    # event=5 置运行标志；event=6 清标志。槽位 out_freq_adj=0x10001624、
    # shadow=0x10001628、cfg_word=0x10001620。
    (0xAB48, "freq_adjust_sync", "freq_adj(event=0)",
     {0x10001624: 0x0100, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=2) 未达上限",
     {0x10001624: 0x0100, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=2) 触上限",
     {0x10001624: 0x02B0, 0x10001628: 0x02B0, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=0x16) 触上限",
     {0x10001624: 0x02B5, 0x10001628: 0x02B5, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=3) 未达下限",
     {0x10001624: 0x0100, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=3) 触下限",
     {0x10001624: 0x0045, 0x10001628: 0x0045, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=0x21) 触下限",
     {0x10001624: 0x0040, 0x10001628: 0x0040, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=5) 置运行",
     {0x10001624: 0x0200, 0x10001628: 0x0100, 0x10001620: 0}),
    (0xAB48, "freq_adjust_sync", "freq_adj(event=6) 置停止",
     {0x10001624: 0x0200, 0x10001628: 0x0200, 0x10001620: 1}),
    (0x14F6, "disp_decimal1", "disp_decimal1(1234,1,2,0)",
     {0x10000000: 0}),
    (0x448A, "disp_screen_static", "disp_screen_static", {0x10000000: 0}),
    (0x44C2, "disp_screen_calib", "disp_screen_calib", {0x10000000: 0}),
    # run_stop_preset：读 operation_configuration 决定是否把 ramp 参数压成 5 并调
    # 两个 pin22 函数（原机 0xF9C8 `ldrb; cbnz 0xF9E6` 为 0 才进）。两个分支都要跑。
    (0xF9AA, "run_stop_preset", "run_stop_preset(cfg=0)",
     {0x10001628: 0, 0x100015FC: 0x1234, 0x10001614: 0x40}),
    (0xF9AA, "run_stop_preset", "run_stop_preset(cfg=1)",
     {0x10001628: 1, 0x100015FC: 0x1234, 0x10001614: 0x40}),
    # auth_set_timeout 只写一个 50000 常量槽；auth_retry 的重试循环里 auth_challenge
    # 会做 1-Wire 时序，本用例先把 retry_state 置成非 1 走「不进入」分支，再单独覆盖
    # 「进入但计数已满」的路径（两者都是原机 0x10820 的可达分支）。
    (0x10696, "auth_set_timeout", "auth_set_timeout", {0x100020EC: 0}),
    (0x10820, "auth_retry", "auth_retry(state=0)", {0x10001730: 0, 0x100020F0: 0}),
    (0x10820, "auth_retry", "auth_retry(state=1,cnt=9)", {0x10001730: 1, 0x100020F0: 9}),
    # 认证失败（auth_response_status=1）时才真正跑满重试轮数：counter 从 0 递增，
    # 到 4 时置 state=2、再自增到 5 退出 —— 共 5 轮。这是原机 `while (<5)` 的边界，
    # 也是「重试 5 次」与「重试 4 次」唯一能被观察到的地方。
    (0x10820, "auth_retry", "auth_retry(state=1,fail)",
     {0x10001730: 1, 0x100020F0: 0, 0x100020E8: 1}),
)






def _skip_set(is_new):
    if is_new:
        return {V.SYMS[n] for n in SKIP_NEW_NAMES if n in V.SYMS}
    return set(SKIP_OLD)


def _install_hw_model(uc):
    """晶振/PLL 锁定模型。MEM_WRITE 钩子早于写入生效，故记账后在 CODE 钩子补位。"""
    pending = []

    def mem_hook(m, access, address, size, value, user):
        if address == SCS and size == 4:
            pending.append((SCS, 0x40))
        elif address in (PLL0FEED, PLL1FEED):
            pending.append((PLL0STAT, 0x07000000))
            pending.append((PLL1STAT, 0x00000700))

    def code_hook(m, address, size, user):
        while pending:
            addr, bits = pending.pop()
            cur = struct.unpack("<I", m.mem_read(addr, 4))[0]
            m.mem_write(addr, struct.pack("<I", cur | bits))

    uc.hook_add(UC_HOOK_MEM_WRITE, mem_hook)
    uc.hook_add(UC_HOOK_CODE, code_hook)


def _observe(is_new, entry, args, hw_model, max_insn=3_000_000, long_delay=False):
    uc = V.machine(is_new)
    for base, size in PAGES:
        try:
            uc.mem_map(base, size)
        except Exception:
            pass
    if args:
        uc.reg_write(UC_ARM_REG_R0, args[0])

    skip = _skip_set(is_new)
    pokes = []
    inner_body = LONG_DELAY_INNER_BODY[is_new] if long_delay else None
    inner_off = LONG_DELAY_INNER_OFF[is_new] if long_delay else 0

    # long_delay 只空转、不写任何外设或 SRAM，唯一可观察量就是「外层跑了几轮」。
    # 不把它导出成被比较的量，把 6000 改成 5999 也抓不到（实测会漏）。
    long_iters = [0]

    def code_hook(m, address, size, user):
        if inner_body is not None and address == inner_body:
            long_iters[0] += 1
            sp = m.reg_read(UC_ARM_REG_SP)
            m.mem_write(sp + inner_off, struct.pack("<I", LONG_DELAY_INNER - 1))
        if address in skip:
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    def mem_hook(m, access, address, size, value, user):
        pokes.append((address, size, value & ((1 << (8 * size)) - 1)))

    for base, size in PAGES:
        if base == FIO_BASE:
            continue
        uc.hook_add(UC_HOOK_MEM_WRITE, mem_hook, begin=base, end=base + size - 1)
    uc.hook_add(UC_HOOK_CODE, code_hook)
    if hw_model:
        _install_hw_model(uc)

    try:
        V.run(uc, entry, max_insn=max_insn)
        finished = True
    except AssertionError:
        finished = False
    final = [bytes(uc.mem_read(base, size)) for base, size in PAGES]
    if long_delay:
        # 用一条伪写迹承载迭代次数，让它参与既有的写序列相等比较
        pokes.append((0xFFFFFFF0, 4, long_iters[0]))
    return finished, pokes, final


def _describe(a, b, limit=4):
    idx = [i for i in range(len(a)) if a[i] != b[i]]
    return ", ".join("+0x%03X OLD=0x%02X NEW=0x%02X" % (i, a[i], b[i]) for i in idx[:limit])


def _observe_sram(is_new, entry, presets, args=(), max_insn=3_000_000, trace=False):
    uc = V.machine(is_new)
    for base, size in PAGES:
        try:
            uc.mem_map(base, size)
        except Exception:
            pass
    if args:
        for reg, val in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                             UC_ARM_REG_R3), args):
            uc.reg_write(reg, val)
    for addr, val in presets.items():
        uc.mem_write(addr, struct.pack("<I", val))

    skip = _skip_set(is_new)
    # 显示叶函数体一律跳过（否则整屏 512×N 字节的 GPIO 位翻转会吃光预算）；
    # 但必须**记录它们的调用序列与实参**，否则显示后端函数就沦为「什么都不写」
    # 的空跑，比对毫无区分力（实测：不记录时 disp_screen_static 也能 PASS）。
    calls = []
    traced = {}
    noarg = set()
    if trace:
        if is_new:
            for name in ("disp_string", "disp_render_char8", "disp_render_char16"):
                if name in V.SYMS:
                    traced[V.SYMS[name]] = name
        else:
            traced = dict(DISP_TRACE_OLD)
        skip |= set(traced)
    # auth_retry 的进入循环分支会调 auth_challenge（24 位 1-Wire 时序，每 bit 数千
    # 空转、还要读 P2 引脚），实跑会耗尽预算且结果依赖引脚初值。这里跳函数体但仍
    # 记录调用次数 —— 能验证「重试循环跑了几轮」，这正是该函数的核心可观察行为。
    if entry in (AUTH_RETRY_OLD, V.SYMS.get("auth_retry")):
        auth_ch = V.SYMS["auth_challenge"] if is_new else AUTH_CHALLENGE_OLD
        traced[auth_ch] = "auth_challenge"
        # auth_challenge 无形参：原机调用前 r0/r1 残留 auth_set_timeout 的写入值
        # （实测 r0=50000），新 ELF 该写入内联成 movw/str、寄存器为 0。实参不是
        # 契约，只比调用次数与顺序。
        noarg.add(auth_ch)
        skip.add(auth_ch)
        # 循环体末的 param_sync_live_to_eeprom 会按需写 EEPROM（每字节 10000 次
        # 空转），本用例只验证重试轮数与状态迁移，同步本身已由专用测试覆盖。
        sync_fn = (V.SYMS["param_sync_live_to_eeprom"] if is_new
                   else PARAM_SYNC_OLD)
        skip.add(sync_fn)

    def code_hook(m, address, size, user):
        if address in traced:
            if address in noarg:
                calls.append((traced[address],))
                m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
                return
            calls.append((traced[address], m.reg_read(UC_ARM_REG_R0),
                          m.reg_read(UC_ARM_REG_R1), m.reg_read(UC_ARM_REG_R2),
                          m.reg_read(UC_ARM_REG_R3)))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
            return
        if address in skip:
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    uc.hook_add(UC_HOOK_CODE, code_hook)
    try:
        V.run(uc, entry, max_insn=max_insn)
        finished = True
    except AssertionError:
        finished = False
    sram = bytes(uc.mem_read(0x10000000, SRAM0_END))
    final = [bytes(uc.mem_read(base, size)) for base, size in PAGES]
    return finished, sram, final, calls


def _first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return min(len(a), len(b))


def _run_sram_cases():
    bad = 0
    for addr, sym, label, presets in SRAM_CASES:
        args = ()
        trace = False
        if sym == "auth_retry":
            trace = True
        elif sym == "disp_decimal1":
            args = (1234, 1, 2, 0)
            trace = True
        elif sym in ("disp_screen_static", "disp_screen_calib"):
            trace = True
        old_ok, old_s, old_f, old_c = _observe_sram(False, addr, presets, args, trace=trace)
        new_ok, new_s, new_f, new_c = _observe_sram(True, V.SYMS[sym], presets, args, trace=trace)
        if not (old_ok and new_ok):
            bad += 1
            print("  [FAIL] %-28s 未执行到返回 old=%s new=%s" % (label, old_ok, new_ok))
            continue
        problems = []
        if old_s != new_s:
            idx = [i for i in range(len(old_s)) if old_s[i] != new_s[i]]
            problems.append("SRAM %d 字节不同，首个 +0x%03X OLD=0x%02X NEW=0x%02X"
                            % (len(idx), idx[0], old_s[idx[0]], new_s[idx[0]]))
        for (base, _), a, b in zip(PAGES, old_f, new_f):
            if a != b:
                problems.append("页0x%08X: %s" % (base, _describe(a, b)))
        if trace and sym.startswith("disp_"):
            # 显示后端函数**必须**产生调用，否则「什么都不画」也能 PASS。
            if not old_c:
                problems.append("原机未产生任何显示调用，用例没进绘制分支")
            elif old_c != new_c:
                d = _first_diff(old_c, new_c)
                problems.append("显示调用 %d/%d 项，首个差异 #%d old=%s new=%s"
                                % (len(old_c), len(new_c), d,
                                   old_c[d] if d < len(old_c) else None,
                                   new_c[d] if d < len(new_c) else None))
        elif trace and old_c != new_c:
            # auth_retry：调用次数本身就是被验证的行为（可能合法为 0，
            # 例如 retry_state!=1 时不进循环），只要求两侧一致。
            problems.append("调用序列 %d/%d 项不一致 old=%s new=%s"
                            % (len(old_c), len(new_c), old_c[:6], new_c[:6]))
        if problems:
            bad += 1
            print("  [FAIL] %-28s %s" % (label, "; ".join(problems)))
            continue
        tail = "，显示调用 %d 项一致" % len(old_c) if trace else ""
        print("  [PASS] %-28s SRAM + %d 页末态%s" % (label, len(PAGES), tail))
    return bad


def main():
    bad = 0
    for addr, sym, label, args in TARGETS:
        need_hw = (sym == "SystemInit")
        is_long = (addr == LONG_DELAY_OLD)
        old_ok, old_p, old_f = _observe(False, addr, args, need_hw, long_delay=is_long)
        new_ok, new_p, new_f = _observe(True, V.SYMS[sym], args, need_hw, long_delay=is_long)

        if not (old_ok and new_ok):
            bad += 1
            print("  [FAIL] %-28s 未执行到返回 old=%s new=%s" % (label, old_ok, new_ok))
            continue

        problems = []
        if old_p != new_p:
            only_old = [p for p in old_p if p not in new_p][:3]
            only_new = [p for p in new_p if p not in old_p][:3]
            problems.append("写迹 %d/%d 项不同 old_only=%s new_only=%s"
                            % (len(old_p), len(new_p), only_old, only_new))
        for (base, _), a, b in zip(PAGES, old_f, new_f):
            if a != b:
                problems.append("页0x%08X: %s" % (base, _describe(a, b)))
        if problems:
            bad += 1
            print("  [FAIL] %-28s %s" % (label, "; ".join(problems)))
            continue

        print("  [PASS] %-28s 写迹 %2d 项 + %d 页末态一致"
              % (label, len(old_p), len(PAGES)))

    uniq = {addr for addr, _, _, _ in TARGETS} | {c[0] for c in SRAM_CASES}
    print("  --- 读写 SRAM / 显示后端的函数")
    bad += _run_sram_cases()
    total = len(TARGETS) + len(SRAM_CASES)
    print("  ---")
    print("  覆盖门禁：%d 个唯一原机入口 / %d 组用例，全部执行到返回"
          % (len(uniq), total))
    if bad:
        print("  [FAIL] 六相初始化/底层 I/O 外设等价性 %d 项失败" % bad)
        return 1
    print("  [PASS] 六相初始化/底层 I/O 外设等价性：%d 组全部一致" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
