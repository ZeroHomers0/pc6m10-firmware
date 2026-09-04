/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 09：输出级（SCR 触发角计算）/ 引脚配置 /
 *                         定时器 / 外部中断（急停/锁存）
 *
 * 关键硬件（对照 HARDWARE_VERIFICATION_2026-08-20.md）：
 *   · EINT1/2/3 = P2.11/12/13（PINSEL4 0x4002C000+0x10，值=01；三相同步过零 U10/U11/U12）
 *     反编译确证 2026-08-21；P0.22/23/24 为 LPC176x 默认复用但本固件未启用
 *     SCB 0x400FC000：EXTINT=+0x140、EXTMODE=+0x148、EXTPOLAR=+0x14C
 *   · P0.22 = 运行继电器 RLY1（fio0_pin22_ctrl）；P1.22 = 触发/运行指示（fio1_pin22_ctrl）
 *   · TIMER1(IRQ2)、TIMER2(IRQ3) 兼作显示/触发时序；TIMER1 ISR 为 LCD 动态扫描
 *
 * output_stage（主循环每节拍调用）——SCR 移相触发的核心：
 *   运行状态 0x1000EDA8（==1 跳过处理）；每 9 拍执行一次
 *   触发角：180°(0xB4) - 当前角；周期基量 TRIG_PERIOD、角度系数 ANGLE_SCALE(6333)/100
 *   软起/软停斜坡（状态机 0x1000EE24/0x1000F268/0x1000F780：0=停、4=运行、5=稳定）
 *   闭环：closed_loop_wrapper(PID) 计算输出角；上下限钳位 0x1000F2C8/0x1000F2D0
 *   保护：过压(0x1000EDE4..) / 过流(0x1000EDF8..) / 缺相(0x1000EE04..)，
 *   故障标志 0x1000EDF4（只置不清锁存，需断电复位；2026-08-21 反编译确证）：
 *     bit4=过压、bit5=过流、bit3=缺相(判据×0.15/1.5/2 且延时×50/20/10)、
 *     bit9=缺相严重级(判据×2.5/3/3.5 且延时×5/2/1)
 *   停机斜坡（cfg_word=0 且联锁非 0 时）：0x1000FBB0/6C/70 逐拍降频
 * 导出：2026-08-21（L0 语义化：局部 GPIO、定时器和电平变量均已按实际角色命名、
 *   output_stage/EINT3/TIMER1 HANDLER 各局部已按角色命名；函数名保持语义化原样）
 *
 * 交叉引用：
 *   · 触发 GPIO（G1-G6 / P12-G1~G6）与驱动链 → docs/HARDWARE_VERIFICATION_2026-08-20.md §二.2、§六.B
 *   · SCR 触发时序（TIMER2.MR0 / TIMER1 扫描 240 步 / 12° 脉宽）→ docs/PROGRESS_2026-08-20.md §4j、§4k
 *   · 保护标志 / 停机链 → APPLICATION_GUIDE_2026-08-21.md §四.1
 *   · 菜单映射（软起/相位限制/主从偏移/起始相位）→ docs/MENU_PARAMETER_MAPPING.md §1
 *   ✅ EINT 引脚差异复核完成（2026-08-21）：eint1/2/3_init 反编译确证 PINSEL4
 *     配置 P2.11/12/13=EINT1/2/3（硬件印证正确）；P0.22=fio0_pin22_ctrl=运行继电器 RLY1。
 * ========================================================================== */

/* =============================================================================
 * src/09_output_stage.c — 反编译模块 09（输出级 SCR 触发角/引脚/定时器/EINT）可编译副本
 * 目标B 阶段4 修正：
 *   1) 补 include（types.h/reg.h/firmware_state.h/firmware_parameters.h）。
 *   2) output_fio_base = FIO 池基址 0x2009C000，旧反编译代码将其表示为无类型字节偏移基址
 *      （gpio_base+off = +off 字节：+0x1c FIO0CLR、+0x18 FIO0SET、+0x40 FIO2DIR、+0x58 FIO2SET）。
 *      语义地址映射统一使用 volatile uint8_t*；局部 gpio_base 同步改为
 *      volatile uint8_t*。
 *   3) &parameter_profile4_gain_b_ptr 指针算术 → 数字地址 0x3904（flash 表地址，非变量地址）。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"

#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include "inc/firmware_parameters.h"
#include "inc/consts.h"

/* 0x0000E5A8 —— 引脚功能/方向配置（PINSEL+FIO DIR，详见 HARDWARE_VERIFICATION）
 *   地址映射固定地址0E988 = FIO 池基址 0x2009C000
 *   FIO0DIR(P0.20/21/22、P0.15..19、P0.7/8、P0.4/5)、FIO1/FIO2/FIO3/FIO4 方向
 *   局部：gpio_base = output_fio_base（FIO 池基址，gpio_base+off = +off 字节） */
void pin_config(void)
{
  fio_set_direction(FIO0, 0x100000); /* P0.20 */
  fio_set_direction(FIO0, 0x200000); /* P0.21 */
  fio_set_direction(FIO0, 0x400000); /* P0.22 */
  fio_clear(FIO0, 0x100000);         /* P0.20 */
  fio_clear(FIO0, 0x200000);         /* P0.21 */
  fio_clear(FIO0, 0x400000);         /* P0.22 */
  fio_set_direction(FIO0, 0x20000);  /* P0.17 */
  fio_set_direction(FIO0, 0x40000);  /* P0.18 */
  fio_set_direction(FIO0, 0x80000);  /* P0.19 */
  fio_set_direction(FIO2, 0x200);    /* P2.9 */
  fio_set_direction(FIO0, 0x10000);  /* P0.16 */
  fio_set_direction(FIO0, 0x8000);   /* P0.15 */
  fio_set_direction(FIO2, 0x100);    /* P2.8 */
  fio_set_direction(FIO2, 0x80);     /* P2.7 */
  fio_set_direction(FIO2, 0x40);     /* P2.6 */
  fio_set_direction(FIO2, 0x20);     /* P2.5 */
  fio_set_direction(FIO0, 0x100);    /* P0.8 */
  fio_set_direction(FIO0, 0x80);     /* P0.7 */
  fio_set(FIO0, 0x20000);            /* P0.17 */
  fio_set(FIO0, 0x40000);            /* P0.18 */
  fio_set(FIO0, 0x80000);            /* P0.19 */
  fio_set(FIO2, 0x200);              /* P2.9 */
  fio_set(FIO0, 0x10000);             /* P0.16 */
  fio_set(FIO0, 0x8000);              /* P0.15 */
  fio_set(FIO2, 0x100);              /* P2.8 */
  fio_set(FIO2, 0x80);               /* P2.7 */
  fio_set(FIO2, 0x40);               /* P2.6 */
  fio_set(FIO2, 0x20);               /* P2.5 */
  fio_set(FIO0, 0x100);              /* P0.8 */
  fio_set(FIO0, 0x80);               /* P0.7 */
  fio_set_direction(FIO1, 0x100000); /* P1.20 */
  fio_set_direction(FIO1, 0x200000); /* P1.21 */
  fio_set_direction(FIO1, 0x400000); /* P1.22 */
  fio_set_direction(FIO1, 0x800000); /* P1.23 */
  fio_clear(FIO1, 0x100000);         /* P1.20 */
  fio_clear(FIO1, 0x200000);         /* P1.21 */
  fio_clear(FIO1, 0x400000);         /* P1.22 */
  fio_clear(FIO1, 0x800000);         /* P1.23 */
  fio_set_direction(FIO4, 0x10000000); /* P4.28 */
  fio_set_direction(FIO4, 0x20000000); /* P4.29 */
  fio_set_direction(FIO0, 0x10);       /* P0.4 */
  fio_set_direction(FIO0, 0x20);       /* P0.5 */
  fio_set_direction(FIO2, 0x1);        /* P2.0 */
  fio_clear(FIO4, 0x10000000);         /* P4.28 */
  fio_clear(FIO4, 0x20000000);         /* P4.29 */
  fio_clear(FIO0, 0x10);                /* P0.4 */
  fio_clear(FIO0, 0x20);                /* P0.5 */
  fio_clear(FIO2, 0x1);                 /* P2.0 */
  return;
}
/* 0x0000E79A —— 输出使能（关断时所有触发/使能线复位） */
void gpio_outputs_set(void)
{
  fio_set(FIO0, 0x20000);
  fio_set(FIO0, 0x40000);
  fio_set(FIO0, 0x80000);
  fio_set(FIO2, 0x200);
  fio_set(FIO0, 0x10000);
  fio_set(FIO0, 0x8000);
  fio_set(FIO2, 0x100);
  fio_set(FIO2, 0x80);
  fio_set(FIO2, 0x40);
  fio_set(FIO2, 0x20);
  fio_set(FIO0, 0x100);
  fio_set(FIO0, 0x80);
  return;
}
/* 0x0000E816 —— TIMER1 初始化（IRQ2）：TCR 复位、PR=0x18 预分频、MR0=999、
 *   匹配中断+复位（MCR=3）。元素索引×4 = 寄存器字节偏移（TIMER 寄存器 4 字节对齐，
 *   与原厂 0xE816 反汇编逐地址一致）：[0]=IR、[1]=TCR、[2]=TC、[3]=PR、[5]=MCR、[6]=MR0。
 *   局部：timer1 = output_timer1_registers → 指向 TIMER1（0x40008000） */
void timer1_init(void)
{
  TIMER1->TCR = 2;       /* TCR=2：TC 复位 */
  TIMER1->PR = 0x18;     /* PR=0x18：预分频 */
  TIMER1->MR0 = 999;     /* MR0=999 匹配值 */
  TIMER1->IR = 0xff;     /* IR 清中断 */
  TIMER1->MCR = 3;       /* MCR=3：MR0 匹配中断+复位 */
  nvic_enable_irq(2);                                 /* NVIC TIMER1_IRQ */
  return;
}

/* 0x0000E838 —— TIMER2 初始化（IRQ3）：PCONP TMR2、TCR 复位、PR=0x18 预分频、
 *   MR0=999、匹配中断+复位（MCR=3）。元素索引×4 = 寄存器字节偏移（TIMER 寄存器
 *   4 字节对齐，与原厂 0xE838 反汇编逐地址一致）。
 *   局部：timer2 = output_timer2_registers → 指向 TIMER2（0x40090000） */
void timer2_init(void)
{
  SYSTEM_CONTROL->power_control = *system_pconp | 0x400000;  /* PCONP bit22 TMR2 */
  TIMER2->TCR = 2;       /* TCR=2：TC 复位 */
  TIMER2->PR = 0x18;     /* PR=0x18：预分频 */
  TIMER2->MR0 = 999;     /* MR0=999 匹配值 */
  TIMER2->IR = 0xff;     /* IR 清中断 */
  TIMER2->MCR = 3;       /* MCR=3：MR0 匹配中断+复位 */
  nvic_enable_irq(3);                                 /* NVIC TIMER2_IRQ（反汇编 0xE868 核 r0=3） */
  return;
}

/* 0x0000E86E —— EINT1 外部中断初始化（P2.11，边沿触发，清挂起，NVIC 0x13=19）
 *   局部：base = 先 PINSEL(0x4002C000,+0x10 PINSEL4) 后 SCB(0x400FC000,EXTINT/EXTMODE/EXTPOLAR) 复用基址 */
void eint1_init(void)
{
  SYSTEM_CONTROL->external_interrupt = SYSTEM_CONTROL->external_interrupt | 2; /* EXTINT 清 EINT1 */
  PIN_SELECT->PINSEL[4] = PIN_SELECT->PINSEL[4] | 0x400000; /* P2.11=EINT1 */
  PIN_SELECT->PINSEL[4] = PIN_SELECT->PINSEL[4] & 0xff7fffff;
  SYSTEM_CONTROL->external_mode = SYSTEM_CONTROL->external_mode | 2; /* EXTMODE 边沿 */
  SYSTEM_CONTROL->external_polarity = SYSTEM_CONTROL->external_polarity & 0xfffffffd; /* 下降沿 */
  SYSTEM_CONTROL->external_interrupt = SYSTEM_CONTROL->external_interrupt | 2; /* 清挂起 */
  nvic_enable_irq(0x13);                                    /* NVIC EINT1_IRQ=19 */
  return;
}

/* 0x0000E8CA —— EINT2 外部中断初始化（P2.12，边沿触发，NVIC 0x14=20）
 *   局部：base = 先 PINSEL(0x4002C000) 后 SCB(0x400FC000) 复用基址 */
void eint2_init(void)
{
  PIN_SELECT->PINSEL[4] = PIN_SELECT->PINSEL[4] | 0x1000000; /* P2.12=EINT2 */
  PIN_SELECT->PINSEL[4] = PIN_SELECT->PINSEL[4] & 0xfdffffff;
  SYSTEM_CONTROL->external_mode = SYSTEM_CONTROL->external_mode | 4; /* EXTMODE 边沿 */
  SYSTEM_CONTROL->external_polarity = SYSTEM_CONTROL->external_polarity & 0xfffffffb; /* 下降沿 */
  nvic_enable_irq(0x14);                                    /* NVIC EINT2_IRQ=20 */
  return;
}

/* 0x0000E908 —— EINT3 外部中断初始化（P2.13，边沿触发，NVIC 0x15=21）
 *   局部：base = 先 PINSEL(0x4002C000) 后 SCB(0x400FC000) 复用基址 */
void eint3_init(void)
{
  PIN_SELECT->PINSEL[4] = PIN_SELECT->PINSEL[4] | 0x4000000; /* P2.13=EINT3 */
  PIN_SELECT->PINSEL[4] = PIN_SELECT->PINSEL[4] & 0xf7ffffff;
  SYSTEM_CONTROL->external_mode = SYSTEM_CONTROL->external_mode | 8; /* EXTMODE 边沿 */
  SYSTEM_CONTROL->external_polarity = SYSTEM_CONTROL->external_polarity & 0xfffffff7; /* 下降沿 */
  nvic_enable_irq(0x15);                                    /* NVIC EINT3_IRQ=21 */
  return;
}

/* 0x0000E946 —— P1.22 电平控制（level>=1 置位、<1 清零；触发/运行指示 LED）
 *   level>=1 → FIO1SET；否则 FIO1CLR */
void fio1_pin22_ctrl(int level)
{
  if (level < 1) {
    fio_clear(FIO1, 0x400000);
  }
  else {
    fio_set(FIO1, 0x400000);
  }
  return;
}

/* 0x0000E966 —— P0.22 电平控制（level>=1 置位、<1 清零；运行继电器 RLY1，2026-08-21 复核）
 *   level>=1 → FIO0SET；否则 FIO0CLR */
void fio0_pin22_ctrl(int level)
{
  if (level < 1) {
    fio_clear(FIO0, 0x400000);
  }
  else {
    fio_set(FIO0, 0x400000);
  }
  return;
}

/* 0x0000E9AC —— 输出级主处理（SCR 移相触发角计算 + 保护 + 软起停 + PID）
 *   详见文件头说明。每节拍调用；*0x1000EDA8==1 跳过处理；每 9 拍执行一次。
 * 局部变量角色（反编译寄存器复用，跨段复用）：
 *   scheduler_tick_ptr    — 9 拍节拍计数（0x1000EDAC）
 *   working_word_ptr    — 通用字指针（触发角/软起累加/步进等 0x1000EDB4/EE28/EE48/F2F0/F788）
 *   working_value_ptr — 通用临时指针（闭环看门狗计数/输出值/软起多处）
 *   constant_voltage_output_ptr      — 恒压源模式输出值指针（0x1000F77C）
 *   ramp_counter_ptr    — 停机斜坡当前值指针（output_ramp_counter_ptr）
 *   closed_loop_output     — closed_loop_wrapper(PID) 返回的闭环输出角 */
static void output_check_overvoltage(void)
{
      if ((*parameter_overvoltage_limit_ptr == 0) || (*adc_voltage_output_ptr <= *parameter_overvoltage_limit_ptr)) {
        *output_overvoltage_counter_ptr = 0;
      }
      else {
        *output_overvoltage_counter_ptr = *output_overvoltage_counter_ptr + 1;
        if ((uint32_t)*parameter_overvoltage_time_ptr * 0x32 < *output_overvoltage_counter_ptr) {
          *output_overvoltage_counter_ptr = 0;
          *output_fault_flags_ptr = *output_fault_flags_ptr | 0x10;      /* 过压故障标志 */
        }
      }
}

static void output_check_undervoltage(void)
{
      if ((*parameter_undervoltage_limit_ptr == 0) || (*parameter_undervoltage_limit_ptr <= *adc_voltage_output_ptr)) {
        *output_undervoltage_counter_ptr = 0;
      }
      else {
        *output_undervoltage_counter_ptr = *output_undervoltage_counter_ptr + 1;
        if ((uint32_t)*parameter_undervoltage_time_ptr * 0x32 < *output_undervoltage_counter_ptr) {
          *output_undervoltage_counter_ptr = 0;
          *output_fault_flags_ptr = *output_fault_flags_ptr | 0x20;      /* 过流故障标志 */
        }
      }
}

static void output_check_phase_imbalance(void)
{
      if (*parameter_if_overload_limit_ptr != 0) {
        *output_phase_imbalance_counter_ptr = *output_phase_imbalance_counter_ptr + 1;
        if (*adc_field_output_ptr < *parameter_if_overload_limit_ptr) {
          *output_phase_imbalance_counter_ptr = 0;
        }
        if ((*parameter_if_overload_limit_ptr <= *adc_field_output_ptr) && ((uint32_t)*parameter_if_overload_time_ptr * 0x32 < *output_phase_imbalance_counter_ptr)) {
          *output_fault_flags_ptr = *output_fault_flags_ptr | 8;
        }
        if (((*parameter_if_overload_limit_ptr * 0xf) / 10 < *adc_field_output_ptr) &&
           ((uint32_t)*parameter_if_overload_time_ptr * 0x14 < *output_phase_imbalance_counter_ptr)) {
          *output_fault_flags_ptr = *output_fault_flags_ptr | 8;
        }
        if ((*parameter_if_overload_limit_ptr * 2 < *adc_field_output_ptr) && ((uint32_t)*parameter_if_overload_time_ptr * 10 < *output_phase_imbalance_counter_ptr)) {
          *output_fault_flags_ptr = *output_fault_flags_ptr | 8;
        }
        if (((*parameter_if_overload_limit_ptr * 0x19) / 10 < *adc_field_output_ptr) &&
           ((uint32_t)*parameter_if_overload_time_ptr * 5 < *output_phase_imbalance_counter_ptr)) {
          *output_fault_flags_ptr = *output_fault_flags_ptr | 0x200;
        }
        if ((*parameter_if_overload_limit_ptr * 3 < *adc_field_output_ptr) && ((uint32_t)*parameter_if_overload_time_ptr * 2 < *output_phase_imbalance_counter_ptr)) {
          *output_fault_flags_ptr = *output_fault_flags_ptr | 0x200;
        }
        if (((*parameter_if_overload_limit_ptr * 0x23) / 10 < *adc_field_output_ptr) && (1 < *output_phase_imbalance_counter_ptr)) {
          *output_fault_flags_ptr = *output_fault_flags_ptr | 0x200;
        }
      }
}

void output_stage(void)
{
  volatile uint8_t *scheduler_tick_ptr;
  volatile uint32_t *working_word_ptr;
  volatile uint32_t *working_value_ptr;
  volatile uint32_t *constant_voltage_output_ptr;
  volatile uint32_t *ramp_counter_ptr;
  uint32_t closed_loop_output;

  scheduler_tick_ptr = output_tick_counter_ptr;
  if ((*output_enable_state_ptr != 1) && (*output_tick_counter_ptr = *output_tick_counter_ptr + 1, 9 < *scheduler_tick_ptr)) {
    *scheduler_tick_ptr = 0;
    working_word_ptr = output_trigger_angle_ptr;
    *output_trigger_angle_ptr = 0xb4 - *parameter_start_phase_ptr;            /* 触发角 = 180° - 当前角 */
    if (*working_word_ptr == 0) {
      *working_word_ptr = 1;
    }
    *output_trigger_period_ptr = 0xb4 - *parameter_phase_limit_ptr;
    if (*parameter_output_mode_ptr == '\0') {
      *output_soft_start_state_ptr = 0;
      if (*parameter_control_mode_ptr == '\0') {
        *output_ramp_divisor_ptr = *parameter_current_range_ptr / 0xf;         /* 周期/15 换算 */
      }
      if (*parameter_control_mode_ptr == '\x01') {
        *output_ramp_divisor_ptr = *parameter_voltage_range_ptr / 0xf;
      }
      if (*parameter_control_mode_ptr == '\x02') {
        *output_current_scale_ptr = (*parameter_current_limit_ptr * 1000) / *parameter_current_range_ptr;
        *output_ramp_divisor_ptr = *parameter_current_range_ptr / 0xf;
      }
    }
    if ((*parameter_output_mode_ptr == '\x01') && (9 < *adc_output_reference_ptr)) {
      /* —— 过压保护（0x1000EDE4 阈值 / 0x1000EDE8 延时上限）—— */
      output_check_overvoltage();
      /* —— 过流保护（0x1000EDF8 阈值）—— */
      output_check_undervoltage();
      /* —— 缺相/不平衡保护（0x1000EE04 判据，多级延时）—— */
      output_check_phase_imbalance();
      working_value_ptr = output_protection_counter_ptr;
      *output_protection_counter_ptr = *output_protection_counter_ptr + 1;
      if (0x5dc < *working_value_ptr) {                          /* 1500 拍 → 欠压/锁定标志 */
        *output_protection_counter_ptr = 0;
        *output_soft_start_state_ptr = 1;
      }
      if (*parameter_control_mode_ptr == '\0') {
        /* —— 软起动斜坡状态机（0x1000EE24：0=停 4=运行 5=稳定）—— */
        *output_current_difference_ptr = *output_reference_average_ptr;
        *pid_divisor_source_ptr = 0;
        if (*output_run_state_ptr == 0) {
          *output_run_state_ptr = 4;
          working_word_ptr = output_setpoint_ptr;
          *output_setpoint_ptr = 0;
          *pid_accumulator_ptr = *working_word_ptr;
          *pid_error_current_ptr = 0;
          *pid_error_previous_term_ptr = 0;
          *output_feedback_delta_ptr = 0;
          *output_phase_mode_ptr = 0;
          *output_current_limited_ptr = SOFT_START_INIT;                     /* 软起累加初始 6001 */
        }
        if (*output_run_state_ptr == 4) {
          *output_current_difference_ptr = *adc_output_reference_ptr;
          working_word_ptr = output_ramp_counter_ptr;
          *output_ramp_counter_ptr =                             /* 触发步进 = (TRIG_PERIOD-角*6333/100)/50/除数 */
               (int)(((uint64_t)(TRIG_PERIOD - (uint32_t)(*output_trigger_angle_ptr * ANGLE_SCALE) / 100) / 0x32) /
                    (uint64_t)*parameter_soft_start_time_ptr);
          *output_ramp_value_ptr = *output_ramp_value_ptr + *working_word_ptr;
          if (*output_limit_status_ptr == '\0') {
            *output_ramp_target_ptr = (uint32_t)(*output_trigger_angle_ptr * ANGLE_SCALE) / 100 + *output_ramp_value_ptr;
            *output_fault_state_ptr = 1;
          }
          if (*output_limit_status_ptr == '\x01') {
            *output_ramp_target_ptr = *output_ramp_value_ptr;
            *output_fault_state_ptr = 0;
          }
          working_word_ptr = output_setpoint_ptr;
          *output_setpoint_ptr = *output_ramp_target_ptr * 100;
          *pid_cached_output_ptr = *working_word_ptr;
          *pid_accumulator_ptr = *output_setpoint_ptr;
          *pid_error_current_ptr = 0;
          *pid_error_previous_term_ptr = 0;
          if ((*output_current_difference_ptr * 0x2f8) / 100 + 0xedd < *output_ramp_target_ptr) {  /* 达目标 → 5 稳定 */
            *output_run_state_ptr = 5;
          }
          if ((*parameter_current_limit_ptr <= *adc_field_output_ptr) && (*parameter_current_limit_ptr <= *parameter_current_range_ptr)) {
            *output_run_state_ptr = 5;
          }
          if (*adc_gain_b_output_ptr < *adc_voltage_output_scaled_ptr) {
            *output_run_state_ptr = 5;
          }
        }
        if (*output_run_state_ptr == 5) {
          /* —— 第一路闭环运行（0x1000F270..F2A4 反馈/给定比较 + PID）—— */
          *output_fault_state_ptr = 0;
          working_value_ptr = output_state_aux_b_ptr;
          if ((*adc_field_output_ptr < *parameter_current_limit_ptr) || (*output_reference_average_ptr <= *adc_voltage_output_ptr)) {
            *output_state_aux_b_ptr = *output_state_aux_b_ptr + 1;
            if (0x32 < *working_value_ptr) {
              *working_value_ptr = 0;
              *output_state_aux_a_ptr = 2;
              *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
              *output_phase_mode_ptr = '\0';
            }
          }
          else {
            *output_state_aux_a_ptr = 1;
            *output_state_aux_b_ptr = 0;
          }
          working_value_ptr = output_state_aux_c_ptr;
          if (*output_reference_average_ptr < *output_current_limited_ptr) {
            *output_state_aux_c_ptr = *output_state_aux_c_ptr + 1;
            if (10 < *working_value_ptr) {
              *working_value_ptr = 0;
              *output_state_aux_a_ptr = 0;
              *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
              *output_phase_mode_ptr = '\0';
            }
          }
          else {
            *output_state_aux_c_ptr = 0;
          }
          if (((*output_current_limited_ptr <= *output_reference_average_ptr) && (*parameter_current_limit_ptr <= *parameter_current_range_ptr)) &&
             (*output_state_aux_a_ptr == 2)) {
            *output_reference_average_ptr = *output_current_limited_ptr;
            working_value_ptr = output_current_limited_ptr;
            *output_current_limited_ptr = *output_current_limited_ptr + 1;
            if (6000 < *working_value_ptr) {
              *output_current_limited_ptr = 6000;
            }
            *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
            *output_phase_mode_ptr = '\0';
          }
          if (((*parameter_current_limit_ptr <= *parameter_current_range_ptr) && (*output_state_aux_a_ptr == 1)) &&
             (*parameter_voltage_range_ptr < *parameter_current_range_ptr)) {
            *pid_divisor_source_ptr = 1;
            if (*output_phase_mode_ptr != '\x01') {
              *output_phase_mode_ptr = '\x01';
              *output_current_limited_ptr = *adc_voltage_output_scaled_ptr;
            }
            if (*output_current_limited_ptr < 2) {
              *output_current_limited_ptr = 1;
            }
            *output_reference_average_ptr = *parameter_current_limit_ptr;
            *adc_voltage_output_scaled_ptr = *adc_field_output_scaled_ptr;
          }
          if (((*adc_voltage_output_scaled_ptr <= *output_reference_average_ptr) && (*parameter_current_limit_ptr <= *parameter_current_range_ptr)) &&
             ((*output_state_aux_a_ptr == 1 && (*parameter_current_range_ptr <= *parameter_voltage_range_ptr)))) {
            *pid_divisor_source_ptr = 1;
            if (*output_phase_mode_ptr != '\x01') {
              *output_phase_mode_ptr = '\x01';
              *output_current_limited_ptr = *adc_voltage_output_scaled_ptr;
            }
            if (*output_current_limited_ptr < 2) {
              *output_current_limited_ptr = 1;
            }
            *output_reference_average_ptr = *parameter_current_limit_ptr;
            *adc_voltage_output_scaled_ptr = *adc_field_output_scaled_ptr;
          }
          if (((*adc_voltage_output_ptr < 2) && (*parameter_current_limit_ptr <= *parameter_current_range_ptr)) && (*output_state_aux_a_ptr == 1)) {
            *pid_divisor_source_ptr = 1;
            if (*output_phase_mode_ptr != '\x01') {
              *output_phase_mode_ptr = '\x01';
              *output_current_limited_ptr = *adc_voltage_output_scaled_ptr;
            }
            if (*output_current_limited_ptr < 2) {
              *output_current_limited_ptr = 1;
            }
            *output_reference_average_ptr = *parameter_current_limit_ptr;
            *adc_voltage_output_scaled_ptr = *adc_field_output_scaled_ptr;
          }
          working_value_ptr = output_feedback_delta_ptr;
          if (((*adc_voltage_output_raw_ptr < 5) && (*output_soft_start_state_ptr == '\x01')) && (*parameter_feedback_mode_ptr == '\x01')) {
            *output_feedback_delta_ptr = *output_feedback_delta_ptr + 1;
            if (100 < *working_value_ptr) {
              *output_fault_flags_ptr = *output_fault_flags_ptr | 0x800;
            }
          }
          else {
            *output_feedback_delta_ptr = 0;
          }
          closed_loop_output = closed_loop_wrapper(*output_reference_average_ptr,*adc_voltage_output_scaled_ptr,*parameter_active_gain_a_ptr,*parameter_active_gain_b_ptr);
          *output_setpoint_ptr = closed_loop_output;
        }
        if (((uint32_t)(*output_upper_limit_ptr * ANGLE_SCALE) < *output_setpoint_ptr ||
             *output_upper_limit_ptr * ANGLE_SCALE - *output_setpoint_ptr == 0) && (*output_protection_mode_ptr == '\0')) {
          *output_setpoint_ptr = *output_upper_limit_ptr * ANGLE_SCALE;     /* 输出上限钳位 */
        }
        if ((*output_setpoint_ptr <= (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE)) && (*output_protection_mode_ptr == '\0')) {
          *output_setpoint_ptr = *output_lower_limit_ptr * ANGLE_SCALE;     /* 输出下限钳位 */
        }
      }
      if (*parameter_control_mode_ptr == '\x01') {
        /* —— 第二路闭环（0x1000F2E8.. 软起 + PID + 钳位）—— */
        *output_current_difference_ptr = *output_reference_average_ptr;
        *pid_divisor_source_ptr = 0;
        if (*output_run_state_ptr == 0) {
          *output_run_state_ptr = 4;
          working_value_ptr = output_setpoint_ptr;
          *output_setpoint_ptr = 0;
          *pid_accumulator_ptr = *working_value_ptr;
          *pid_error_current_ptr = 0;
          *pid_error_previous_term_ptr = 0;
          *output_feedback_delta_ptr = 0;
          *output_phase_mode_ptr = '\0';
          *output_current_limited_ptr = SOFT_START_INIT;
        }
        if (*output_run_state_ptr == 4) {
          *output_current_difference_ptr = *adc_output_reference_ptr;
          working_word_ptr = output_ramp_counter_ptr;
          *output_ramp_counter_ptr =
               (int)(((uint64_t)(TRIG_PERIOD - (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE) / 100) / 0x32) /
                    (uint64_t)*parameter_soft_start_time_ptr);
          *output_ramp_value_ptr = *output_ramp_value_ptr + *working_word_ptr;
          if (*output_limit_status_ptr == '\0') {
            *output_ramp_target_ptr = (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE) / 100 + *output_ramp_value_ptr;
            *output_fault_state_ptr = 1;
          }
          if (*output_limit_status_ptr == '\x01') {
            *output_ramp_target_ptr = *output_ramp_value_ptr;
            *output_fault_state_ptr = 0;
          }
          working_value_ptr = output_setpoint_ptr;
          *output_setpoint_ptr = *output_ramp_target_ptr * 100;
          *pid_cached_output_ptr = *working_value_ptr;
          *pid_accumulator_ptr = *output_setpoint_ptr;
          *pid_error_current_ptr = 0;
          *pid_error_previous_term_ptr = 0;
          if ((*output_current_difference_ptr * 0x2f8) / 100 + 0xedd < *output_ramp_target_ptr) {
            *output_run_state_ptr = 5;
          }
          if ((*parameter_voltage_limit_ptr <= *adc_voltage_output_ptr) && (*parameter_voltage_limit_ptr <= *parameter_voltage_range_ptr)) {
            *output_run_state_ptr = 5;
          }
          if (*adc_gain_b_output_ptr < *adc_field_output_scaled_ptr) {
            *output_run_state_ptr = 5;
          }
        }
        if (*output_run_state_ptr == 5) {
          /* —— 第二路闭环运行（0x1000F70C..F740）—— */
          *output_fault_state_ptr = 0;
          working_value_ptr = output_state_aux_a_ptr;
          if ((*adc_voltage_output_ptr < *parameter_voltage_limit_ptr) || (*output_reference_average_ptr <= *adc_field_output_ptr)) {
            *output_state_aux_a_ptr = *output_state_aux_a_ptr + 1;
            if (0x32 < *working_value_ptr) {
              *working_value_ptr = 0;
              *output_state_aux_b_ptr = 2;
              *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
              *output_phase_mode_ptr = '\0';
            }
          }
          else {
            *output_state_aux_b_ptr = 1;
            *output_state_aux_a_ptr = 0;
          }
          working_value_ptr = output_state_aux_c_ptr;
          if (*output_reference_average_ptr < *output_current_limited_ptr) {
            *output_state_aux_c_ptr = *output_state_aux_c_ptr + 1;
            if (10 < *working_value_ptr) {
              *working_value_ptr = 0;
              *output_state_aux_b_ptr = 0;
              *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
              *output_phase_mode_ptr = '\0';
            }
          }
          else {
            *output_state_aux_c_ptr = 0;
          }
          if (((*output_current_limited_ptr <= *output_reference_average_ptr) && (*parameter_voltage_limit_ptr <= *parameter_voltage_range_ptr)) &&
             (*output_state_aux_b_ptr == 2)) {
            *output_reference_average_ptr = *output_current_limited_ptr;
            working_value_ptr = output_current_limited_ptr;
            *output_current_limited_ptr = *output_current_limited_ptr + 1;
            if (6000 < *working_value_ptr) {
              *output_current_limited_ptr = 6000;
            }
            *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
            *output_phase_mode_ptr = '\0';
          }
          if (((*parameter_voltage_limit_ptr <= *parameter_voltage_range_ptr) && (*output_state_aux_b_ptr == 1)) &&
             (*parameter_current_range_ptr < *parameter_voltage_range_ptr)) {
            *pid_divisor_source_ptr = 1;
            if (*output_phase_mode_ptr != '\x01') {
              *output_phase_mode_ptr = '\x01';
              *output_current_limited_ptr = *adc_voltage_output_scaled_ptr;
            }
            if (*output_current_limited_ptr < 2) {
              *output_current_limited_ptr = 1;
            }
            *output_reference_average_ptr = *parameter_voltage_limit_ptr;
            *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
          }
          if (((*adc_voltage_output_scaled_ptr <= *output_reference_average_ptr) && (*parameter_voltage_limit_ptr <= *parameter_voltage_range_ptr)) &&
             ((*output_state_aux_b_ptr == 1 && (*parameter_voltage_range_ptr <= *parameter_current_range_ptr)))) {
            *pid_divisor_source_ptr = 1;
            if (*output_phase_mode_ptr != '\x01') {
              *output_phase_mode_ptr = '\x01';
              *output_current_limited_ptr = *adc_voltage_output_scaled_ptr;
            }
            if (*output_current_limited_ptr < 2) {
              *output_current_limited_ptr = 1;
            }
            *output_reference_average_ptr = *parameter_voltage_limit_ptr;
            *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
          }
          if (((*adc_field_output_ptr < 2) && (*parameter_voltage_limit_ptr <= *parameter_voltage_range_ptr)) && (*output_state_aux_b_ptr == 1)) {
            *pid_divisor_source_ptr = 1;
            if (*output_phase_mode_ptr != '\x01') {
              *output_phase_mode_ptr = '\x01';
              *output_current_limited_ptr = *adc_voltage_output_scaled_ptr;
            }
            if (*output_current_limited_ptr < 2) {
              *output_current_limited_ptr = 1;
            }
            *output_reference_average_ptr = *parameter_voltage_limit_ptr;
            *adc_voltage_output_scaled_ptr = *adc_voltage_output_scaled_ptr;
          }
          working_value_ptr = output_feedback_delta_ptr;
          if (((*adc_field_output_raw_ptr < 5) && (*output_soft_start_state_ptr == '\x01')) && (*parameter_feedback_mode_ptr == '\x01')) {
            *output_feedback_delta_ptr = *output_feedback_delta_ptr + 1;
            if (100 < *working_value_ptr) {
              *output_fault_flags_ptr = *output_fault_flags_ptr | 0x800;
            }
          }
          else {
            *output_feedback_delta_ptr = 0;
          }
          closed_loop_output = closed_loop_wrapper(*output_reference_average_ptr,*adc_voltage_output_scaled_ptr,*parameter_active_gain_a_ptr,*parameter_active_gain_b_ptr);
          *output_setpoint_ptr = closed_loop_output;
        }
        if (((uint32_t)(*output_upper_limit_ptr * ANGLE_SCALE) < *output_setpoint_ptr ||
             *output_upper_limit_ptr * ANGLE_SCALE - *output_setpoint_ptr == 0) && (*output_protection_mode_ptr == '\0')) {
          *output_setpoint_ptr = *output_upper_limit_ptr * ANGLE_SCALE;
        }
        if ((*output_setpoint_ptr <= (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE)) && (*output_protection_mode_ptr == '\0')) {
          *output_setpoint_ptr = *output_lower_limit_ptr * ANGLE_SCALE;
        }
      }
      working_value_ptr = output_state_aux_b_ptr;
      if (*parameter_control_mode_ptr == '\x02') {
        /* —— 恒压源模式（0x1000F774..F794）—— */
        *output_state_aux_b_ptr = *adc_output_reference_ptr;
        constant_voltage_output_ptr = output_current_difference_ptr;
        *output_current_difference_ptr = *working_value_ptr % 10000;
        if (1000 < *constant_voltage_output_ptr) {
          *constant_voltage_output_ptr = 1000;
        }
        if (*output_current_difference_ptr < 10) {
          *output_current_difference_ptr = 10;
        }
        if (*output_run_state_ptr == 0) {
          *output_run_state_ptr = 4;
        }
        if (*output_run_state_ptr == 4) {
          if (*parameter_soft_start_time_ptr == 0) {
            *output_run_state_ptr = 5;
          }
          working_word_ptr = output_ramp_counter_ptr;
          *output_ramp_counter_ptr =
               (int)(((uint64_t)(TRIG_PERIOD - (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE) / 100) / 0x32) /
                    (uint64_t)*parameter_soft_start_time_ptr);
          *output_ramp_value_ptr = *output_ramp_value_ptr + *working_word_ptr;
          if (*output_limit_status_ptr == '\0') {
            *output_ramp_target_ptr = (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE) / 100 + *output_ramp_value_ptr;
            *output_fault_state_ptr = 1;
          }
          if (*output_limit_status_ptr == '\x01') {
            *output_ramp_target_ptr = *output_ramp_value_ptr;
            *output_fault_state_ptr = 0;
          }
          working_value_ptr = output_setpoint_ptr;
          *output_setpoint_ptr = *output_ramp_target_ptr * 100;
          if (*working_value_ptr < 10) {
            *working_value_ptr = 10;
          }
          if ((*output_current_difference_ptr * 0x2f8) / 100 + 0xedd < *output_ramp_target_ptr) {
            *output_run_state_ptr = 5;
          }
        }
        if (*output_run_state_ptr == 5) {
          *output_fault_state_ptr = 0;
          working_value_ptr = output_ramp_target_ptr;
          *output_ramp_target_ptr = (*output_current_difference_ptr * 0x2f8) / 100 + 0xedd;
          *output_setpoint_ptr = *working_value_ptr * 100;
        }
        if (((uint32_t)(*output_upper_limit_ptr * ANGLE_SCALE) < *output_setpoint_ptr ||
             *output_upper_limit_ptr * ANGLE_SCALE - *output_setpoint_ptr == 0) && (*output_protection_mode_ptr == '\0')) {
          *(volatile int *)output_setpoint_ptr = *output_upper_limit_ptr * ANGLE_SCALE;
        }
        if ((*(volatile uint32_t *)output_setpoint_ptr <= (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE)) &&
           (*output_protection_mode_ptr == '\0')) {
          *(volatile int *)output_setpoint_ptr = *output_lower_limit_ptr * ANGLE_SCALE;
        }
      }
    }
    if (((*parameter_output_mode_ptr == '\0') && (*(volatile int *)output_run_state_ptr != 0)) &&
       (*output_fault_state_ptr == '\x01')) {
      /* —— 运行联锁解除：全部输出复位 —— */
      gpio_outputs_set();
      *(volatile uint32_t *)output_ramp_counter_ptr = 0;
      *(volatile uint32_t *)output_ramp_value_ptr = 0;
      *(volatile uint32_t *)output_ramp_target_ptr = 0;
      *(volatile uint32_t *)output_setpoint_ptr = 0;
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
      *(volatile uint32_t *)output_run_state_ptr = 0;
      *output_limit_status_ptr = 0;
    }
    if (((*parameter_output_mode_ptr == '\0') && (*(volatile int *)output_run_state_ptr != 0)) ||
       (*adc_output_reference_ptr < 10)) {
      /* —— 停机斜坡（逐拍降频）—— */
      if (*(volatile int *)output_run_state_ptr == 5) {
        *(volatile uint32_t *)output_run_state_ptr = 4;
        *(volatile uint32_t *)output_ramp_value_ptr = *(volatile uint32_t *)output_setpoint_ptr / 100;
      }
      ramp_counter_ptr = output_ramp_counter_ptr;
      if (*parameter_soft_stop_time_ptr == 0) {
        gpio_outputs_set();
        *(volatile uint32_t *)output_ramp_counter_ptr = 0;
        *(volatile uint32_t *)output_ramp_value_ptr = 0;
        *(volatile uint32_t *)output_ramp_target_ptr = 0;
        *(volatile uint32_t *)output_setpoint_ptr = 0;
        if (*parameter_output_mode_ptr == '\0') {
          fio0_pin22_ctrl(0);
          fio1_pin22_ctrl(0);
        }
        *(volatile uint32_t *)output_run_state_ptr = 0;
        *output_limit_status_ptr = 0;
      }
      else {
        *(volatile int *)output_ramp_counter_ptr =
             (int)(((uint64_t)(TRIG_PERIOD - (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE) / 100) / 0x32) /
                  (uint64_t)*parameter_soft_stop_time_ptr);
        if (*(volatile uint32_t *)ramp_counter_ptr < *(volatile uint32_t *)output_ramp_value_ptr) {
          *(volatile uint32_t *)output_run_state_ptr = 4;
          if (*output_fault_state_ptr == '\0') {
            *output_limit_status_ptr = 1;
          }
          *(volatile int *)output_ramp_value_ptr = *(volatile int *)output_ramp_value_ptr - *(volatile int *)output_ramp_counter_ptr;
          if (*output_fault_state_ptr == '\x01') {
            *(volatile uint32_t *)output_ramp_target_ptr =
                 (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE) / 100 + *(volatile int *)output_ramp_value_ptr;
          }
          if (*output_fault_state_ptr == '\0') {
            *(volatile uint32_t *)output_ramp_target_ptr = *(volatile uint32_t *)output_ramp_value_ptr;
          }
          *(volatile int *)output_setpoint_ptr = *(volatile int *)output_ramp_target_ptr * 100;
          if (((uint32_t)(*output_upper_limit_ptr * ANGLE_SCALE) < *(volatile uint32_t *)output_setpoint_ptr ||
               *output_upper_limit_ptr * ANGLE_SCALE - *(volatile uint32_t *)output_setpoint_ptr == 0) &&
             (*output_protection_mode_ptr == '\0')) {
            *(volatile int *)output_setpoint_ptr = *output_upper_limit_ptr * ANGLE_SCALE;
          }
          if ((*(volatile uint32_t *)output_setpoint_ptr <= (uint32_t)(*output_lower_limit_ptr * ANGLE_SCALE)) &&
             (*output_protection_mode_ptr == '\0')) {
            gpio_outputs_set();
            *(volatile uint32_t *)output_ramp_counter_ptr = 0;
            *(volatile uint32_t *)output_ramp_value_ptr = 0;
            *(volatile uint32_t *)output_ramp_target_ptr = 0;
            *(volatile uint32_t *)output_setpoint_ptr = 0;
            if (*parameter_output_mode_ptr == '\0') {
              fio0_pin22_ctrl(0);
              fio1_pin22_ctrl(0);
            }
            *(volatile uint32_t *)output_run_state_ptr = 0;
            *output_limit_status_ptr = 0;
          }
        }
        else {
          gpio_outputs_set();
          *(volatile uint32_t *)output_ramp_counter_ptr = 0;
          *(volatile uint32_t *)output_ramp_value_ptr = 0;
          *(volatile uint32_t *)output_ramp_target_ptr = 0;
          *(volatile uint32_t *)output_setpoint_ptr = 0;
          if (*parameter_output_mode_ptr == '\0') {
            fio0_pin22_ctrl(0);
            fio1_pin22_ctrl(0);
          }
          *(volatile uint32_t *)output_run_state_ptr = 0;
          *output_limit_status_ptr = 0;
        }
      }
    }
  }
  return;
}

/* 0x0000F9AA —— 运行/停机预设（cfg_word=1 → 启动；=0 → 停机复位输出） */
void run_stop_preset(void)
{
  if (*parameter_output_mode_ptr == '\x01') {
    *(volatile int *)output_setpoint_ptr = output_ramp_table_ptr[0xfa] * 100;
    *(volatile uint32_t *)output_run_state_ptr = 5;
  }
  if (*parameter_output_mode_ptr == '\0') {
    *(volatile uint32_t *)output_setpoint_ptr = *output_ramp_table_ptr;
    *(volatile uint32_t *)output_run_state_ptr = 0;
    fio0_pin22_ctrl(0);
    fio1_pin22_ctrl(0);
  }
  return;
}

/* 0x0000F9E8 —— EINT1 ISR：清中断，置 input_state=2（正转），eint1_flag=1 */
void EINT1_IRQHandler(void)
{
    SYSTEM_CONTROL->external_interrupt = SYSTEM_CONTROL->external_interrupt | 2;   /* EXTINT 清 EINT1 */
  if (*output_input_state_ptr == '\0') {
    *output_input_state_ptr = 2;
  }
  *output_eint1_flag_ptr = 1;
  return;
}

/* 0x0000FA0A —— EINT2 ISR：清中断，置 input_state=1（反转），eint2_flag=1 */
void EINT2_IRQHandler(void)
{
    SYSTEM_CONTROL->external_interrupt = SYSTEM_CONTROL->external_interrupt | 4;   /* EXTINT 清 EINT2 */
  if (*output_input_state_ptr == '\0') {
    *output_input_state_ptr = 1;
  }
  *output_eint2_flag_ptr = 1;
  return;
}

/* 0x0000FA2C —— EINT3 ISR：急停/外部控制锁存
 *   0x1000FBD8=input_state、0x1000FBE4=mode_byte（正/反转模式）、
 *   0x1000FBA8=input_locked、0x1000FBEC 消抖计数、
 *   0x1000FBF0=phase_cnt（相位窗口判断）、0x1000FBF4=freq_hz、
 *   0x1000FBF8=hold_count（保持计数超时 → 复位运行）、
 *   后半段为输出预置：按 freq_hz（'2'=50Hz/'<'=60Hz）与 out_phase(0/1) 查表
 *   计算 out_scale/out_div/MR0（0x1000D0C+0x18）与触发使能
 * 局部：ptr=消抖计数/保持计数/输出区复用指针；out_scale=输出比例(0x1000 输出区)指针 */
static void output_process_phase_interrupt(void)
{
  volatile uint8_t *ptr;
  volatile uint8_t *out_scale;
  ptr = output_debounce_counter_ptr;
  *output_debounce_counter_ptr = *output_debounce_counter_ptr + '\x01';
  if (9 < (uint8_t)*ptr) {
    *ptr = 0;
    if ((0x60 < (uint8_t)*system_phase_counter_ptr) && ((uint8_t)*system_phase_counter_ptr < 0x68)) {
      *output_frequency_ptr = 0x32;                 /* '2' = 50Hz 档 */
      *(volatile uint32_t *)output_hold_counter_ptr = 0;
    }
    if ((0x51 < (uint8_t)*system_phase_counter_ptr) && ((uint8_t)*system_phase_counter_ptr < 0x56)) {
      *output_frequency_ptr = 0x3c;                 /* '<' = 60Hz 档 */
      *(volatile uint32_t *)output_hold_counter_ptr = 0;
    }
    ptr = output_hold_counter_byte_ptr;
    if (((((uint8_t)*system_phase_counter_ptr < 0x61) || (0x67 < (uint8_t)*system_phase_counter_ptr)) &&
        (*output_frequency_ptr == '2')) &&
       (*(volatile int *)output_hold_counter_ptr = *(volatile int *)output_hold_counter_ptr + 1, 4 < *(volatile uint32_t *)ptr))
    {
      /* —— 50Hz 相位保持超时 → 停机复位 —— */
      *(volatile uint32_t *)ptr = 0;
      *output_frequency_ptr = 0;
      *parameter_output_mode_ptr = 0;
      gpio_outputs_set();
      *(volatile uint32_t *)output_ramp_counter_ptr = 0;
      *(volatile uint32_t *)output_ramp_value_ptr = 0;
      *(volatile uint32_t *)output_ramp_target_ptr = 0;
      *(volatile uint32_t *)output_setpoint_ptr = 0;
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
      *(volatile uint32_t *)output_run_state_ptr = 0;
      *output_limit_status_ptr = 0;
      *(volatile uint32_t *)output_fault_flags_ptr = *(volatile uint32_t *)output_fault_flags_ptr | 0x2000;
    }
    ptr = output_hold_counter_byte_ptr;
    if ((((uint8_t)*system_phase_counter_ptr < 0x52) || (0x55 < (uint8_t)*system_phase_counter_ptr)) &&
       ((*output_frequency_ptr == '<' &&
        (*(volatile int *)output_hold_counter_ptr = *(volatile int *)output_hold_counter_ptr + 1, 4 < *(volatile uint32_t *)ptr)
        ))) {
      /* —— 60Hz 相位保持超时 → 停机复位 —— */
      *(volatile uint32_t *)ptr = 0;
      *output_frequency_ptr = 0;
      *parameter_output_mode_ptr = 0;
      gpio_outputs_set();
      *(volatile uint32_t *)output_ramp_counter_ptr = 0;
      *(volatile uint32_t *)output_ramp_value_ptr = 0;
      *(volatile uint32_t *)output_ramp_target_ptr = 0;
      *(volatile uint32_t *)output_setpoint_ptr = 0;
      fio0_pin22_ctrl(0);
      fio1_pin22_ctrl(0);
      *(volatile uint32_t *)output_run_state_ptr = 0;
      *output_limit_status_ptr = 0;
      *(volatile uint32_t *)output_fault_flags_ptr = *(volatile uint32_t *)output_fault_flags_ptr | 0x2000;
    }
    *system_phase_counter_ptr = 0;
  }
  if (((*(volatile uint32_t *)output_run_state_ptr < 2) || (7 < *(volatile uint32_t *)output_run_state_ptr)) ||
     ((*(volatile int *)output_fault_flags_ptr != 0 || ((uint8_t)*output_frequency_ptr < 0x32)))) {
    gpio_outputs_set();                               /* 非法态/停机 → 复位输出 */
  }
  else {
    /* —— 输出预置：根据 freq_hz + out_phase 计算触发参数 —— */
    TIMER2->TCR = 2;
    TIMER2->IR = 0xff;
    ptr = output_scale_byte_ptr;
    if (*parameter_output_phase_ptr == '\0') {
      if (*output_frequency_ptr == '2') {
        /* 50Hz、单相出 */
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)output_setpoint_ptr;
        out_scale = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = (uint32_t)(*(volatile int *)ptr * 0x58) / 100;
        ptr = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)out_scale / 100;
        if (0x2730 < *(volatile uint32_t *)ptr) {
          *(volatile uint32_t *)output_scale_ptr = 0x2730;
        }
        *(volatile uint32_t *)output_divisor_ptr =
             (uint32_t)((0x2731 - *(volatile int *)output_scale_ptr) * 10) / 0x22d;
        if (*output_mode_ptr == '\x01') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x1800 +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x38) - *(volatile int *)output_scale_ptr;
        }
        if (*output_mode_ptr == '\x02') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x1814 +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x38) - *(volatile int *)output_scale_ptr;
        }
      }
      ptr = output_scale_byte_ptr;
      if (*output_frequency_ptr == '<') {
        /* 60Hz、单相出 */
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)output_setpoint_ptr;
        out_scale = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = (uint32_t)(*(volatile int *)ptr * 0x50) / 100;
        ptr = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)out_scale / 100;
        if (0x23a0 < *(volatile uint32_t *)ptr) {
          *(volatile uint32_t *)output_scale_ptr = 0x23a0;
        }
        *(volatile uint32_t *)output_divisor_ptr =
             (uint32_t)((0x23a1 - *(volatile int *)output_scale_ptr) * 10) / 0x1fb;
        if (*output_mode_ptr == '\x01') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x11d7 +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x33) - *(volatile int *)output_scale_ptr;
        }
        if (*output_mode_ptr == '\x02') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x11eb +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x33) - *(volatile int *)output_scale_ptr;
        }
      }
    }
    ptr = output_scale_byte_ptr;
    if (*parameter_output_phase_ptr == '\x01') {
      if (*output_frequency_ptr == '2') {
        /* 50Hz、三相出 */
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)output_setpoint_ptr;
        out_scale = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = (uint32_t)(*(volatile int *)ptr << 7) / 100;
        ptr = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)out_scale / 100;
        if (0x3903 < *(volatile uint32_t *)ptr) {
          *(volatile uint32_t *)output_scale_ptr = 0x3903;
        }
        *(volatile uint32_t *)output_divisor_ptr =
             (uint32_t)((0x3904 - *(volatile int *)output_scale_ptr) * 10) / 0x32b;
        if (*output_mode_ptr == '\x01') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x2ab5 +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x38) - *(volatile int *)output_scale_ptr;
        }
        if (*output_mode_ptr == '\x02') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x2ac9 +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x38) - *(volatile int *)output_scale_ptr;
        }
      }
      ptr = output_scale_byte_ptr;
      if (*output_frequency_ptr == '<') {
        /* 60Hz、三相出 */
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)output_setpoint_ptr;
        out_scale = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = (uint32_t)(*(volatile int *)ptr * 0x70) / 100;
        ptr = output_scale_byte_ptr;
        *(volatile uint32_t *)output_scale_ptr = *(volatile uint32_t *)out_scale / 100;
        if (0x31e0 < *(volatile uint32_t *)ptr) {
          *(volatile uint32_t *)output_scale_ptr = 0x31e0;
        }
        *(volatile uint32_t *)output_divisor_ptr =
             (uint32_t)((0x31e1 - *(volatile int *)output_scale_ptr) * 10) / 0x2c5;
        if (*output_mode_ptr == '\x01') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x20af +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x33) - *(volatile int *)output_scale_ptr;
        }
        if (*output_mode_ptr == '\x02') {
          TIMER2->MR0 =
               (*(volatile int *)frequency_adjust_value_ptr * 10 + 0x20b9 +
               (uint32_t)(uint8_t)*parameter_master_slave_offset_ptr * 0x33) - *(volatile int *)output_scale_ptr;
        }
      }
    }
    TIMER2->TCR = 1;
  }

}

void EINT3_IRQHandler(void)
{
    SYSTEM_CONTROL->external_interrupt = SYSTEM_CONTROL->external_interrupt | 8;   /* EXTINT 清 EINT3 */
  if (*(volatile int *)output_run_state_ptr == 0) {
    if (*output_input_state_ptr == '\x01') {
      *output_mode_ptr = 1;
    }
    if (*output_input_state_ptr == '\x02') {
      *output_mode_ptr = 2;
    }
    *output_input_state_ptr = 0;
  }
  *output_eint3_flag_ptr = 1;
  output_process_phase_interrupt();
  return;
}

/* 0x0000FF48 —— TIMER2 ISR：清中断、disp_scan 复位、TIMER1 周期重载
 *   局部：timer_base = TIMER 寄存器基址（先 output_timer2_registers 后 00010030） */
void TIMER2_IRQHandler(void)
{
  TIMER2->IR = 0xff;             /* TIMER2 IR 清中断 */
  TIMER2->TCR = 2;
  *output_scan_counter_ptr = 0;
  TIMER1->TCR = 2;
  TIMER1->IR = 0xff;
  TIMER1->MR0 = 0x36;              /* MR0=0x36 触发周期 */
  TIMER1->TCR = 1;
  return;
}

/* 0x0000FF6C —— TIMER1 ISR：LCD 12864 动态扫描
 *   disp_scan(0x1000102C/0x10001058/0x10001064) 行计数（0..0xF0，每 0x28 行一组，
 *   共 4 个区域/页），按 mode_byte 与奇偶行控制 FIO1/FIO2 各 COM/SEG 位；
 *   output_timer1_registers=TIMER1 基址，MR0 按 freq_hz('2'=0x488/'<'=0x261) 或 0x36 逐行
 * 局部：gpio_base1/gpio_base2 = FIO 池各区域基址（output_fio_base/00010454/00010640，字节偏移） */
static void lcd_scan_update_rows(void)
{
  if ((*output_scan_counter_ptr != '\0') && ((uint8_t)*output_scan_counter_ptr < 0x29)) {
    /* —— 区域 0（行 1..0x28）—— */
    if (*output_mode_ptr == '\x01') {
      if ((uint32_t)(uint8_t)*output_scan_counter_ptr == ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2
         ) {
        FIO2->SET = FIO2->SET | 0x200;
        FIO0->SET = FIO0->SET | 0x80000;
        FIO2->SET = FIO2->SET | 0x20;
        FIO2->SET = FIO2->SET | 0x40;
      }
      else {
        FIO2->CLR = FIO2->CLR | 0x200;
        FIO0->CLR = FIO0->CLR | 0x80000;
        FIO2->CLR = FIO2->CLR | 0x20;
        FIO2->CLR = FIO2->CLR | 0x40;
      }
    }
    else if ((uint32_t)(uint8_t)*output_scan_counter_ptr ==
             ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2) {
      FIO0->SET = FIO0->SET | 0x20000;
      FIO0->SET = FIO0->SET | 0x10000;
      FIO2->SET = FIO2->SET | 0x80;
      FIO0->SET = FIO0->SET | 0x100;
    }
    else {
      FIO0->CLR = FIO0->CLR | 0x20000;
      FIO0->CLR = FIO0->CLR | 0x10000;
      FIO2->CLR = FIO2->CLR | 0x80;
      FIO0->CLR = FIO0->CLR | 0x100;
    }
  }
  if ((0x28 < (uint8_t)*output_scan_counter_ptr) && ((uint8_t)*output_scan_counter_ptr < 0x51)) {
    /* —— 区域 1（行 0x29..0x50）—— */
    if (*output_mode_ptr == '\x01') {
      if ((uint32_t)(uint8_t)*output_scan_counter_ptr == ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2
         ) {
        FIO0->SET = FIO0->SET | 0x10000;
        FIO0->SET = FIO0->SET | 0x80000;
        FIO0->SET = FIO0->SET | 0x100;
        FIO2->SET = FIO2->SET | 0x40;
      }
      else {
        FIO0->CLR = FIO0->CLR | 0x10000;
        FIO0->CLR = FIO0->CLR | 0x80000;
        FIO0->CLR = FIO0->CLR | 0x100;
        FIO2->CLR = FIO2->CLR | 0x40;
      }
    }
    else if ((uint32_t)(uint8_t)*output_scan_counter_ptr ==
             ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2) {
      FIO0->SET = FIO0->SET | 0x80000;
      FIO0->SET = FIO0->SET | 0x10000;
      FIO2->SET = FIO2->SET | 0x40;
      FIO0->SET = FIO0->SET | 0x100;
    }
    else {
      FIO0->CLR = FIO0->CLR | 0x80000;
      FIO0->CLR = FIO0->CLR | 0x10000;
      FIO2->CLR = FIO2->CLR | 0x40;
      FIO0->CLR = FIO0->CLR | 0x100;
    }
  }
  if ((0x50 < (uint8_t)*output_scan_counter_ptr) && ((uint8_t)*output_scan_counter_ptr < 0x79)) {
    /* —— 区域 2（行 0x51..0x78）—— */
    if (*output_mode_ptr == '\x01') {
      if ((uint32_t)(uint8_t)*output_scan_counter_ptr == ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2
         ) {
        FIO0->SET = FIO0->SET | 0x10000;
        FIO0->SET = FIO0->SET | 0x20000;
        FIO0->SET = FIO0->SET | 0x100;
        FIO2->SET = FIO2->SET | 0x80;
      }
      else {
        FIO0->CLR = FIO0->CLR | 0x10000;
        FIO0->CLR = FIO0->CLR | 0x20000;
        FIO0->CLR = FIO0->CLR | 0x100;
        FIO2->CLR = FIO2->CLR | 0x80;
      }
    }
    else if ((uint32_t)(uint8_t)*output_scan_counter_ptr ==
             ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2) {
      FIO0->SET = FIO0->SET | 0x80000;
      FIO2->SET = FIO2->SET | 0x200;
      FIO2->SET = FIO2->SET | 0x40;
      FIO2->SET = FIO2->SET | 0x20;
    }
    else {
      FIO0->CLR = FIO0->CLR | 0x80000;
      FIO2->CLR = FIO2->CLR | 0x200;
      FIO2->CLR = FIO2->CLR | 0x40;
      FIO2->CLR = FIO2->CLR | 0x20;
    }
  }
  if ((0x78 < (uint8_t)*output_scan_counter_ptr) && ((uint8_t)*output_scan_counter_ptr < 0xa1)) {
    /* —— 区域 3（行 0x79..0xA0）—— */
    if (*output_mode_ptr == '\x01') {
      if ((uint32_t)(uint8_t)*output_scan_counter_ptr == ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2
         ) {
        FIO0->SET = FIO0->SET | 0x8000;
        FIO0->SET = FIO0->SET | 0x20000;
        FIO0->SET = FIO0->SET | 0x80;
        FIO2->SET = FIO2->SET | 0x80;
      }
      else {
        FIO0->CLR = FIO0->CLR | 0x8000;
        FIO0->CLR = FIO0->CLR | 0x20000;
        FIO0->CLR = FIO0->CLR | 0x80;
        FIO2->CLR = FIO2->CLR | 0x80;
      }
    }
    else if ((uint32_t)(uint8_t)*output_scan_counter_ptr ==
             ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2) {
      FIO0->SET = FIO0->SET | 0x40000;
      FIO2->SET = FIO2->SET | 0x200;
      FIO2->SET = FIO2->SET | 0x100;
      FIO2->SET = FIO2->SET | 0x20;
    }
    else {
      FIO0->CLR = FIO0->CLR | 0x40000;
      FIO2->CLR = FIO2->CLR | 0x200;
      FIO2->CLR = FIO2->CLR | 0x100;
      FIO2->CLR = FIO2->CLR | 0x20;
    }
  }
  if ((0xa0 < (uint8_t)*output_scan_counter_ptr) && ((uint8_t)*output_scan_counter_ptr < 0xc9)) {
    /* —— 区域 4（行 0xA1..0xC8）—— */
    if (*output_mode_ptr == '\x01') {
      if ((uint32_t)(uint8_t)*output_scan_counter_ptr == ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2
         ) {
        FIO0->SET = FIO0->SET | 0x8000;
        FIO0->SET = FIO0->SET | 0x40000;
        FIO0->SET = FIO0->SET | 0x80;
        FIO2->SET = FIO2->SET | 0x100;
      }
      else {
        FIO0->CLR = FIO0->CLR | 0x8000;
        FIO0->CLR = FIO0->CLR | 0x40000;
        FIO0->CLR = FIO0->CLR | 0x80;
        FIO2->CLR = FIO2->CLR | 0x100;
      }
    }
    else if ((uint32_t)(uint8_t)*output_scan_counter_ptr ==
             ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2) {
      FIO0->SET = FIO0->SET | 0x40000;
      FIO0->SET = FIO0->SET | 0x8000;
      FIO2->SET = FIO2->SET | 0x100;
      FIO0->SET = FIO0->SET | 0x80;
    }
    else {
      FIO0->CLR = FIO0->CLR | 0x40000;
      FIO0->CLR = FIO0->CLR | 0x8000;
      FIO2->CLR = FIO2->CLR | 0x100;
      FIO0->CLR = FIO0->CLR | 0x80;
    }
  }
  if ((200 < (uint8_t)*output_scan_counter_ptr) && ((uint8_t)*output_scan_counter_ptr < 0xf1)) {
    /* —— 区域 5（行 0xC9..0xF0）—— */
    if (*output_mode_ptr == '\x01') {
      if ((uint32_t)(uint8_t)*output_scan_counter_ptr == ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2
         ) {
        FIO2->SET = FIO2->SET | 0x200;
        FIO0->SET = FIO0->SET | 0x40000;
        FIO2->SET = FIO2->SET | 0x20;
        FIO2->SET = FIO2->SET | 0x100;
      }
      else {
        FIO2->CLR = FIO2->CLR | 0x200;
        FIO0->CLR = FIO0->CLR | 0x40000;
        FIO2->CLR = FIO2->CLR | 0x20;
        FIO2->CLR = FIO2->CLR | 0x100;
      }
    }
    else if ((uint32_t)(uint8_t)*output_scan_counter_ptr ==
             ((int)(uint32_t)(uint8_t)*output_scan_counter_ptr >> 1) * 2) {
      FIO0->SET = FIO0->SET | 0x20000;
      FIO0->SET = FIO0->SET | 0x8000;
      FIO2->SET = FIO2->SET | 0x80;
      FIO0->SET = FIO0->SET | 0x80;
    }
    else {
      FIO0->CLR = FIO0->CLR | 0x20000;
      FIO0->CLR = FIO0->CLR | 0x8000;
      FIO2->CLR = FIO2->CLR | 0x80;
      FIO0->CLR = FIO0->CLR | 0x80;
    }
  }
  if (0xf0 < (uint8_t)*output_scan_counter_ptr) {
    *output_scan_counter_ptr = 0;
  }

}

void TIMER1_IRQHandler(void)
{
  volatile uint8_t *scan_counter_ptr;

  TIMER1->IR = 0xff;
  TIMER1->TCR = 2;
  scan_counter_ptr = output_scan_counter_ptr;
  *output_scan_counter_ptr = *output_scan_counter_ptr + '\x01';
  if ((uint32_t)(uint8_t)*scan_counter_ptr == ((uint8_t)*scan_counter_ptr / 0x28) * 0x28) {
    if (*output_frequency_ptr == '2') {
      TIMER1->MR0 = 0x488;
    }
    if (*output_frequency_ptr == '<') {
      TIMER1->MR0 = 0x261;
    }
  }
  else {
    TIMER1->MR0 = 0x36;
  }
  if ((uint8_t)*output_scan_counter_ptr < 0xf1) {
    TIMER1->TCR = 1;
  }

  lcd_scan_update_rows();
  return;
}
