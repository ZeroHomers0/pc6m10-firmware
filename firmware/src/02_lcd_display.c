/* =============================================================================
 * src/02_lcd_display.c — 反编译模块 02（12864 LCD 显示）可编译副本
 * 目标B 阶段4 修正（对照反汇编 0x7A8/0x8F4/0x94A/0xB44/0xD3C）：
 *   1) Delay(int) = 延时×50（0x7A8），定义在 01_startup.c:418 → extern。
 *   2) disp_data 真实 2 参 (uint8_t, invert)；disp_cmd 真实 1 参 —— Ghidra 多造了参数
 *      与多余寄存器参数，已按反汇编还原（RS=1/0 → Delay → lcd_data_byte → E 脉冲）。
 *   3) disp_render_char8 4 参 / disp_render_char16 5 参 / disp_string 4 参：
 *      disp_string 内 char8 调用去掉多余实参、char16 调用补 invert 实参。
 *   4) 字符串资源统一通过语义地址映射访问（flash GBK 数据区，XIP 直读）。
 *      splash/static/calib 屏 disp_string 3 参调用的第 4 参 invert 反汇编未显式
 *      设置，暂取 0（正写），待 W7 对照 0x427C 反汇编复核。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include <stdbool.h>

/* 0x000007B6 —— LCD 背光/控制线：on<1 → 关闭（CLR bit24），否则开启（SET bit24）
 * （state_machine 屏幕超时>5000 拍后调 lcd_ctrl_line(0) 关闭；非 RS！RS 实为 bit27）
 * （LCD GPIO 池基址为 0x2009C000；+0x3C=CLR / +0x38=SET） */
void lcd_ctrl_line(int on)
{
  if (on < 1) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x1000000;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x1000000;
  }
  return;
}

/* 0x000007D6 —— 写一个字节到数据线 DB0-7（P1.0-7），按位 SET/CLR */
void lcd_data_byte(uint32_t byte_val)
{
  if ((byte_val & 0x80) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x8000;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x8000;
  }
  if ((byte_val & 0x40) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x4000;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x4000;
  }
  if ((byte_val & 0x20) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x400;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x400;
  }
  if ((byte_val & 0x10) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x200;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x200;
  }
  if ((byte_val & 8) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x100;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x100;
  }
  if ((byte_val & 4) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x10;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x10;
  }
  if ((byte_val & 2) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 2;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 2;
  }
  if ((byte_val & 1) == 0) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 1;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 1;
  }
  return;
}

/* 0x000008F4 —— 写数据（RS=1，P1.27），invert==0 正写 / else 反写（^0xFF）
 * 反汇编核实：实参 2 个（byte_val, invert）；RS=1 → Delay(1) → lcd_data_byte(byte_val[^0xFF])
 *   → Delay(1) → E=1 → Delay(1) → E=0 → Delay(1)。原反编译器生成的多余参数为伪影。 */
void disp_data(uint32_t byte_val,int invert)
{
  *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x8000000;  /* P1.27 RS=1（数据模式） */
  Delay(1);
  if (invert == 0) {
    lcd_data_byte(byte_val);
  }
  else {
    lcd_data_byte(byte_val ^ 0xff);
  }
  Delay(1);
  *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x10000000;  /* P1.28 E=1 */
  Delay(1);
  *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x10000000;  /* P1.28 E=0 */
  Delay(1);
  return;
}

/* 0x0000094A —— 写命令（RS=0，P1.27）—— 反汇编核实 1 实参 */
void disp_cmd(uint32_t cmd)
{
  *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x8000000;  /* P1.27 RS=0（命令模式） */
  Delay(1);
  lcd_data_byte(cmd);
  Delay(1);
  *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x10000000;  /* E=1 */
  Delay(1);
  *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x10000000;  /* E=0 */
  Delay(1);
  return;
}

/* 0x00000992 —— 清屏：上下两半各 8 页（0xB8..0xBF），每页 64 字节写 0 */
void disp_clear(void)
{
  uint32_t gpio_base;
  uint8_t col;
  uint8_t page;

  gpio_base = lcd_fio_base;
  *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x4000000;   /* CS1=1（上半屏） */
  *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x2000000;                 /* CS2=0 */
  Delay(10);
  disp_cmd(0xc0);                          /* 起始行=0 */
  for (page = 0; gpio_base = lcd_fio_base, page < 8; page = page + 1) {
    disp_cmd(page + 0xb8);                /* 页 0..7 */
    for (col = 0; col < 0x40; col = col + 1) {
      disp_data(0,0);
    }
  }
  *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x4000000;
  *(volatile uint32_t *)(gpio_base + 0x38) = *(volatile uint32_t *)(gpio_base + 0x38) | 0x2000000;
  Delay(10);
  disp_cmd(0xc0);
  for (page = 0; page < 8; page = page + 1) {
    disp_cmd(page + 0xb8);
    for (col = 0; col < 0x40; col = col + 1) {
      disp_data(0,0);
    }
  }
  return;
}

/* 0x00000A30 —— P1 口 GPIO 初始化 + LCD 上电初始化序列
 *   P1.0-7(DB)+P1.24-28(控制) 置输出；然后 CS 切换 + disp_cmd(0xC0)+disp_cmd(0x3F) 开显示 */
void gpio1_init(void)
{
  uint32_t gpio_base;

  gpio_base = lcd_fio_base;
  *(volatile uint32_t *)(lcd_fio_base + 0x20) = *(volatile uint32_t *)(lcd_fio_base + 0x20) | 0x8000000;   /* FIODIR bit27 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x10000000;                /* bit28 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x4000000;                 /* bit26 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x2000000;                 /* bit25 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 1;                         /* bit0 DB0 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 2;                         /* bit1 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x10;                      /* bit4 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x100;                     /* bit8 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x200;                     /* bit9 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x400;                     /* bit10 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x4000;                    /* bit14 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x8000;                    /* bit15 */
  *(volatile uint32_t *)(gpio_base + 0x20) = *(volatile uint32_t *)(gpio_base + 0x20) | 0x1000000;                 /* bit24 */
  *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x1000000;                 /* RES=0 */
  *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x8000000;                 /* R/W=0 */
  Delay(1);
  *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x10000000;  /* E=0 */
  Delay(1);
  gpio_base = lcd_fio_base;
  *(volatile uint32_t *)(gpio_base + 0x38) = *(volatile uint32_t *)(gpio_base + 0x38) | 0x4000000;
  *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x2000000;
  Delay(1);
  disp_cmd(0xc0);
  disp_cmd(0x3f);                          /* DISPLAY ON */
  Delay(1);
  gpio_base = lcd_fio_base;
  *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x4000000;
  *(volatile uint32_t *)(gpio_base + 0x38) = *(volatile uint32_t *)(gpio_base + 0x38) | 0x2000000;
  Delay(1);
  disp_cmd(0xc0);
  disp_cmd(0x3f);
  Delay(1);
  disp_clear();
  return;
}

/* ==================== 字符渲染 ==================== */

/* 扩展 8×8 字形：X 与 O —— 原厂字库仅 36 字符（map @0x10000BB8 无 X/O），产品信息
 * "厂商:XIANPOWER" 的 X/O 查表未命中会渲染空白（原厂串 SINEP0WER 用数字 0 因此无感）。
 * 本表为对本固件的有意扩展（用户 2026-09-01 明确授权，与 PC12M-2 方案C 同款）：
 * 与原字库同格式（每字 0x10 字节 = 8 列 × 2 页，列高字节 bit7=行首），
 * disp_render_char8 对 X 提前匹配。
 * 字形风格与原字库一致：字符使用 col0-6、有效像素位于行3-13、笔画 1px。
 * 'O' 直接复用原字库数字 '0'（原厂 SINEP0WER 即用 0 代替 O）。
 * 'X' 的两条斜线在行8、col3 真实相交；上下页只是 LCD 的 8 行分页，不是字形
 * 中间留白。此前把行5-9留空会把 X 画成菱形，已按原字库 N/A/V 的实际行高修正。
 * 注意：这是对原 BIN 的**有意定制偏离**（原 BIN 渲染 X/O 为空白），A/B 快照区
 * 无差异，仅"渲染 X/O 字符"的 GPIO 写迹与 OLD 不同（OLD 不写）。 */
static const unsigned char ext_char8_x[0x10] = {
    /* 'X' 上页（行3-7，两斜线收敛） */ 0x08,0x30,0xC0,0x00,0xC0,0x30,0x08,0x00,
    /* 'X' 下页（行8中心相交，行9-13发散） */ 0x20,0x18,0x06,0x01,0x06,0x18,0x20,0x00,
};

/* 0x00000B44 —— 渲染 8×8 ASCII 字符
 *   查表 0x10000BB8（0x24 个 8×8 字符映射表）→ 字形表 0x10000FC0（每字 0x10 字节）
 *   X 匹配扩展字形表（见 ext_char8_x）；O 复用原字库数字 0；其余查原字库。
 *   col<8 → 上半屏（CS1），否则下半屏（CS2，col-8）
 *   row=行，col=列×8+0x40 地址 */
void disp_render_char8(uint32_t ch,char row,uint32_t col,uint32_t invert)
{
  uint32_t gpio_base;
  uint32_t bit_index;
  uint32_t glyph_index;
  uint32_t glyph_base;

  gpio_base = lcd_fio_base;
  glyph_index = 0;
  if (ch == 0x4f) {                  /* 'O'：直接复用原字库索引0的数字'0' */
    glyph_index = 0;
    glyph_base = lcd_ascii_font;
  }
  else if (ch == 0x58) {             /* 'X'：原厂字库无 X，走扩展字形表 */
    glyph_index = 0;
    glyph_base = (uint32_t)ext_char8_x;
  }
  else {
    while( true ) {
      if (0x23 < glyph_index) {
        return;
      }
      if (*(volatile uint8_t *)(lcd_ascii_map + glyph_index) == ch) break;
      glyph_index = glyph_index + 1 & 0xff;
    }
    glyph_base = lcd_ascii_font;
  }
  if ((int)col < 8) {
    *(volatile uint32_t *)(lcd_fio_base + 0x3c) = *(volatile uint32_t *)(lcd_fio_base + 0x3c) | 0x4000000;
    *(volatile uint32_t *)(gpio_base + 0x38) = *(volatile uint32_t *)(gpio_base + 0x38) | 0x2000000;
  }
  else {
    *(volatile uint32_t *)(lcd_fio_base + 0x38) = *(volatile uint32_t *)(lcd_fio_base + 0x38) | 0x4000000;
    *(volatile uint32_t *)(gpio_base + 0x3c) = *(volatile uint32_t *)(gpio_base + 0x3c) | 0x2000000;
    col = col - 8 & 0xff;
  }
  Delay(10);
  disp_cmd(0xc0);
  disp_cmd(row * '\x02' + -0x48);      /* 页 = 行×2 - 0x48 */
  for (bit_index = 0; bit_index < 8; bit_index = bit_index + 1 & 0xff) {
    disp_cmd(col * 8 + 0x40 + bit_index & 0xff);   /* 列地址 */
    disp_data(*(volatile uint8_t *)(glyph_base + glyph_index * 0x10 + bit_index),invert);
  }
  disp_cmd(row * '\x02' + -0x47);      /* 下一行 */
  for (bit_index = 0; bit_index < 8; bit_index = bit_index + 1 & 0xff) {
    disp_cmd(col * 8 + 0x40 + bit_index & 0xff);
    disp_data(*(volatile uint8_t *)(glyph_base + glyph_index * 0x10 + bit_index + 8),invert);
  }
  return;
}

/* 0x00000C3A —— 渲染 16×16 汉字（GB 双字节）
 *   查表 0x10000FC4（0x8F 个汉字，每字 2 字节码）→ 字形 0x10000FCC（每字 0x20 字节）
 *   行占 2 页（上半字 + 下半字） */
void disp_render_char16(uint32_t gb_hi,uint32_t gb_lo,char row,int col,uint32_t invert)
{
  uint32_t glyph_base;
  uint32_t col2;
  uint32_t bit_index;
  uint32_t glyph_index;

  glyph_base = lcd_gbk_base;
  glyph_index = 0;
  col2 = (col - (col >> 0x1f) & 0x1ffU) >> 1;
  while( true ) {
    if (0x8e < glyph_index) {
      return;
    }
    if ((*(volatile uint8_t *)(lcd_gbk_map + glyph_index * 2) == gb_hi) &&
       (*(volatile uint8_t *)(lcd_gbk_map + glyph_index * 2 + 1) == gb_lo)) break;
    glyph_index = glyph_index + 1 & 0xff;
  }
  if (col2 < 4) {
    *(volatile uint32_t *)(lcd_gbk_base + 0x3c) = *(volatile uint32_t *)(lcd_gbk_base + 0x3c) | 0x4000000;
    *(volatile uint32_t *)(glyph_base + 0x38) = *(volatile uint32_t *)(glyph_base + 0x38) | 0x2000000;
  }
  else {
    *(volatile uint32_t *)(lcd_gbk_base + 0x38) = *(volatile uint32_t *)(lcd_gbk_base + 0x38) | 0x4000000;
    *(volatile uint32_t *)(glyph_base + 0x3c) = *(volatile uint32_t *)(glyph_base + 0x3c) | 0x2000000;
    col2 = col2 - 4 & 0xff;
  }
  Delay(10);
  disp_cmd(0xc0);
  disp_cmd(row * '\x02' + -0x48);
  for (bit_index = 0; bit_index < 0x10; bit_index = bit_index + 1 & 0xff) {
    disp_cmd(col2 * 0x10 + 0x40 + bit_index & 0xff);
    disp_data(*(volatile uint8_t *)(lcd_gbk_font + glyph_index * 0x20 + bit_index),invert);
  }
  disp_cmd(row * '\x02' + -0x47);
  for (bit_index = 0; bit_index < 0x10; bit_index = bit_index + 1 & 0xff) {
    disp_cmd(col2 * 0x10 + 0x40 + bit_index & 0xff);
    disp_data(*(volatile uint8_t *)(lcd_gbk_font + glyph_index * 0x20 + bit_index + 0x10),invert);
  }
  return;
}

/* 0x00000D3C —— 字符串渲染：逐字符，<0xA1 走 8×8 半角，>=0xA1 走 16×16 全角（双字节）
 * W7a：反编译把 str_addr 直传**原固件 flash 字符串地址**（如 0x47dc），GCC 重链接后该
 *   地址是指令字节。此处经 strpool_map 把 flash 地址映射到内嵌 GBK blob 偏移；未命中
 *   （RAM/外设地址）原样返回。见 src/strpool.c。 */
extern uint32_t strpool_map(uint32_t addr);
void disp_string(int str_addr,uint32_t row,uint32_t col,uint32_t invert)
{
  uint32_t i;
  uint32_t invert_tmp;   /* Ghidra 死存储伪影：仅赋值未读，保留（invert 实参直接下发） */

  str_addr = (int)strpool_map((uint32_t)str_addr);
  invert_tmp = invert;     /* 死存储，无逻辑影响 */
  for (i = 0; (i < 0x10 && (*(volatile char *)(str_addr + i) != '\0')); i = i + 1 & 0xff)
  {
    if (*(volatile uint8_t *)(str_addr + i) < 0xa1) {
      disp_render_char8(*(volatile uint8_t *)(str_addr + i),row,col,invert);
      col = col + 1;
    }
    else {
      invert_tmp = invert;
      disp_render_char16(*(volatile uint8_t *)(str_addr + i),*(volatile uint8_t *)(str_addr + i + 1),
                         row,col,invert);
      col = col + 2;
      i = i + 1 & 0xff;
    }
    col = col & 0xff;
  }
  return;
}

/* ==================== 数字/数值格式化 ==================== */

/* 0x00000D94 —— 单个数字 0..9 渲染 */
void disp_digit(uint32_t digit,uint32_t row,uint32_t col,uint32_t invert)
{
  switch(digit) {
  case 0:
    disp_render_char8(0x30,row,col,invert);
    break;
  case 1:
    disp_render_char8(0x31,row,col,invert);
    break;
  case 2:
    disp_render_char8(0x32,row,col,invert);
    break;
  case 3:
    disp_render_char8(0x33,row,col,invert);
    break;
  case 4:
    disp_render_char8(0x34,row,col,invert);
    break;
  case 5:
    disp_render_char8(0x35,row,col,invert);
    break;
  case 6:
    disp_render_char8(0x36,row,col,invert);
    break;
  case 7:
    disp_render_char8(0x37,row,col,invert);
    break;
  case 8:
    disp_render_char8(0x38,row,col,invert);
    break;
  case 9:
    disp_render_char8(0x39,row,col,invert);
  }
  return;
}

/* 0x00000E42 —— 3 位数字（前置空格补齐） */
void disp_number3(int val,uint32_t row,int col,uint32_t invert)
{
  if (val < 100) {
    disp_render_char8(0x20,row,col,invert);
  }
  else {
    disp_digit(val / 100 & 0xff,row,col,invert);
  }
  if (val < 10) {
    disp_render_char8(0x20,row,col + 1U & 0xff,invert);
  }
  else {
    disp_digit((val / 10) % 10 & 0xff,row,col + 1U & 0xff,invert);
  }
  disp_digit(val % 10 & 0xff,row,col + 2U & 0xff,invert);
  return;
}

/* 0x00000ED0 —— 4 位数字（前置空格补齐） */
void disp_uint4(uint32_t val,uint32_t row,int col,uint32_t invert)
{
  if (val < 1000) {
    disp_render_char8(0x20,row,col,invert);
  }
  else {
    disp_digit(val / 1000 & 0xff,row,col,invert);
  }
  if (val < 100) {
    disp_render_char8(0x20,row,col + 1U & 0xff,invert);
  }
  else {
    disp_digit((val / 100) % 10,row,col + 1U & 0xff,invert);
  }
  if (val < 10) {
    disp_render_char8(0x20,row,col + 2U & 0xff,invert);
  }
  else {
    disp_digit((val / 10) % 10,row,col + 2U & 0xff,invert);
  }
  disp_digit(val % 10,row,col + 3U & 0xff,invert);
  return;
}

/* 0x00000F8C —— 5 位数字 */
void disp_uint5(uint32_t val,uint32_t row,int col,uint32_t invert)
{
  if (val < 10000) {
    disp_render_char8(0x20,row,col,invert);
  }
  else {
    disp_digit(val / 10000 & 0xff,row,col,invert);
  }
  if (val < 1000) {
    disp_render_char8(0x20,row,col + 1U & 0xff,invert);
  }
  else {
    disp_digit((val / 1000) % 10,row,col + 1U & 0xff,invert);
  }
  if (val < 100) {
    disp_render_char8(0x20,row,col + 2U & 0xff,invert);
  }
  else {
    disp_digit((val / 100) % 10,row,col + 2U & 0xff,invert);
  }
  if (val < 10) {
    disp_render_char8(0x20,row,col + 3U & 0xff,invert);
  }
  else {
    disp_digit((val / 10) % 10,row,col + 3U & 0xff,invert);
  }
  disp_digit(val % 10,row,col + 4U & 0xff,invert);
  return;
}

/* 0x00001092 —— 6 位数字（最大位权除数为 0x000186A0） */
void disp_number(uint32_t val,uint32_t row,int col,uint32_t invert)
{
  if (lcd_max_decimal_divisor - 1 < val) {
    disp_digit(val / lcd_max_decimal_divisor & 0xff,row,col,invert);
  }
  else {
    disp_render_char8(0x20,row,col,invert);
  }
  if (val < 10000) {
    disp_render_char8(0x20,row,col + 1U & 0xff,invert);
  }
  else {
    disp_digit((val / 10000) % 10,row,col + 1U & 0xff,invert);
  }
  if (val < 1000) {
    disp_render_char8(0x20,row,col + 2U & 0xff,invert);
  }
  else {
    disp_digit((val / 1000) % 10,row,col + 2U & 0xff,invert);
  }
  if (val < 100) {
    disp_render_char8(0x20,row,col + 3U & 0xff,invert);
  }
  else {
    disp_digit((val / 100) % 10,row,col + 3U & 0xff,invert);
  }
  if (val < 10) {
    disp_render_char8(0x20,row,col + 4U & 0xff,invert);
  }
  else {
    disp_digit((val / 10) % 10,row,col + 4U & 0xff,invert);
  }
  disp_digit(val % 10,row,col + 5U & 0xff,invert);
  return;
}

/* 0x000011BC —— 有符号角度显示（±XX，主从偏移 100±60°）
 *   angle<100 → '-' + (100-angle)；angle>=100 → '+'（0 时不显示符号）+ (angle-100) */
void disp_signed_angle(int angle,uint32_t row,int col,uint32_t invert)
{
  uint32_t mag;

  if (angle < 100) {
    disp_render_char8(0x2d,row,col,invert);   /* '-' */
    mag = 100U - angle & 0xff;
    if (mag < 10) {
      disp_render_char8(0x20,row,col + 1U & 0xff,invert);
    }
    else {
      disp_digit((mag / 10) % 10,row,col + 1U & 0xff,invert);
    }
    disp_digit(mag % 10,row,col + 2U & 0xff,invert);
  }
  else {
    mag = angle - 100U & 0xff;
    if (mag == 0) {
      disp_render_char8(0x20,row,col,invert);
    }
    else {
      disp_render_char8(0x2b,row,col,invert);   /* '+' */
    }
    if (mag < 10) {
      disp_render_char8(0x20,row,col + 1U & 0xff,invert);
    }
    else {
      disp_digit((mag / 10) % 10,row,col + 1U & 0xff,invert);
    }
    disp_digit(mag % 10,row,col + 2U & 0xff,invert);
  }
  return;
}

/* 0x000012B0 —— 偏移量显示（中心 0x148=328，±显示；用于恢复出厂屏的偏移值） */
void disp_offset(uint32_t offset,uint32_t row,int col,uint32_t invert)
{
  if (offset < 0x148) {
    disp_render_char8(0x2b,row,col,invert);   /* '+' */
    offset = 0x148 - offset;
    if (offset < 100) {
      disp_render_char8(0x20,row,col + 1U & 0xff,invert);
    }
    else {
      disp_digit(offset / 100 & 0xff,row,col + 1U & 0xff,invert);
    }
    if (offset < 10) {
      disp_render_char8(0x20,row,col + 2U & 0xff,invert);
    }
    else {
      disp_digit((offset / 10) % 10,row,col + 2U & 0xff,invert);
    }
    disp_digit(offset % 10,row,col + 3U & 0xff,invert);
  }
  else {
    offset = offset - 0x148;
    if (offset == 0) {
      disp_render_char8(0x20,row,col,invert);
    }
    else {
      disp_render_char8(0x2d,row,col,invert);   /* '-' */
    }
    if (offset < 100) {
      disp_render_char8(0x20,row,col + 1U & 0xff,invert);
    }
    else {
      disp_digit(offset / 100 & 0xff,row,col + 1U & 0xff,invert);
    }
    if (offset < 10) {
      disp_render_char8(0x20,row,col + 2U & 0xff,invert);
    }
    else {
      disp_digit((offset / 10) % 10,row,col + 2U & 0xff,invert);
    }
    disp_digit(offset % 10,row,col + 3U & 0xff,invert);
  }
  return;
}

/* 0x000013E8 —— 2 位数字 */
void disp_uint2(uint32_t val,uint32_t row,int col,uint32_t invert)
{
  if (val < 10) {
    disp_render_char8(0x20,row,col,invert);
  }
  else {
    disp_digit(val / 10 & 0xff,row,col,invert);
  }
  disp_digit(val % 10,row,col + 1U & 0xff,invert);
  return;
}

/* 0x0000143C —— 固定 1 位小数（4 位整数 + '.' + 1 位小数），如 XX.X */
void disp_fixed_1dec(uint32_t val,uint32_t row,int col,uint32_t invert)
{
  if (val < 1000) {
    disp_render_char8(0x20,row,col,invert);
  }
  else {
    disp_digit(val / 1000 & 0xff,row,col,invert);
  }
  if (val < 100) {
    disp_render_char8(0x20,row,col + 1U & 0xff,invert);
  }
  else {
    disp_digit((val / 100) % 10,row,col + 1U & 0xff,invert);
  }
  disp_digit((val / 10) % 10,row,col + 2U & 0xff,invert);
  disp_render_char8(0x2e,row,col + 3U & 0xff,invert);   /* '.' */
  disp_digit(val % 10,row,col + 4U & 0xff,invert);
  return;
}

/* 0x000014F6 —— 1 位小数（2 位整数 + '.' + 1 位小数） */
void disp_decimal1(uint32_t val,uint32_t row,int col,uint32_t invert)
{
  if (val < 100) {
    disp_render_char8(0x20,row,col,invert);
  }
  else {
    disp_digit(val / 100 & 0xff,row,col,invert);
  }
  disp_digit((val / 10) % 10,row,col + 1U & 0xff,invert);
  disp_render_char8(0x2e,row,col + 2U & 0xff,invert);   /* '.' */
  disp_digit(val % 10,row,col + 3U & 0xff,invert);
  return;
}

/* ==================== 整屏渲染 ==================== */

/* 0x0000427C —— 开机画面（4 行：ST33C / 版本 V2.0.2016 / SINEP0WER / 电话）
 *   右上角显示增益档位值（gain_sel 对应增益，cfg_1710 组）；底部运行状态/子状态文字 */
void disp_splash_screen(void)
{
  disp_string((int)0x4370,0,0,0);
  disp_string((int)0x4384,1,0,0);
  disp_string((int)0x4398,2,0,0);
  disp_string((int)0x43ac,3,0,0);
  if (lcd_source_mode == '\0') {
    disp_fixed_1dec(lcd_source_value,0,9,0);   /* 反汇编 0x42B4 核：r3=0 → 4 参 */
  }
  if (lcd_source_mode == '\x01') {
    disp_fixed_1dec(lcd_source_value_alt_a,0,9,0);
  }
  if (lcd_source_mode == '\x02') {
    disp_fixed_1dec(lcd_source_value_alt_b,0,9,0);
  }
  disp_uint4(lcd_output_value_a,1,9,0);
  disp_uint4(lcd_output_value_b,2,9,0);
  if (lcd_display_selection == 0) {
    if (parameter_output_mode == '\0') {
      disp_string((int)0x47e8,3,10,0);
    }
    if (parameter_output_mode == '\x01') {
      disp_string((int)0x47f0,3,10,0);
    }
  }
  else {
    disp_string((int)0x47dc,3,10,0);
  }
  if (parameter_control_mode == '\0') {
    disp_string((int)0x47fc,3,0,0);
  }
  if (parameter_control_mode == '\x01') {
    disp_string((int)0x4804,3,0,0);
  }
  if (parameter_control_mode == '\x02') {
    disp_string((int)0x480c,3,0,0);
  }
  return;
}

/* 0x0000448A —— 静态信息屏（4 行静态文字，如版本信息页） */
void disp_screen_static(void)
{
  disp_clear();
  disp_string((int)0x4814,0,0,1);
  disp_string((int)0x4824,1,0,0);
  disp_string((int)0x4834,2,0,0);
  disp_string((int)0x4844,3,0,0);
  return;
}

/* 0x000044C2 —— 标定屏：4 行标定值 + 光标行高亮（ADC 标定除数显示；与相位校准相关）
 *   0x100048A4=光标行；0x100048A8/AC/B0/B4=4 个标定值 */
void disp_screen_calib(void)
{
  disp_string((int)0x4854,0,0,0);
  disp_string((int)0x4868,1,0,0);
  disp_string((int)0x487c,2,0,0);
  disp_string((int)0x4890,3,0,0);
  if (lcd_calibration_cursor == '\0') {
    disp_uint4(lcd_calibration_value_1,0,0xb,1);   /* 反汇编 0x44FA/0x4520 核：if 分支 r3=1、else r3=0 */
  }
  else {
    disp_uint4(lcd_calibration_value_1,0,0xb,0);
  }
  if (lcd_calibration_cursor == '\x01') {
    disp_uint4(lcd_calibration_value_2,1,0xb,1);
  }
  else {
    disp_uint4(lcd_calibration_value_2,1,0xb,0);
  }
  if (lcd_calibration_cursor == '\x02') {
    disp_uint4(lcd_calibration_value_3,2,0xb,1);
  }
  else {
    disp_uint4(lcd_calibration_value_3,2,0xb,0);
  }
  if (lcd_calibration_cursor == '\x03') {
    disp_uint4(lcd_calibration_value_4,3,0xb,1);
  }
  else {
    disp_uint4(lcd_calibration_value_4,3,0xb,0);
  }
  return;
}
