"""相位显示按原 BIN 的 LDRB 读取：显示低字节，编辑/存储仍为整字。

原机 0x7752/0x7762、0x7A16/0x7A26、0x5F58/0x5F68。
高字节与低字节刻意不同，避免仅测 0..180 时错误的整字读取仍通过。
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_display_call_equivalence import trace, BASE

def main():
    failures = 0
    count = 0
    for menu, item, address in ((3, 8, 0x10001650), (3, 15, 0x10001660), (0x63, 10, 0x10001660),
                                (0xa, 0, 0x10001760), (0x62, 0, 0x10001760)):
        for value in (0, 180, 255, 256, 0x12345678, 0x7fffffff, 0x80000000, 0xffffffff):
            params = dict(BASE)
            params[address] = value
            if menu in (0xa, 0x62):
                params[0x10001768] = 499  # 本帧刷新并先将倒计时减一
            a = trace(False, menu, item, 0, 0xfa, params)
            b = trace(True, menu, item, 0, 0xfa, params)
            numeric = [c for c in a if c[0] == 'disp_number3']
            assert numeric, '未实际进入相位绘制分支'
            expected = ((value - 1) if menu in (0xa, 0x62) else value) & 255
            assert any(c[1] == expected for c in numeric), '原 BIN 低字节读取未命中'
            count += 1
            if a != b:
                failures += 1
                if failures <= 3: print('DISPLAY_BYTE_DIFF', menu, item, hex(value), a, b)
    print('DISPLAY_BYTE_READS cases=%d failures=%d' % (count, failures))
    return int(bool(failures))

if __name__ == '__main__':
    raise SystemExit(main())
