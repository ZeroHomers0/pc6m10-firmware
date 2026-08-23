/* =============================================================================
 * lpc1765.ld — LPC1765 链接脚本（firmware/lpc1765.ld）
 *
 * 三个内存区：
 *   FLASH : 0x0, 256K（代码 + 只读 + SRAM0 镜像的 LMA）
 *   SRAM0 : 0x10000000, 32K —— 原固件 SRAM0 使用区：
 *           0x10000000-0x1000213C 放 .data 初始镜像（fw_image，LMA 在 flash）
 *           0x1000213C-0x100029C8 为原 .bss 清零区（fw_bss）
 *           _estack=0x100029C8（原始复位向量栈顶）
 *   SRAM1 : 0x2007C000, 16K —— 本固件编译期全局（DAT_ 指针/值变量）
 *           （与匿名 SRAM0 目标物理分离，避免覆盖 DAT_ 指针指向的变量区）
 * ========================================================================== */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  SRAM0 (rwx) : ORIGIN = 0x10000000, LENGTH = 32K
  SRAM1 (rwx) : ORIGIN = 0x2007C000, LENGTH = 16K
}

_estack = 0x100029C8;

/* Cortex-M 函数向量含 Thumb bit，链接符号本身为偶地址，故补六个 +1。 */
_vector_checksum = 0 - (_estack + Reset_Handler + NMI_Handler + HardFault_Handler +
                        MemManage_Handler + BusFault_Handler + UsageFault_Handler + 6);

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    *(.text*)            /* 含 Reset_Handler */
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  /* ── 原始固件 SRAM0 .data 镜像：VMA 固定 0x10000000，LMA 在 flash ── */
  . = 0x10000000;
  .fw_image :
  {
    _ram_image_vma_start = .;
    KEEP(*(.fw_image_data))          /* data_image.s 的 .incbin */
    _ram_image_vma_end = .;
  } > SRAM0 AT > FLASH
  _ram_image_lma_start = LOADADDR(.fw_image);

  /* ── 原始 .bss 区：0x1000213C-0x100029C8 清零 ──
   * 注意：段内 "." 赋值是相对段起点的偏移，故 . = 0x88C（=0x29C8-0x213C）；
   * 符号用 ABSOLUTE() 取绝对。 */
  . = ORIGIN(SRAM0) + 0x213C;
  .fw_bss (NOLOAD) :
  {
    _ram_bss_start = ABSOLUTE(.);
    . = 0x88C;
    _ram_bss_end = ABSOLUTE(.);
  } > SRAM0

  /* ── 本固件编译期全局（SRAM1），.data 带 LMA ── */
  .data :
  {
    _sdata = .;
    *(.data*)
    *(.data.*)
    . = ALIGN(4);
    _edata = .;
  } > SRAM1 AT > FLASH
  _sidata = LOADADDR(.data);

  .bss (NOLOAD) :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > SRAM1
}
