/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 13：GPIO 初始化（NVIC IRQ 使能 + GPIO2 认证总线）
 *
 * nvic_enable_irq：NVIC ISER0..（0xE000E100 起，每个 32 位寄存器管 32 个 IRQ）
 *   irq_num>>5 = ISER 寄存器索引，irq_num&0x1f = 位号；置位即使能对应 IRQ。
 * gpio2_init：把 GPIO2 的 4 条腿配成 1-Wire 认证总线（经 ADuM1201 隔离）：
 *   FIO 池 0x2009C000（auth_gpio_base），+0x40 FIO2DIR、+0x58 FIO2SET；
 *   P2.1=数据出、P2.2=数据入（双向/输入）、P2.3=时钟、P2.4=复位，初值全高。
 *
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 认证时序/引脚 → src/11_auth.c（auth_challenge/auth_retry）
 *   · 开机调用序列 → src/01_startup.c（gpio2_init → auth_challenge ×3 → auth_retry）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"

#include "inc/firmware_api.h"
#include "inc/firmware_state.h"

/* 0x00010628 —— NVIC IRQ 使能：置位 ISER 指定位。irq_num 为 IRQ 号（见 LPC176x 向量表），
 *   ISER 基址 0xE000E100，index=irq_num>>5，bit=irq_num&0x1f。 */
void nvic_enable_irq(uint32_t irq_num)
{
  NVIC->ISER[irq_num >> 5] = 1u << (irq_num & 0x1f);
}

/* 0x0001064C —— GPIO2（1-Wire 认证总线）引脚方向/初值初始化。
 *   FIO2DIR(+0x40)：P2.4/P2.3/P2.1 置输出，P2.2 清位 2 改为输入（0xfffffffb &= ~0x4）；
 *   FIO2SET(+0x58)：P2.4/P2.3/P2.1 初值拉高。 */
void gpio2_init(void)
{
  fio_set_direction(FIO2, 0x10);   /* P2.4=输出 */
  fio_set_direction(FIO2, 0x08);   /* P2.3=输出 */
  fio_set_direction(FIO2, 0x02);   /* P2.1=输出 */
  fio_clear_direction(FIO2, 0x04);  /* P2.2=输入（双向） */
  fio_set(FIO2, 0x10);              /* P2.4=高 */
  fio_set(FIO2, 0x08);              /* P2.3=高 */
  fio_set(FIO2, 0x02);              /* P2.1=高 */
}
