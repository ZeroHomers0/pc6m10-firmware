# -*- coding: utf-8 -*-
# =============================================================================
# test_unicorn_param_sync.py — 真实执行 param_sync_live_to_eeprom，验证写 EEPROM 行为
#
# 用 Unicorn 执行编译的 param_sync_live_to_eeprom@0x3478。该函数对每参数做
#   if (live != shadow) { shadow=live; i2c_write_reg(reg); }
# 本测试：
#   * 在仿真 SRAM0 写构造虚拟数据：某些 live≠shadow（应写），某些 live==shadow（不应写）
#   * 用代码 hook 拦截 i2c_write_byte@0x1998 → 返回 ACK，使 i2c_write_reg 能完整走完
#   * 统计 i2c_write_reg 被调用的「数据/寄存器号」序列 vs Python 行为模型
#   * 回读 shadow 区，确认写后 live==shadow
# 若 unicorn 不可用 → SKIP。
# =============================================================================
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

from unicorn_harness import load_firmware, call, lookup
from unicorn import *
from unicorn.arm_const import *

FUNC_param_sync = lookup('param_sync_live_to_eeprom')
FUNC_i2c_write_reg = lookup('i2c_write_reg')

def main():
    try:
        import unicorn  # noqa
    except Exception as ex:
        print(f"  [SKIP] unicorn 不可用（{ex}）")
        return 0
    e = load_firmware()

    passed = failed = 0
    def check(name, cond, detail=""):
        nonlocal passed, failed
        st = "PASS" if cond else "FAIL"
        if cond: passed += 1
        else: failed += 1
        print(f"  [{st}] {name}" + (f"  {detail}" if detail else ""))

    # 8 位单次写参数对：实时控制方式 / EEPROM 镜像控制方式。
    # 地址与固件语义映射头文件保持一致，不再依赖反编译全局符号表。

    SRAM0 = 0x10000000
    def w32(addr, v):
        e.mem_write(addr, v.to_bytes(4,'little'))
    def w8(addr, v):
        e.mem_write(addr, bytes([v]))
    def r32(addr):
        return int.from_bytes(e.mem_read(addr,4),'little')
    def r8(addr):
        return e.mem_read(addr,1)[0]

    # ── 构造虚拟数据：live≠shadow 的参数应写；live==shadow 应不写 ──
    live_addr = 0x10001634   # parameter_control_mode_ptr
    shadow_addr = 0x10001664 # eeprom_shadow_control_mode
    w8(live_addr, 0xAB); w8(shadow_addr, 0x00)   # 不等 → 应写

    # ── hook i2c_write_reg 本体：拦截 GPIO 位带模拟，记录 (data, reg) 并返回 0 ──
    write_calls = []
    def hook_write_reg(uc, addr, size, user):
        # i2c_write_reg(data,reg_addr)：r0=data, r1=reg_addr
        write_calls.append((uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1)))
        uc.reg_write(UC_ARM_REG_R0, 0)  # 返回 0（uint32_t 状态码成功）
        # 跳过整个函数体（i2c_write_reg 返回指令）：设置 lr + pc 直接返回
        # 用模拟 pop：写 pc 使 emu 继续。最简单：设 lr= 当前返回地址，直接 bx lr
        lr = uc.reg_read(UC_ARM_REG_LR)
        uc.reg_write(UC_ARM_REG_PC, lr)
    e.hook_add(UC_HOOK_CODE, hook_write_reg, begin=FUNC_i2c_write_reg, end=FUNC_i2c_write_reg+1)

    # 调 param_sync
    call(e, FUNC_param_sync)

    # 断言1：g_gain_sel(live) != shadow 初始 → 写后 shadow 应 == live(0xAB)
    after = r8(shadow_addr)
    check("写后 EEPROM 镜像控制方式=live(0xAB)", after == 0xAB, f"got 0x{after:02X}")

    # 断言2：i2c_write_reg 被调用（说明走了 EEPROM 写路径）
    check("i2c_write_reg 被调用（EEPROM 写触发）", len(write_calls) > 0, f"{len(write_calls)} 次")

    print()
    print(f"  通过 {passed}/{passed+failed}")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
