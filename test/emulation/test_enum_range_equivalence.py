# -*- coding: utf-8 -*-
"""枚举越界时的绘制契约：原 BIN 在控制方式/校验位/通信校验越界时不画该行选项。

用户实测（2026-11-17）指出：COM_CHK 为 2、3 等越界值时原版不绘制选项，而旧源码的
`if (...) 开启 else 关闭` 会把任意非 1 值都画成"关闭"。同类问题还有校验位 >=4、
控制方式 >=3、控制手法 >=3、急停 >=3。本用例把这些值逐个灌进状态并比较两版绘制序列，
不比较 SRAM（越界值只影响绘制，不影响状态机行为）。
"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from test_display_call_equivalence import BASE, trace  # noqa: E402

PARITY = 0x10001704
COM_CHK = 0x10001705
CONTROL_MODE = 0x10001634
CONTROL_METHOD = 0x10001655
ESTOP = 0x10001657


def cases():
    out = []
    # 通信页：校验位与通信校验选项（菜单 5，item 2/3）
    for value in range(5):
        for item in (2, 3):
            params = dict(BASE)
            params[PARITY] = value
            params[COM_CHK] = value
            out.append(("parity/comchk=%d item=%d" % (value, item), 5, item, 0, 0xFA, params))
    # 基本参数页：通信校验在选项 3 的入口块（菜单 3，item 3）
    for value in range(5):
        params = dict(BASE)
        params[COM_CHK] = value
        out.append(("entry comchk=%d" % value, 3, 3, 0, 0xFA, params))
    # 控制方式 / 控制手法 / 急停 越界
    for value in range(4):
        for address, item in ((CONTROL_MODE, 0), (CONTROL_METHOD, 10), (ESTOP, 12)):
            params = dict(BASE)
            params[address] = value
            out.append(("enum 0x%X=%d item=%d" % (address, value, item), 3, item, 0, 0xFA, params))
    return out


def main():
    failed = 0
    total = 0
    empty = 0
    for tag, menu, item, view, stats, params in cases():
        old = trace(False, menu, item, view, stats, params)
        new = trace(True, menu, item, view, stats, params)
        total += 1
        if not old and not new:
            empty += 1
        if old != new:
            failed += 1
            print("  [FAIL] %s" % tag)
            print("     OLD:", old)
            print("     NEW:", new)
    print("  [%s] 枚举越界绘制 %d/%d（双方都空 %d）"
          % ("PASS" if not failed else "FAIL", total - failed, total, empty))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
