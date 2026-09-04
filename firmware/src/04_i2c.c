/* =============================================================================
 * src/04_i2c.c — 反编译模块 04（I2C GPIO 位带模拟）可编译副本
 * 目标B 阶段4：原样保留，修正三处 Ghidra 反编译伪影：
 *   1) i2c_write_byte/i2c_read_byte 的多余返回寄存器伪变量（反编译器对
 *      lsls/lsrs 移位 + 调用后 r1/r2 残留的建模）→ 按 0x1D3C/0x1DFA
 *      反汇编精确重写：移位 = (data<<1)&0xFF，ACK = FIO0PIN bit10。
 *   2) i2c_delay_short 被反编译出多余实参 → 恢复无参调用。
 *   3) 原反编译器生成的临时指针 → volatile uint32_t*（i2c_fio_base 语义）—— 本模块
 *      局部已语义化：fio（FIO 池指针）、i/j（循环计数）、units（延时单位数）、
 *      data/i/ack（写字节）、ack（读寄存器 ACK）、out_buf/reg_addr（读写参数）。
 * 符号：i2c_fio_base=0x2009C000 FIO0 池；[5]=+0x14 FIO0PIN、[6]=+0x18
 *       FIO0SET、[7]=+0x1C FIO0CLR。ACK 状态记录在 i2c_ack_status。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"

/* 0x00001C40 —— I2C GPIO 初始化：P0.10(SDA)/P0.11(SCL) 置输出（FIODIR） */
void i2c_gpio_init(void)
{
  volatile uint32_t *fio;

  fio = i2c_fio_base;
  *i2c_fio_base = *i2c_fio_base | 0x800;    /* P0.11 SCL 输出 */
  *fio = *fio | 0x400;                /* P0.10 SDA 输出 */
  fio[6] = fio[6] | 0x800;            /* SCL 初始高 */
  fio[6] = fio[6] | 0x400;            /* SDA 初始高 */
  return;
}
/* 0x00001C6C —— 短延时（5 空循环） */
void i2c_delay_short(void)
{
  volatile uint32_t i;
  for (i = 5; i != 0; i--) {
    /* 原固件空转延时。 */
  }
}

/* 0x00001C82 —— 延时（units × 10000 空循环；EEPROM 写周期等待用） */
void i2c_delay(uint32_t units)
{
  volatile uint32_t i;
  volatile uint32_t j;

  for (i = 0; i < units; i++) {
    for (j = 0; j < 10000; j++) {
      /* EEPROM 写周期等待。 */
    }
  }
}

/* 0x00001CAE —— START 条件：SDA=1 → SCL=1 → SDA=0 → SCL=0 */
uint32_t i2c_start(void)
{
  volatile uint32_t *fio;

  *i2c_fio_base = *i2c_fio_base | 0x400;    /* SDA=1 */
  i2c_delay_short();
  fio = i2c_fio_base;
  i2c_fio_base[6] = i2c_fio_base[6] | 0x400;
  fio[6] = fio[6] | 0x800;            /* SCL=1 */
  i2c_delay_short();
  i2c_fio_base[7] = i2c_fio_base[7] | 0x400;  /* SDA=0 */
  i2c_delay_short();
  i2c_fio_base[7] = i2c_fio_base[7] | 0x800;  /* SCL=0 */
  i2c_delay_short();
  return 1;
}

/* 0x00001CFE —— STOP 条件：SCL=1 → SDA=1 */
void i2c_stop(void)
{
  volatile uint32_t *fio;

  *i2c_fio_base = *i2c_fio_base | 0x400;    /* SDA=1 */
  i2c_delay_short();
  fio = i2c_fio_base;
  i2c_fio_base[7] = i2c_fio_base[7] | 0x400;  /* SDA=0 */
  fio[6] = fio[6] | 0x800;            /* SCL=1 */
  i2c_delay_short();
  i2c_fio_base[6] = i2c_fio_base[6] | 0x400;  /* SDA=1 */
  i2c_delay_short();
  return;
}

/* 0x00001D3C —— 发送 1 字节（MSB 先）+ 读 ACK；返回 ACK（1=无应答/从机未拉低）
 * 反汇编核实（0x1D3C）：data=(data<<1)&0xFF，移位过程不保留多余返回值；
 *   ACK：读 FIO0PIN(+0x14) bit10，为高→1（无 ACK），为低→0（ACK）。 */
uint32_t i2c_write_byte(uint32_t byte_val)
{
  uint32_t data;
  uint32_t i;
  uint32_t ack;

  data = byte_val;
  *i2c_fio_base = *i2c_fio_base | 0x400;    /* SDA=1 */
  i2c_delay_short();
  for (i = 0; i < 8; i = i + 1) {
    if ((data & 0x80) != 0) {
      i2c_fio_base[6] = i2c_fio_base[6] | 0x400;   /* 位=1 → SDA=1 */
    }
    else {
      i2c_fio_base[7] = i2c_fio_base[7] | 0x400;   /* 位=0 → SDA=0 */
    }
    i2c_fio_base[6] = i2c_fio_base[6] | 0x800;     /* SCL=1 */
    i2c_delay_short();
    i2c_fio_base[7] = i2c_fio_base[7] | 0x800;     /* SCL=0 */
    i2c_delay_short();
    data = (data << 1) & 0xFF;                     /* 下一位（=lsls/lsrs） */
  }
  *i2c_fio_base = *i2c_fio_base & 0xfffffbff;   /* SDA 方向输入（读 ACK） */
  i2c_fio_base[6] = i2c_fio_base[6] | 0x400;    /* 释放 SDA */
  i2c_delay_short();
  i2c_fio_base[6] = i2c_fio_base[6] | 0x800;    /* SCL=1 */
  i2c_delay_short();
  ack = (i2c_fio_base[5] & 0x400) != 0;         /* 读 FIO0PIN bit10 */
  i2c_fio_base[7] = i2c_fio_base[7] | 0x800;    /* SCL=0 */
  i2c_delay_short();
  *i2c_fio_base = *i2c_fio_base | 0x400;        /* SDA 方向输出 */
  return ack;
}

/* 0x00001DFA —— 接收 1 字节（MSB 先）+ 发 NACK；返回读到的字节
 * 反汇编核实（0x1DFA）：data=(data<<1)&0xFF；SDA 引脚高→该位=1。 */
uint32_t i2c_read_byte(void)
{
  uint32_t data;
  uint32_t i;

  *i2c_fio_base = *i2c_fio_base & 0xfffffbff;   /* SDA 输入 */
  i2c_fio_base[6] = i2c_fio_base[6] | 0x400;    /* 释放 SDA */
  i2c_delay_short();
  data = 0;
  for (i = 0; i < 8; i = i + 1) {
    i2c_delay_short();
    i2c_fio_base[7] = i2c_fio_base[7] | 0x800;  /* SCL=0 */
    i2c_delay_short();
    i2c_fio_base[6] = i2c_fio_base[6] | 0x800;  /* SCL=1 */
    i2c_delay_short();
    data = (data << 1) & 0xFF;                  /* 移位累加 */
    if ((i2c_fio_base[5] & 0x400) != 0) {       /* 读 SDA 引脚 */
      data = data + 1;                          /* 位=1 */
    }
    i2c_delay_short();
  }
  i2c_delay_short();
  i2c_fio_base[7] = i2c_fio_base[7] | 0x800;    /* SCL=0 */
  i2c_delay_short();
  *i2c_fio_base = *i2c_fio_base | 0x400;        /* SDA 输出（NACK） */
  i2c_delay_short();
  return data;
}

/* 0x00001E88 —— 写芯片寄存器：[0xA6][reg][val] + STOP + 写周期等待
 *   data=写寄存器数据、reg_addr=寄存器号；ACK 记录到 0x10001F00 */
void i2c_write_reg(uint32_t data,uint32_t reg_addr)
{
  uint8_t ack;

  i2c_start();
  ack = i2c_write_byte(0xa6);      /* 器件地址写（0x53<<1） */
  i2c_ack_status = ack;
  ack = i2c_write_byte(reg_addr);   /* 寄存器号 */
  i2c_ack_status = ack;
  ack = i2c_write_byte(data);   /* 数据 */
  i2c_ack_status = ack;
  i2c_stop();
  i2c_delay(5);                      /* EEPROM 写周期 */
  return;
}

/* 0x00001EBC —— 读芯片寄存器：[0xA6][reg] → restart → [0xA7] 读 1 字节 → STOP
 *   out_buf=读回数据缓冲区、reg_addr=寄存器号 */
void i2c_read_reg(uint8_t *out_buf,uint32_t reg_addr)
{
  uint8_t ack;

  i2c_start();
  ack = i2c_write_byte(0xa6);      /* 器件地址写 */
  i2c_ack_status = ack;
  ack = i2c_write_byte(reg_addr);   /* 寄存器号 */
  i2c_ack_status = ack;
  i2c_start();                       /* 重复 START */
  ack = i2c_write_byte(0xa7);      /* 器件地址读 */
  i2c_ack_status = ack;
  ack = i2c_read_byte();           /* 原反编译 i2c_read_byte(0) 实参去掉 */
  *out_buf = ack;                  /* 读 1 字节 */
  i2c_stop();
  i2c_delay(2);
  return;
}
