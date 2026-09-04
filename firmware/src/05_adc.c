/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 05：ADC0 多通道采样 + 标定换算
 *
 * 采样链（三相电源反馈 + 给定）：
 *   · ADC0（AD0CR 0x40034000）：adc0_start 置 START bit、adc0_wait_done
 *     等 GDR bit31 DONE 后读 12 位（&0xffff >> 4）
 *   · 逐通道扫描，通道轮转计数 0x10002314（0..5）：
 *       ch2=IA(三相电流A, SEL=4)、ch1=IB(SEL=2)、ch0=IC(SEL=1)、ch5=Ug 给定(SEL=0x20)、
 *       ch3=IF(SEL=8)、ch4=Uf(SEL=0x10)   —— 每通道 5 点原始采样存 0x10002318..0x1000232C
 *   · 每轮转满 5 点求 5 点平均（ch5 再求 10 点平均），按互感器比(0x1000233C=param4)
 *     与 ADC 标定除数（0x10002340 等）换算 → 电流/电压反馈（→ reg40 Ug/reg42 IB/reg43 IC/
 *     reg44 IF/reg45 Uf，见 MENU_PARAMETER_MAPPING.md）
 * 导出：2026-08-21（L0 语义化：时钟、采样值和输出指针均已按实际角色命名）
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_state.h"

/* 0x00001F04 —— ADC0 初始化：CLKDIV、PCLKSEL（ADC 时钟分频）、使能 ADC 电源
 *   adc_clock_base=时钟/PCLKSEL 基址（+4 PCLK ADC=CCLK、+0xC 分频/CKLK 位）、
 *   adc_scb_base=SCB 0x400FC000（+0xC4 PCONP 上电 bit12 ADC）、AD0CR 初值 0x00201820 */
void adc_init(void)
{
  int clock_base;

  clock_base = adc_clock_base;
  *(volatile uint32_t *)(adc_clock_base + 4) = *(volatile uint32_t *)(adc_clock_base + 4) & 0xffc03fff;
  *(volatile uint32_t *)(clock_base + 4) = *(volatile uint32_t *)(clock_base + 4) | 0x154000;      /* PCLK ADC=CCLK */
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) & 0xcfffffff;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) | 0x30000000;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) & 0xcfffffff;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) | 0x30000000;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) & 0x3fffffff;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) | 0xc0000000;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) & 0x3fffffff;
  *(volatile uint32_t *)(clock_base + 0xc) = *(volatile uint32_t *)(clock_base + 0xc) | 0xc0000000;
  *(volatile uint32_t *)(adc_scb_base + 0xc4) = *system_pconp | 0x1000;      /* PCONP ADC 上电 */
  *adc_control_base = adc_control_default;                     /* AD0CR 初值 */
  return;
}

/* 0x00001F80 —— ADC0 启动转换（CR bit27=START） */
void adc0_start(void)
{
  volatile uint32_t *adc_control_reg;

  adc_control_reg = adc_control_base;
  *adc_control_base = *adc_control_base & 0xf8ffffff;
  *adc_control_reg = *adc_control_reg | 0x1000000;
  return;
}

/* 0x00001FA6 —— 等转换完成（GDR bit31 DONE）并返回 12 位结果（>>4）
 * 注意：AD0CR 是 uint32_t*，不能直接 +4（会按元素偏移 +16 字节读到 AD0DR0）。
 * 必须先转整数做字节偏移，才能读到 AD0GDR(0x40034004)。 */
uint32_t adc0_wait_done(void)
{
  do {
  } while ((*(volatile uint32_t *)((uint32_t)adc_control_base + 4) & 0x80000000) == 0);
  return (*(volatile uint32_t *)((uint32_t)adc_control_base + 4) & 0xffff) >> 4;
}

/* 0x00001FBC —— 逐通道扫描：每通道 5 点循环采样存原始数组，
 *   各通道在对应索引点（0..5 轮转）计算 5 点平均并做标定换算
 *   0x10002314=通道轮转计数（0..5）；0x10002330/4C/5C/70/A0/C0=各通道平均索引
 *   （换算公式 0x1000233C=param4 互感器比，0x10002340 等=ADC 标定除数）
 * 局部变量角色（反编译寄存器复用，请注意跨段复用）：
 *   value_ptr   — 复用：采样段=AD0CR；平均段=中间换算值指针（0x10002338/0x10002588）
 *   output_value_ptr   — 标定输出值指针（0x10002344/58/6C/94 等）
 *   average_index_ptr   — 通道平均索引计数器（0x10002330/4C/5C/70/A0/C0）
 *   sample_value    — 复用：采样段=ADC 原始 12 位结果；平均段=5 点平均数组基址
 *   raw_sample  — ADC 原始结果（写入 32 位缓冲 ch3/ch4） */
void adc0_scan_channels(void)
{
  volatile uint32_t *value_ptr;
  volatile uint8_t *average_index_ptr;
  volatile uint32_t *output_value_ptr;
  volatile uint32_t *reference_average_buffer;
  int sample_value;
  uint32_t raw_sample;

  average_index_ptr = adc_scan_channel_ptr;
  *adc_scan_channel_ptr = *adc_scan_channel_ptr + 1;
  if (5 < *average_index_ptr) {
    *average_index_ptr = 0;
  }
  /* —— ch2（SEL=4，IA）—— */
  value_ptr = adc_control_base;
  *adc_control_base = *adc_control_base & 0xffffffc0;
  *value_ptr = *value_ptr | 4;
  adc0_start();
  sample_value = adc0_wait_done();
  adc_samples_current_ptr[*adc_scan_channel_ptr] = sample_value;
  /* —— ch1（SEL=2，IB）—— */
  value_ptr = adc_control_base;
  *adc_control_base = *adc_control_base & 0xffffffc0;
  *value_ptr = *value_ptr | 2;
  adc0_start();
  sample_value = adc0_wait_done();
  adc_samples_voltage_ptr[*adc_scan_channel_ptr] = sample_value;
  /* —— ch0（SEL=1，IC）—— */
  value_ptr = adc_control_base;
  *adc_control_base = *adc_control_base & 0xffffffc0;
  *value_ptr = *value_ptr | 1;
  adc0_start();
  sample_value = adc0_wait_done();
  adc_samples_current_c_ptr[*adc_scan_channel_ptr] = sample_value;
  /* —— ch5（SEL=0x20，Ug 给定）—— */
  value_ptr = adc_control_base;
  *adc_control_base = *adc_control_base & 0xffffffc0;
  *value_ptr = *value_ptr | 0x20;
  adc0_start();
  sample_value = adc0_wait_done();
  adc_samples_reference_ptr[*adc_scan_channel_ptr] = sample_value;
  /* —— ch3（SEL=8，IF）—— */
  value_ptr = adc_control_base;
  *adc_control_base = *adc_control_base & 0xffffffc0;
  *value_ptr = *value_ptr | 8;
  adc0_start();
  raw_sample = adc0_wait_done();
  *(volatile uint32_t *)(adc_samples_field_ptr + (uint32_t)*adc_scan_channel_ptr * 4) = raw_sample;
  /* —— ch4（SEL=0x10，Uf）—— */
  value_ptr = adc_control_base;
  *adc_control_base = *adc_control_base & 0xffffffc0;
  *value_ptr = *value_ptr | 0x10;
  adc0_start();
  raw_sample = adc0_wait_done();
  *(volatile uint32_t *)(adc_samples_voltage_f_ptr + (uint32_t)*adc_scan_channel_ptr * 4) = raw_sample;

  /* —— ch2 平均（每轮转满 5 点计算）—— */
  average_index_ptr = adc_average_index_a_ptr;
  if (*adc_scan_channel_ptr == 0) {
    *adc_average_index_a_ptr = *adc_average_index_a_ptr + 1;
    if (9 < *average_index_ptr) {
      *average_index_ptr = 0;
    }
    sample_value = adc_average_buffer_a_ptr;
    *(volatile uint32_t *)(adc_average_buffer_a_ptr + (uint32_t)*adc_average_index_a_ptr * 4) =
         (uint32_t)(*adc_samples_current_ptr + adc_samples_current_ptr[1] + adc_samples_current_ptr[2] + adc_samples_current_ptr[3] +
               adc_samples_current_ptr[4]) / 5;
    value_ptr = adc_average_work_ptr;
    *adc_average_work_ptr = *(volatile uint32_t *)(sample_value + (uint32_t)*adc_average_index_a_ptr * 4);
    output_value_ptr = adc_output_current_a_ptr;
    *adc_output_current_a_ptr = (*adc_current_scale_ptr * *value_ptr * 2) / *adc_current_divisor_ptr;
    if ((*parameter_output_mode_ptr == '\0') && (*output_value_ptr < 10)) {
      *output_value_ptr = 0;
    }
  }
  /* —— ch1 平均（→ reg42 IB）—— */
  average_index_ptr = adc_average_index_b_ptr;
  if (*adc_scan_channel_ptr == 1) {
    *adc_average_index_b_ptr = *adc_average_index_b_ptr + 1;
    if (9 < *average_index_ptr) {
      *average_index_ptr = 0;
    }
    sample_value = adc_average_buffer_b_ptr;
    *(volatile uint32_t *)(adc_average_buffer_b_ptr + (uint32_t)*adc_average_index_b_ptr * 4) =
         (uint32_t)(*adc_samples_voltage_ptr + adc_samples_voltage_ptr[1] + adc_samples_voltage_ptr[2] + adc_samples_voltage_ptr[3] +
               adc_samples_voltage_ptr[4]) / 5;
    value_ptr = adc_average_work_ptr;
    *adc_average_work_ptr = *(volatile uint32_t *)(sample_value + (uint32_t)*adc_average_index_b_ptr * 4);
    output_value_ptr = adc_output_current_b_ptr;
    *adc_output_current_b_ptr = (*adc_current_scale_ptr * *value_ptr * 2) / *adc_current_b_divisor_ptr;
    if ((*parameter_output_mode_ptr == '\0') && (*output_value_ptr < 10)) {
      *output_value_ptr = 0;
    }
  }
  /* —— ch0 平均（→ reg43 IC）—— */
  average_index_ptr = adc_average_index_c_ptr;
  if (*adc_scan_channel_ptr == 2) {
    *adc_average_index_c_ptr = *adc_average_index_c_ptr + 1;
    if (9 < *average_index_ptr) {
      *average_index_ptr = 0;
    }
    sample_value = adc_average_buffer_c_ptr;
    *(volatile uint32_t *)(adc_average_buffer_c_ptr + (uint32_t)*adc_average_index_c_ptr * 4) =
         (uint32_t)(*adc_samples_current_c_ptr + adc_samples_current_c_ptr[1] + adc_samples_current_c_ptr[2] + adc_samples_current_c_ptr[3] +
               adc_samples_current_c_ptr[4]) / 5;
    value_ptr = adc_average_work_ptr;
    *adc_average_work_ptr = *(volatile uint32_t *)(sample_value + (uint32_t)*adc_average_index_c_ptr * 4);
    *adc_output_current_c_raw_ptr = *value_ptr;
    value_ptr = adc_output_current_c_ptr;
    *adc_output_current_c_ptr = (*adc_current_scale_ptr * *adc_average_work_ptr * 2) / *adc_current_c_divisor_ptr;
    if ((*parameter_output_mode_ptr == '\0') && (*value_ptr < 10)) {
      *value_ptr = 0;
    }
  }
  /* —— ch5 平均（→ reg40 读回源 Ug）—— */
  average_index_ptr = adc_average_index_ref_ptr;
  if (*adc_scan_channel_ptr == 3) {
    *adc_average_index_ref_ptr = *adc_average_index_ref_ptr + 1;
    if (9 < *average_index_ptr) {
      *average_index_ptr = 0;
    }
    reference_average_buffer = adc_average_buffer_ref_ptr;
    adc_average_buffer_ref_ptr[*adc_average_index_ref_ptr] =
         (uint32_t)(*adc_samples_reference_ptr + adc_samples_reference_ptr[1] + adc_samples_reference_ptr[2] + adc_samples_reference_ptr[3] +
               adc_samples_reference_ptr[4]) / 5;
    value_ptr = adc_average_work_ptr;
    *adc_average_work_ptr =
         (uint32_t)(*reference_average_buffer + reference_average_buffer[1] + adc_average_buffer_ref_ptr[2] + adc_average_buffer_ref_ptr[3] + adc_average_buffer_ref_ptr[4] +
                adc_average_buffer_ref_ptr[5] + adc_average_buffer_ref_ptr[6] + adc_average_buffer_ref_ptr[7] + adc_average_buffer_ref_ptr[8] +
               adc_average_buffer_ref_ptr[9]) / 10;
    output_value_ptr = adc_output_reference_ptr;
    *adc_output_reference_ptr = (*value_ptr * 0x65) / 400;      /* ×101/400 缩放 */
    if (*adc_reference_mode_ptr == '\0') {
      if (1000 < *output_value_ptr) {
        *output_value_ptr = 1000;
      }
      if (*adc_reference_output_ptr < 10) {
        *adc_reference_output_ptr = 0;
      }
    }
    if (*adc_reference_calibration_ptr == '\x01') {
      if (*adc_reference_output_ptr < 0xcd) {
        *adc_reference_output_ptr = 0;
      }
      value_ptr = adc_reference_output_ptr;
      if (0xcc < *adc_reference_output_ptr) {
        *adc_reference_output_ptr = (*adc_reference_output_ptr - 200) * 5 >> 2;
        if (1000 < *value_ptr) {
          *value_ptr = 1000;
        }
        if (*adc_reference_output_ptr < 10) {
          *adc_reference_output_ptr = 0;
        }
      }
      *adc_output_reference_raw_ptr = (*adc_output_reference_raw_ptr - 800) * 5 >> 2;
    }
    if (*parameter_control_mode_ptr == '\0') {
      *adc_gain_b_output_ptr = (*parameter_voltage_range_ptr * *adc_output_reference_raw_ptr) / 0xf78;    /* /3960 */
    }
    if (*parameter_control_mode_ptr == '\x01') {
      *adc_gain_b_output_ptr = (*parameter_current_range_ptr * *adc_output_reference_raw_ptr) / 0xf78;
    }
  }
  /* —— ch3 平均（→ reg44 IF；仅当 0x1000259C==4）—— */
  average_index_ptr = adc_field_average_index_ptr;
  if (*adc_field_mode_ptr == '\x04') {
    *adc_field_average_index_ptr = *adc_field_average_index_ptr + 1;
    if (9 < *average_index_ptr) {
      *average_index_ptr = 0;
    }
    sample_value = adc_field_average_buffer_ptr;
    *(volatile uint32_t *)(adc_field_average_buffer_ptr + (uint32_t)*adc_field_average_index_ptr * 4) =
         (uint32_t)(*adc_field_samples_ptr + adc_field_samples_ptr[1] + adc_field_samples_ptr[2] + adc_field_samples_ptr[3] +
               adc_field_samples_ptr[4]) / 5;
    value_ptr = adc_output_reference_raw_ptr;
    *adc_output_reference_raw_ptr = *(volatile uint32_t *)(sample_value + (uint32_t)*adc_field_average_index_ptr * 4);
    *adc_field_output_raw_ptr = (*value_ptr * 0x65) / 400;
    value_ptr = adc_output_reference_raw_ptr;
    *adc_output_reference_raw_ptr = (*parameter_current_range_ptr * *adc_output_reference_raw_ptr) / *adc_field_divisor_ptr;   /* gain_b/reg54 */
    *adc_field_output_ptr = *value_ptr;
    *adc_field_output_scaled_ptr = *adc_output_reference_raw_ptr;
    if ((*parameter_output_mode_ptr == '\0') && (*adc_field_output_ptr < 10)) {
      *adc_field_output_ptr = 0;
    }
  }
  /* —— ch4 平均（→ reg45 Uf；仅当 0x1000259C==5）—— */
  average_index_ptr = adc_voltage_average_index_ptr;
  if (*adc_field_mode_ptr == '\x05') {
    *adc_voltage_average_index_ptr = *adc_voltage_average_index_ptr + 1;
    if (9 < *average_index_ptr) {
      *average_index_ptr = 0;
    }
    sample_value = adc_voltage_average_buf_ptr;
    *(volatile uint32_t *)(adc_voltage_average_buf_ptr + (uint32_t)*adc_voltage_average_index_ptr * 4) =
         (uint32_t)(*adc_voltage_samples_ptr + adc_voltage_samples_ptr[1] + adc_voltage_samples_ptr[2] + adc_voltage_samples_ptr[3] +
               adc_voltage_samples_ptr[4]) / 5;
    value_ptr = adc_output_reference_raw_ptr;
    *adc_output_reference_raw_ptr = *(volatile uint32_t *)(sample_value + (uint32_t)*adc_voltage_average_index_ptr * 4);
    *adc_voltage_output_raw_ptr = (*value_ptr * 0x65) / 400;
    value_ptr = adc_output_reference_raw_ptr;
    *adc_output_reference_raw_ptr = (*parameter_voltage_range_ptr * *adc_output_reference_raw_ptr) / *adc_voltage_divisor_ptr;   /* gain_a/reg55 */
    *adc_voltage_output_ptr = *value_ptr;
    *adc_voltage_output_scaled_ptr = *adc_output_reference_raw_ptr;
    if ((*parameter_output_mode_ptr == '\0') && (*adc_voltage_output_ptr < 10)) {
      *adc_voltage_output_ptr = 0;
    }
  }
  return;
}
