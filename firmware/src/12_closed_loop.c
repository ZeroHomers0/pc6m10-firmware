/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 12：闭环 PID（位置式/积分式，三路）
 *
 * closed_loop_integral：位置式 PID 计算，工作区在 0x100020D0 起的 RAM 全局。
 *   · 给定/反馈误差分两种符号路径，各按死区三段（上界/带内/下界）选择输出系数；
 *   · 分段除数表按误差绝对值选除数（误差越大除数越小 → 步进越快）；
 *   · 末段公式算位置式输出（0x10002130），并累加（0x10002120），
 *     上限钳 0x00116520、下限钳 0x0005CC60。
 * closed_loop_wrapper：0x100020F4 计数节流——计数清零才重算并缓存 0x1000212C，
 *   其余调用直接返回缓存（相当于按固定周期重算）。
 *
 * 调用点：src/09_output_stage.c 两处
 *   closed_loop_wrapper(setpoint,feedback,coef_a,coef_b) → 0x1000F2C4 / 0x1000F760；
 *   控制方式（0x10001634）选择走哪条通道，闭环输出随后做上下限钳位。
 *
 * 说明：globals 中 PID 工作区 已按访问语义分型（ptr_word / value）；
 *       p_pid / pid_increment_ptr 原反编译为 int*，此处按 volatile uint32_t* 用
 *       （消除 "discards volatile" 警告，访问语义不变）。
 * 导出：2026-08-21
 *
 * 交叉引用：
 *   · 触发/PID 全图 → docs/PROGRESS_2026-08-20.md、docs/state_machine_analysis.md
 *   · 闭环调用与钳位上下文 → src/09_output_stage.c
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"

#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include "inc/firmware_parameters.h"
#include "inc/consts.h"

/* 0x000108B0 —— 位置式 PID（积分）闭环计算：给定 setpoint 与反馈 feedback 求误差，
 *   按误差符号分别用死区三段（上界 / 带内 / 下界）选输出系数；再按控制方式
 *   选分段除数表（误差越大除数越小 → 步进越快）；最后算位置式输出并累加、钳位。
 *   参数：setpoint=给定、feedback=反馈、coef_a/coef_b=PID 系数（写入 PID 工作槽，
 *   具体系数含义待核实）。p_pid 为工作寄存器指针（不同阶段指向不同寄存器，见内联），
 *   pid_increment_ptr 指向位置增量输出。 */
int closed_loop_integral(int setpoint,int feedback,uint32_t coef_a,uint32_t coef_b)
{
  volatile uint32_t *p_pid;
  volatile uint32_t *pid_increment_ptr;

  *pid_coefficient_p_ptr = coef_a;                  /* PID 系数槽 A（=0x10002100，见公式 coef_a 项） */
  *pid_coefficient_i_ptr = coef_b;                  /* PID 系数槽 B（=0x10002104，见公式 coef_b 项） */
  *pid_operation_status_ptr = 3;
  *pid_setpoint_ptr = setpoint;                /* 给定 */
  *pid_feedback_ptr = feedback;                /* 反馈 */
  *pid_error_oldest_ptr = *pid_error_previous_term_ptr;           /* 上上次误差滚动 */
  *pid_error_previous_term_ptr = *pid_error_current_ptr;           /* 上次误差滚动 */
  *pid_error_current_ptr = *pid_setpoint_ptr - *pid_feedback_ptr;   /* 当前误差 */
  p_pid = pid_error_absolute;                    /* p_pid → 误差值寄存器 0x10002118 */
  if (*pid_feedback_ptr <= *pid_setpoint_ptr) {
    *pid_error_absolute = *pid_setpoint_ptr - *pid_feedback_ptr; /* 误差（正值路径） */
    if ((int)(uint32_t)*closed_loop_threshold_upper_ptr <= *p_pid) {     /* 误差 ≥ 死区上界 */
      *pid_selected_gain_ptr = (uint32_t)*closed_loop_gain_high_ptr;
    }
    if ((*pid_error_absolute < (int)(uint32_t)*closed_loop_threshold_upper_ptr) && ((int)(uint32_t)*closed_loop_threshold_lower_ptr < *pid_error_absolute)) {
      *pid_selected_gain_ptr = (uint32_t)*closed_loop_gain_mid_ptr;         /* 死区上界内 */
    }
    if (*pid_error_absolute <= (int)(uint32_t)*closed_loop_threshold_lower_ptr) {
      *pid_selected_gain_ptr = (uint32_t)*closed_loop_gain_low_ptr;         /* 误差 ≤ 死区下界 */
    }
  }
  p_pid = pid_error_absolute;                    /* p_pid → 误差值寄存器（负值路径） */
  if (*pid_setpoint_ptr < *pid_feedback_ptr) {
    *pid_error_absolute = *pid_feedback_ptr - *pid_setpoint_ptr; /* 误差（负值路径，绝对值） */
    if ((int)(uint32_t)*closed_loop_threshold_upper_ptr <= *p_pid) {
      *pid_selected_gain_ptr = (uint32_t)*closed_loop_gain_high_ptr;
    }
    if ((*pid_error_absolute < (int)(uint32_t)*closed_loop_threshold_upper_ptr) && ((int)(uint32_t)*closed_loop_threshold_lower_ptr < *pid_error_absolute)) {
      *pid_selected_gain_ptr = (uint32_t)*closed_loop_gain_mid_ptr;
    }
    if (*pid_error_absolute <= (int)(uint32_t)*closed_loop_threshold_lower_ptr) {
      *pid_selected_gain_ptr = (uint32_t)*closed_loop_gain_low_ptr;
    }
  }
  if (*parameter_control_mode_ptr == '\0') {
    /* —— 通道 1 分段除数表（误差越大除数越小 → 步进越快）—— */
    if (*parameter_voltage_range_ptr < 0xdc) { *pid_divisor_ptr = 8; }
    if ((0xdb < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 0x226)) { *pid_divisor_ptr = 0xf; }
    if ((0x225 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 1000)) { *pid_divisor_ptr = 0x1e; }
    if ((999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 0x5dc)) { *pid_divisor_ptr = 0x2a; }
    if ((0x5db < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 2000)) { *pid_divisor_ptr = 0x37; }
    if ((1999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 0x9c4)) { *pid_divisor_ptr = 0x50; }
    if ((0x9c3 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 3000)) { *pid_divisor_ptr = 100; }
    if ((2999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 4000)) { *pid_divisor_ptr = 0x78; }
    if ((3999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 5000)) { *pid_divisor_ptr = 0x96; }
    if ((4999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < RANGE_MAX)) { *pid_divisor_ptr = 0xb4; }
    if (*pid_divisor_source_ptr == 1) {
      /* —— 通道 1 另套表（0x10010CFC 为误差源）—— */
      if (*parameter_current_range_ptr < 0xdc) { *pid_divisor_ptr = 8; }
      if ((0xdb < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 0x226)) { *pid_divisor_ptr = 0xf; }
      if ((0x225 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 1000)) { *pid_divisor_ptr = 0x1e; }
      if ((999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 0x5dc)) { *pid_divisor_ptr = 0x2a; }
      if ((0x5db < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 2000)) { *pid_divisor_ptr = 0x37; }
      if ((1999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 0x9c4)) { *pid_divisor_ptr = 0x50; }
      if ((0x9c3 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 3000)) { *pid_divisor_ptr = 100; }
      if ((2999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 4000)) { *pid_divisor_ptr = 0x78; }
      if ((3999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 5000)) { *pid_divisor_ptr = 0x96; }
      if ((4999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < RANGE_MAX)) { *pid_divisor_ptr = 0xb4; }
    }
  }
  if (*parameter_control_mode_ptr == '\x01') {
    /* —— 通道 2 分段除数表（0x10010F40）—— */
    if (*parameter_current_range_ptr < 0xdc) { *pid_divisor_ptr = 8; }
    if ((0xdb < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 0x226)) { *pid_divisor_ptr = 0xf; }
    if ((0x225 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 1000)) { *pid_divisor_ptr = 0x1e; }
    if ((999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 0x5dc)) { *pid_divisor_ptr = 0x2a; }
    if ((0x5db < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 2000)) { *pid_divisor_ptr = 0x37; }
    if ((1999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 0x9c4)) { *pid_divisor_ptr = 0x50; }
    if ((0x9c3 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 3000)) { *pid_divisor_ptr = 100; }
    if ((2999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 4000)) { *pid_divisor_channel_2_ptr = 0x78; }
    if ((3999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < 5000)) { *pid_divisor_channel_2_ptr = 0x96; }
    if ((4999 < *parameter_current_range_ptr) && (*parameter_current_range_ptr < RANGE_MAX)) { *pid_divisor_channel_2_ptr = 0xb4; }
    if (*pid_divisor_source_ch2_ptr == 1) {
      if (*parameter_voltage_range_ptr < 0xdc) { *pid_divisor_channel_2_ptr = 8; }
      if ((0xdb < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 0x226)) { *pid_divisor_channel_2_ptr = 0xf; }
      if ((0x225 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 1000)) { *pid_divisor_channel_2_ptr = 0x1e; }
      if ((999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 0x5dc)) { *pid_divisor_channel_2_ptr = 0x2a; }
      if ((0x5db < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 2000)) { *pid_divisor_channel_2_ptr = 0x37; }
      if ((1999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 0x9c4)) { *pid_divisor_channel_2_ptr = 0x50; }
      if ((0x9c3 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 3000)) { *pid_divisor_channel_2_ptr = 100; }
      if ((2999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 4000)) { *pid_divisor_channel_2_ptr = 0x78; }
      if ((3999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < 5000)) { *pid_divisor_channel_2_ptr = 0x96; }
      if ((4999 < *parameter_voltage_range_ptr) && (*parameter_voltage_range_ptr < RANGE_MAX)) { *pid_divisor_channel_2_ptr = 0xb4; }
    }
  }
  if (*parameter_control_mode_ptr == '\x02') {
    *pid_divisor_channel_2_ptr = 0x46;                       /* 控制方式 2：固定除数 0x46 */
  }
  p_pid = pid_gain_register_ptr;                    /* p_pid → 增益寄存器 0x10002124（置 1） */
  *pid_gain_register_ptr = 1;
  pid_increment_ptr = pid_output_increment_ptr;                /* 位置增量输出 0x10002130 */
  *pid_output_increment_ptr =
       /* 位置式 PID 分子：三项分别对应比例/积分/微分贡献（系数见各参数槽位）。
        * 原机码对该误差链按【带符号】算术：误差/上次/上上次误差为带符号量（int32），
        * 末段用 Cortex-M3 SDIV 做符号除法。若按无符号算，负误差 0xFFFFFE70 被当正数，
        * 分子回环成大正数 → 输出/钳位方向与金标准背离（W7 差分测试 已证实）。
        * 故对参与公式的各误差槽按 int32 取读、末段除按 int32；钳位比较也按符号。 */
       (int32_t)((int32_t)*pid_coefficient_d_ptr * (int32_t)*pid_selected_gain_ptr * 10 *
                 ((int32_t)*pid_error_current_ptr + (int32_t)*pid_error_previous_term_ptr * -2 + (int32_t)*pid_error_oldest_ptr) +
                 (int32_t)*pid_coefficient_p_ptr * (int32_t)*pid_selected_gain_ptr * 2 * (int32_t)*pid_error_current_ptr +
                 ((int32_t)*pid_error_current_ptr - (int32_t)*pid_error_previous_term_ptr) * (int32_t)*pid_coefficient_i_ptr *
                   (int32_t)*p_pid * (int32_t)*pid_selected_gain_ptr * 2) /
       (int32_t)*pid_divisor_channel_2_ptr;              /* 除以本次选定的除数（误差分段表，SDIV） */
  p_pid = pid_accumulator_ptr;                    /* p_pid → 累加器 0x10002120 */
  *pid_accumulator_ptr = *pid_accumulator_ptr + *pid_increment_ptr;      /* 位置式累加（补码加，位结果一致） */
  if ((int32_t)pid_accumulator_max < (int32_t)*p_pid) {   /* 上限钳位 0x00116520（符号比较） */
    *pid_accumulator_ptr = pid_accumulator_max;
  }
  if ((int32_t)*pid_accumulator_ptr < (int32_t)pid_accumulator_min) {   /* 下限钳位 0x0005CC60（符号比较） */
    *pid_accumulator_ptr = pid_accumulator_min;
  }
  return *pid_accumulator_ptr;
}

/* 0x00010F0A —— 闭环节流包装：每次调用把 *pid_recalculate_counter_ptr（0x100020F4，重算计数）加 1，
 *   计数非 0 时才清零并真正调用一次 closed_loop_integral()，其输出缓存到
 *   *pid_cached_output_ptr（0x1000212C）；中间各次调用直接返回上次缓存值，
 *   从而把 PID 重算节流到固定周期（每 N 个 tick 一次）。
 *   参数含义与 closed_loop_integral 一致：setpoint=给定、feedback=反馈、
 *   coef_a/coef_b=PID 系数槽。 */
uint32_t
closed_loop_wrapper(uint32_t setpoint,uint32_t feedback,uint32_t coef_a,uint32_t coef_b)
{
  volatile uint32_t *recalculation_counter_ptr;
  uint32_t result;

  recalculation_counter_ptr = pid_recalculate_counter_ptr;
  *pid_recalculate_counter_ptr = *pid_recalculate_counter_ptr + 1;
  if (*recalculation_counter_ptr != 0) {
    *recalculation_counter_ptr = 0;
    result = closed_loop_integral(setpoint,feedback,coef_a,coef_b);
    *pid_cached_output_ptr = result;
  }
  return *pid_cached_output_ptr;
}
