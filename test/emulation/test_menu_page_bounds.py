# -*- coding: utf-8 -*-
"""保护参数页（屏号 4）分页上界的调用级回归。

原 BIN 保护页只有 3 页：项号 0..3、4..7、8..11；项号 >= 12 时既不到第 3 页、
也不刷新任何标题（0x84FC `cmp #8; blt` / 0x8504 `cmp #0xc; bge` → 0x85C0 跳过）。
本仓源码原用无条件 `else` 兜底，项号 >= 12 时仍画第 3 页标题与数值，
与 PC12M-2 同源缺陷（12p 已修为 `else if (item < 0xc)`）。

用例：项号 8..16 × view 0/1 × 导航键（下/上/快加/快减），比较两版显示调用序列；
并额外断言项号 >= 12 时新固件与原 BIN 一样不产生任何显示调用。
"""
import os
import sys

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import test_display_call_equivalence as D

PROTECTION_SCREEN = 4
NAV_KEYS = (2, 3, 0x16, 0x21)


def cases():
    out = []
    for item in range(8, 17):
        for view in (0, 1):
            for key in NAV_KEYS:
                out.append((PROTECTION_SCREEN, item, view, 0xFA, D.BASE, key))
    return out


def main():
    failed = 0
    total = 0
    for case in cases():
        total += 1
        old = D.trace(False, *case)
        new = D.trace(True, *case)
        if old != new:
            failed += 1
            print("  [FAIL] case=%s OLD=%s NEW=%s" % (case[:3] + (case[5],), old[:3], new[:3]))
            continue
        item = case[1]
        key = case[5]
        # 键 2/3 会先把项号钳位（12→11、12→9），只有不改变项号的快加/快减能直接验证上界
        if item >= 0xc and key in (0x16, 0x21) and new:
            failed += 1
            print("  [FAIL] item=%d 原 BIN 无绘制，新固件却有 %d 次调用" % (item, len(new)))
    print("[menu_page_bounds] 保护页分页上界 %d/%d" % (total - failed, total))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
