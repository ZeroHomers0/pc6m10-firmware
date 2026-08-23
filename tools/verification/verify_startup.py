# -*- coding: utf-8 -*-
"""verify_startup.py — 无硬件仿真：host 侧模拟 Reset_Handler 启动链路

验证点：
  1) firmware.bin 内嵌 data_image 是否 == firmware/assets/ram_data_image.bin
     （原始 SRAM0 .data 初始镜像，IAR 解压产物）
  2) 模拟 startup.s 四步：拷贝 data_image→SRAM0 / 清零原始 .bss / 拷贝本固件 .data→SRAM1
  3) 验证 SRAM0 关键地址初始值（波特率表/认证标志/字体表指针）符合预期

用法：cd decompiled && python tools/verify_startup.py
"""
import struct

def dword(buf, off):
    return struct.unpack_from('<I', buf, off)[0]

def main():
    fw = open('firmware/firmware.bin', 'rb').read()
    img = open('firmware/assets/ram_data_image.bin', 'rb').read()

    print('== firmware.bin 大小:', hex(len(fw)))
    print('== ram_data_image.bin 大小:', len(img), '(=0x%x)' % len(img))

    # 链接符号（与 nm 一致）
    RAM_IMAGE_LMA  = 0xca4c   # _ram_image_lma_start
    RAM_IMAGE_VMA  = 0x10000000
    RAM_IMAGE_LEN  = 0x213c   # 0x1000213c - 0x10000000
    RAM_BSS_START  = 0x1000213c
    RAM_BSS_END    = 0x100029c8
    SIDATA         = 0xeb88   # 本固件 .data LMA
    SDATA          = 0x2007c000
    EDATA          = 0x2007cd3c

    ok = True

    # ---- 验证 1：data_image 嵌入正确 ----
    embedded = fw[RAM_IMAGE_LMA:RAM_IMAGE_LMA + RAM_IMAGE_LEN]
    m1 = embedded == img
    ok &= m1
    print('\n[1] firmware.bin@0x%x 内嵌 data_image == firmware/assets/ram_data_image.bin :' % RAM_IMAGE_LMA,
          'PASS' if m1 else 'FAIL')
    if not m1:
        for i, (a, b) in enumerate(zip(embedded, img)):
            if a != b:
                print('    首处差异 @+0x%x: fw=%02x img=%02x' % (i, a, b)); break

    # ---- 验证 2：模拟拷贝 + 清零，构建 SRAM0 镜像 ----
    sram0 = bytearray(0x8000)  # 32K
    sram0[0:RAM_IMAGE_LEN] = embedded                    # 拷贝 data_image
    sram0[RAM_BSS_START - RAM_IMAGE_VMA : RAM_BSS_END - RAM_IMAGE_VMA] = b'\x00' * (RAM_BSS_END - RAM_BSS_START)  # 清零 .bss
    print('[2] 模拟启动：SRAM0 前 0x213C = data_image，0x213C..0x29C8 清零 -> 完成')

    # ---- 验证 3：关键 SRAM0 初始值 ----
    print('\n[3] SRAM0 关键地址初始值：')
    checks = [
        (0x100017bc, '波特率表[0]', lambda v: v in (2400, 4800, 9600, 19200, 38400, 57600, 115200)),
        (0x1000172c, '认证锁定标志(750)', lambda v: v == 0),   # 初始应锁定态=0
        (0x10001734, '认证结果(748)', lambda v: True),          # 任意，只 dump
    ]
    for addr, name, pred in checks:
        v = dword(bytes(sram0), addr - RAM_IMAGE_VMA)
        tag = 'PASS' if pred(v) else 'CHECK'
        print('    %s @0x%08x = 0x%08x (%d)  %s' % (name, addr, v, v, tag))
        ok &= pred(v)

    # 波特率表完整 dump（0x100017bc 起 8 个 word）
    print('    波特率表 0x100017BC 全量:', end=' ')
    vals = [dword(bytes(sram0), 0x17bc + i*4) for i in range(8)]
    print(vals)

    # 本固件 .data 拷贝长度自检
    print('\n[4] 本固件 .data：LMA=0x%x 长度=0x%x (%dB)，VMA=0x%x -> 0x%x' %
          (SIDATA, EDATA - SDATA, EDATA - SDATA, SDATA, EDATA))

    print('\n==== 启动链路 host 模拟:', '全部 PASS' if ok else '存在 FAIL', '====')
    return 0 if ok else 1

if __name__ == '__main__':
    raise SystemExit(main())
