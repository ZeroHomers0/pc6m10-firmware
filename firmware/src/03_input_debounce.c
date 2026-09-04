/* =============================================================================
 * src/03_input_debounce.c — 反编译模块 03（输入/去抖）可编译副本
 * 目标B 阶段4：原样复制，仅加 includes。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"

/* 0x00001578 —— 输入引脚方向配置（置输入模式）
 *   input_fio_base_ptr=0x2009C000（FIO 池基址）
 *   [8]  = FIO0DIR（+0x20）；[0x18]=FIO1DIR（+0x60）
 *   配置为输入：P0.28/27（0x10000000/0x8000000）、P1.16/17（0x10000/0x20000）、
 *   P0.9（0x200）、P0.6（0x40）、P0.2/0.3（0x4/0x8） */
void gpio_inputs_dir_init(void)
{
  volatile uint32_t *gpio_base;

  gpio_base = input_fio_base_ptr;
  input_fio_base_ptr[8] = input_fio_base_ptr[8] & 0xfff7ffff;   /* P1.17 输入 */
  gpio_base[8] = gpio_base[8] & 0xfffbffff;               /* P1.16 输入 */
  *gpio_base = *gpio_base & 0xbfffffff;                   /* P0.30 输入 */
  *gpio_base = *gpio_base & 0xdfffffff;                   /* P0.29 输入 */
  gpio_base[0x18] = gpio_base[0x18] & 0xfdffffff;         /* P2.25 输入 */
  gpio_base[0x18] = gpio_base[0x18] & 0xfbffffff;         /* P2.24 输入 */
  *gpio_base = *gpio_base & 0xf7ffffff;                   /* P0.27 输入 */
  *gpio_base = *gpio_base & 0xefffffff;                   /* P0.28 输入 */
  gpio_base[8] = gpio_base[8] & 0xfffeffff;               /* P1.16 输入 */
  gpio_base[8] = gpio_base[8] & 0xfffdffff;               /* P1.17 输入 */
  *gpio_base = *gpio_base & 0xffffffbf;                   /* P0.6 输入 */
  *gpio_base = *gpio_base & 0xfffffffb;                   /* P0.3 输入 */
  *gpio_base = *gpio_base & 0xfffffff7;                   /* P0.2 输入 */
  return;
}

/* 0x000015FE —— 输入扫描状态机（旋转编码器 + RUN/STOP 按钮解码）
 *   首次检测任何输入变化 → 计数 0x10001978 累加；
 *   连续采样 0x19(25) 拍后按 A/B 相组合锁存旋转方向到 0x1000197C（1..6）；
 *   计数超 0xF9 → 0xF5 并产生按键事件码：
 *     0x0B=编码器加（慢，计数>10 触发，含复位 0x10001980 复用）
 *     0x16=编码器加（A=1,B=0，快）
 *     0x21=编码器减（A=1,B=1,P1.16=0）
 *     0x17=编码器减（A=1,B=0,P1.16=0）
 *     0x0E=编码器加（A=0,B=1,P1.16=0，计数>0x1E 触发）
 *   （0x16/0x21 = state_machine 的 r4 快进/快退码；0x0B/0x17/0x0E 为慢/组合键）
 *   全部输入就绪（非 0）→ 返回 0x1000197C 锁存值 */
uint8_t input_scan_state(void)
{
  volatile uint32_t *scan_counter_ptr;
  volatile uint8_t *counter_ptr;

  scan_counter_ptr = input_scan_counter_ptr;
  /* —— 任一输入未就绪（P1.16/P1.17/P0.28/P0.27/P2.9/P2.8 为 0）→ 进入扫描 —— */
  if ((((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) == 0) ||
        ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) == 0)) ||
       ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) == 0)) ||
      (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) == 0 ||
       ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) == 0)))) ||
     ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) == 0)) {
    *input_scan_counter_ptr = *input_scan_counter_ptr + 1;
    if (*scan_counter_ptr == 0x19) {
      /* 连续 0x19 拍后锁存旋转方向（A/B 相组合，1..6） */
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) == 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
         (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
          ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))))) {
        *input_direction_latch_ptr = 1;
      }
      if (((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) == 0)) &&
          (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)))))) &&
         ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)) {
        *input_direction_latch_ptr = 2;
      }
      if (((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) == 0)) &&
         ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))) {
        *input_direction_latch_ptr = 3;
      }
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
         (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
          ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) == 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))))) {
        *input_direction_latch_ptr = 4;
      }
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
         ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) == 0)))) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))) {
        *input_direction_latch_ptr = 5;
      }
      if ((((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0)) &&
          (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)))) &&
         ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) == 0)) {
        *input_direction_latch_ptr = 6;
      }
    }
    if (0xf9 < *input_scan_counter_ptr) {
      *input_scan_counter_ptr = 0xf5;
      *input_direction_latch_ptr = 0;
      counter_ptr = input_encoder_counter_ptr;
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) == 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
         (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
          ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))))) {
        *input_encoder_counter_ptr = *input_encoder_counter_ptr + 1;
        if (0xb < *counter_ptr) {
          *counter_ptr = 0xb;
        }
        if (*input_encoder_counter_ptr == 10) {
          return 0xb;          /* 0x0B = 慢加事件（计数 10） */
        }
      }
      counter_ptr = input_encoder_counter_ptr;
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) == 0)) &&
         ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)))) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))) {
        return 0x16;           /* 0x16 = 快加（A=1,B=0） */
      }
      if (((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) == 0)) &&
         ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))) {
        return 0x21;           /* 0x21 = 快减（P0.28=1,P0.27=1,P1.16=0） */
      }
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) != 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) == 0)) &&
         (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) == 0 &&
          ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) != 0 &&
            ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x2000000) != 0)) &&
           ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x74) & 0x4000000) != 0)))))) {
        *input_encoder_counter_ptr = *input_encoder_counter_ptr + 1;
        if (0x1f < *counter_ptr) {
          *counter_ptr = 0x1f;
        }
        if (*input_encoder_counter_ptr == 0x1e) {
          return 0x17;         /* 0x17 = 慢减事件（计数 0x1E=30） */
        }
      }
      counter_ptr = input_encoder_counter_ptr;
      if ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x80000) == 0) &&
          ((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x34) & 0x40000) != 0)) &&
         ((((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x40000000) != 0 &&
           (((*(volatile uint32_t *)((uint32_t)input_fio_base_ptr + 0x14) & 0x20000000) == 0 &&
            ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x74) & 0x2000000) != 0)))) &&
          ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x74) & 0x4000000) != 0)))) {
        *input_encoder_counter_ptr = *input_encoder_counter_ptr + 1;
        if (0x1f < *counter_ptr) {
          *counter_ptr = 0x1f;
        }
        if (*input_encoder_counter_ptr == 0x1e) {
          return 0xe;          /* 0x0E = 组合键事件（计数 0x1E） */
        }
      }
    }
  }
  else {
    /* —— 全部输入就绪（稳定）—— */
    if (0x18 < *input_scan_counter_ptr) {
      *input_scan_counter_ptr = 0;
      return *input_direction_latch_ptr;    /* 返回锁存方向 */
    }
    *input_direction_latch_ptr = 0;
    *input_scan_counter_ptr = 0;
    *input_encoder_counter_ptr = 0;
  }
  return 0;
}
/* 0x000019C6 —— RUN/STOP 按钮扫描（P0.28/P0.27）
 *   P0.28 低且 P0.27 高（RUN 按下，计数 0x32=50）→ 返回 7
 *   P0.28 高且 P0.27 低（STOP 按下，计数 0x32）→ 返回 8
 *   双模式：0x10001C14==0 单次触发；!=0 保持模式（0x10001C20 锁存 7/8）
 *   0x10001C0C = FIO 池基址（0x2009C000） */
uint8_t scan_run_stop(void)
{
  volatile uint8_t *counter_ptr;
  uint8_t event_code;

  counter_ptr = input_run_counter_ptr;
  if (*input_run_stop_mode_ptr == '\0') {
    /* —— 单次触发模式 —— */
    if (((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x10000000) == 0) &&
       ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x8000000) != 0)) {
      *input_run_counter_ptr = *input_run_counter_ptr + 1;
      if (*counter_ptr == 0x32) {
        *counter_ptr = 0x32;
        return 7;              /* RUN */
      }
      if (0x32 < *input_run_counter_ptr) {
        *input_run_counter_ptr = 0x32;
      }
    }
    else {
      *input_run_counter_ptr = 0;
    }
    counter_ptr = input_stop_counter_ptr;
    if (((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x10000000) == 0) ||
       ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x8000000) != 0)) {
      *input_stop_counter_ptr = 0;
    }
    else {
      *input_stop_counter_ptr = *input_stop_counter_ptr + 1;
      if (*counter_ptr == 0x32) {
        *counter_ptr = 0x32;
        return 8;              /* STOP */
      }
      if (0x32 < *input_stop_counter_ptr) {
        *input_stop_counter_ptr = 0x32;
      }
    }
    event_code = 0;
  }
  else {
    /* —— 保持模式：去抖后锁存到 0x10001C20 —— */
    if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x10000000) == 0) {
      *input_run_counter_ptr = *input_run_counter_ptr + 1;
      if (0x31 < *counter_ptr) {
        *counter_ptr = 0x32;
        *input_run_stop_latch_ptr = 7;
      }
    }
    else {
      *input_run_counter_ptr = 0;
    }
    counter_ptr = input_stop_counter_ptr;
    if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x10000000) == 0) {
      *input_stop_counter_ptr = 0;
    }
    else {
      *input_stop_counter_ptr = *input_stop_counter_ptr + 1;
      if (0x31 < *counter_ptr) {
        *counter_ptr = 0x32;
        *input_run_stop_latch_ptr = 8;
      }
    }
    event_code = *input_run_stop_latch_ptr;
  }
  return event_code;
}

/* 0x00001AB8 —— P0.9 去抖（24V 交流方波检测，阈值 0xF=15）：
 *   持续高 0xF 拍 → 返回 1（清零计数）；低 → 清零 */
uint32_t debounce_p09(void)
{
  volatile uint8_t *counter_ptr;

  counter_ptr = input_p09_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x200) == 0) {   /* P0.9 低 */
    *input_p09_counter_ptr = 0;
  }
  else {
    *input_p09_counter_ptr = *input_p09_counter_ptr + 1;
    if (0xf < *counter_ptr) {
      *counter_ptr = 0;
      return 1;
    }
  }
  return 0;
}

/* 0x00001AE6 —— P1.16 去抖（外故障，阈值 0xFA=250）：
 *   高持续 0xFA 拍 → 返回 1；低持续 0xFA 拍 → 返回 2（边沿双向） */
uint32_t debounce_p116(void)
{
  volatile uint8_t *counter_ptr;

  counter_ptr = input_p116_high_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x34) & 0x10000) == 0) {   /* P1.16 低 */
    *input_p116_high_counter_ptr = 0;
  }
  else {
    *input_p116_high_counter_ptr = *input_p116_high_counter_ptr + 1;
    if (0xfa < *counter_ptr) {
      *counter_ptr = 0xfa;
      return 1;              /* 高沿 */
    }
  }
  counter_ptr = input_p116_low_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x34) & 0x10000) == 0) {
    *input_p116_low_counter_ptr = *input_p116_low_counter_ptr + 1;
    if (0xfa < *counter_ptr) {
      *counter_ptr = 0xfa;
      return 2;              /* 低沿 */
    }
  }
  else {
    *input_p116_low_counter_ptr = 0;
  }
  return 0;
}

/* 0x00001B3E —— P1.17 去抖（复位，阈值 0x32=50，同 P1.16 结构） */
uint32_t debounce_p117(void)
{
  volatile uint8_t *counter_ptr;

  counter_ptr = input_p117_high_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x34) & 0x20000) == 0) {   /* P1.17 低 */
    *input_p117_high_counter_ptr = 0;
  }
  else {
    *input_p117_high_counter_ptr = *input_p117_high_counter_ptr + 1;
    if (0x32 < *counter_ptr) {
      *counter_ptr = 0x32;
      return 1;
    }
  }
  counter_ptr = input_p117_low_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x34) & 0x20000) == 0) {
    *input_p117_low_counter_ptr = *input_p117_low_counter_ptr + 1;
    if (0x32 < *counter_ptr) {
      *counter_ptr = 0x32;
      return 2;
    }
  }
  else {
    *input_p117_low_counter_ptr = 0;
  }
  return 0;
}

/* 0x00001B96 —— P0.6 去抖（急停，阈值 0x32=50） */
uint32_t debounce_p06(void)
{
  volatile uint8_t *counter_ptr;

  counter_ptr = input_p06_high_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x40) == 0) {      /* P0.6 低 */
    *input_p06_high_counter_ptr = 0;
  }
  else {
    *input_p06_high_counter_ptr = *input_p06_high_counter_ptr + 1;
    if (0x32 < *counter_ptr) {
      *counter_ptr = 0x32;
      return 1;
    }
  }
  counter_ptr = input_p06_low_counter_ptr;
  if ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 0x40) == 0) {
    *input_p06_low_counter_ptr = *input_p06_low_counter_ptr + 1;
    if (0x32 < *counter_ptr) {
      *counter_ptr = 0x32;
      return 2;
    }
  }
  else {
    *input_p06_low_counter_ptr = 0;
  }
  return 0;
}

/* 0x00001BEE —— P0.2 与 P0.3 双低联锁检查：同时为 0 → 返回 1（安全联锁），否则 0
 *   main 中开机判断：<1 才进主循环，否则显示联锁错误屏 */
uint32_t chk_p02_p03(void)
{
  uint32_t result;

  if (((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 4) == 0) && ((*(volatile uint32_t *)(input_run_stop_fio_base_ptr + 0x14) & 8) == 0)) {
    result = 1;
  }
  else {
    result = 0;
  }
  return result;
}

/* 0x000010F8C —— P0 输入初始化（P0.2/3/28/27 等置输入，配合 read_input_p02） */
void gpio0_input_init(void)
{
  volatile uint32_t *gpio_base;

  gpio_base = input_gpio0_base;
  *input_gpio0_base = *input_gpio0_base & 0xfffffffb;   /* P0.3 输入 */
  *gpio_base = *gpio_base & 0xefffffff;               /* P0.28 输入 */
  *gpio_base = *gpio_base & 0xf7ffffff;               /* P0.27 输入 */
  return;
}

/* 0x000010FAE —— 读 P0.2 状态到 0x100010FD0（RUN/STOP 模式选择读取） */
uint32_t read_input_p02(void)
{
  if ((*(volatile uint32_t *)((uint32_t)input_gpio0_base + 0x14) & 4) == 0) {
    input_p02_state = 0;
  }
  else {
    input_p02_state = 1;
  }
  return 0;
}
