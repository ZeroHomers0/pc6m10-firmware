/* =============================================================================
 * data_image.s — 原始固件 SRAM0 .data 初始镜像（IAR 压缩流解压产物）
 * 来源：tools/decompress_dump.py → docs/_data_image.bin（0x213C 字节）
 * 链接进 .fw_image 段（VMA=0x10000000），startup.s 拷贝到 SRAM0。
 * ========================================================================== */
    .section .fw_image_data, "a", %progbits
    .global _ram_image_start
    .type   _ram_image_start, %object
_ram_image_start:
    .incbin "D:/code/LPC1765FBD100/decompiled/docs/_data_image.bin"
    .global _ram_image_end
    .type   _ram_image_end, %object
_ram_image_end:
    .size   _ram_image_start, . - _ram_image_start
    .end
