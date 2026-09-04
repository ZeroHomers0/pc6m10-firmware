/* =============================================================================
 * 07_state_machine.c — state_machine(0x458C) C 级还原
 * 目标B W1b：替换 firmware/stub.c 占位。依据 tools/_sm_case*.txt（Ghidra
 *   disassemble_function 全量反汇编落盘）逐段还原；数据地址一律以反汇编字面量
 *   SRAM 值为准，字符串参数（disp_string 第1参）直传原 flash 地址（flash XIP 直读，
 *   字符串→运行期地址映射属 W7 遗留）。绝不臆造：每 case 均对照对应反汇编段。
 *
 * 函数：0x0000458C-0xAB44（UI 状态机主分发，ui_screen_id_ptr 驱动）
 * 调用点：main() 主循环 state_machine(*key_code)
 * 分发链（顺序 if 级联，遇 return 即返回；历史说明见 docs/history/_SM_W1B_PROGRESS.md）：
 *   entry(0x458C)→case1(0x4B16)→caseA(0x541C)→case62(0x5572)→case63(0x5748)
 *   →case2(0x6134)→case3(0x69D6)→case4(0x7C1A)→case5(0x8780)→case6(0x8C1A)
 *   →case7(0x910C)→case8(0x9A84)→caseB(0x9C5C)→case9(0x9D86)→case5A(0x9E14)
 *   →caseC(0x9FB8)→case14(0xA04E)→case1E(0xA2C8)→0xAB44 返回。
 *   ui_screen_id_ptr 值→case：1→case1、0xa→caseA、0x62→case62、0x63→case63、2→case2、3→case3、
 *   4→case4、5→case5、6→case6、7→case7、8→case8、0xb→caseB、9→case9、0x5a→case5A、
 *   0xc→caseC、0x14→case14、0x1e→case1E。
 *
 * r4=key_code 语义：1=确认、2=DOWN/减、3=UP/加、4=SET/退出、5=启动、6=停机、
 *   0x16=快加、0x21=快减、0x17=统计清零、0xe=初始参数密码、数字键0-9输密码。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include "inc/firmware_parameters.h"

/* 运行时状态、参数和外设地址统一由 firmware_state.h / firmware_parameters.h 提供。 */

/* =============================================================================
 * case3 当前项值渲染 (0x7458-0x7A32 精简)：按项号 item_index 显示其值/枚举到 (row,0xb)。
 * 所有值串地址与枚举宽度均经 LPC1765.bin 校验，禁止臆造。
 * ========================================================================== */
static void sm3_draw_item(uint32_t item_index, uint32_t row, uint32_t attr)
{
  switch (item_index) {
    case 0:
      if (*parameter_control_mode_ptr == 0) { disp_string((int)0x6594, row, 0xb, attr); fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); }
      else if (*parameter_control_mode_ptr == 1) { disp_string((int)0x659c, row, 0xb, attr); fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); }
      else { disp_string((int)0x65a4, row, 0xb, attr); fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); }
      break;
    case 1: disp_uint4(*parameter_voltage_range_ptr, row, 0xb, attr); break;
    case 2: disp_uint4(*parameter_current_range_ptr, row, 0xb, attr); break;
    case 3: disp_uint4(*parameter_transformer_ratio_ptr, row, 0xb, attr); break;
    case 4: disp_uint4(*parameter_voltage_limit_ptr, row, 0xb, attr); break;
    case 5: disp_uint4(*parameter_current_limit_ptr, row, 0xb, attr); break;
    case 6: disp_number3(*parameter_soft_start_time_ptr, row, 0xb, attr); break;
    case 7: disp_number3(*parameter_soft_stop_time_ptr, row, 0xb, attr); break;
    case 8: disp_number3(*parameter_phase_limit_ptr, row, 0xb, attr); break;
    case 9: disp_signed_angle(*parameter_master_slave_offset_ptr, row, 0xb, attr); break;
    case 10:
      if (*parameter_control_method_ptr == 0) disp_string((int)0x7998, row, 0xb, attr);
      else if (*parameter_control_method_ptr == 1) disp_string((int)0x79a0, row, 0xb, attr);
      else disp_string((int)0x79a8, row, 0xb, attr);
      break;
    case 11:
      if (*parameter_start_mode_ptr == 0) disp_string((int)0x79b4, row, 0xb, attr);
      else disp_string((int)0x79bc, row, 0xb, attr);
      break;
    case 12:
      if (*parameter_emergency_stop_ptr == 0) disp_string((int)0x6018, row, 0xb, attr);
      else if (*parameter_emergency_stop_ptr == 1) disp_string((int)0x6020, row, 0xb, attr);
      else disp_string((int)0x6028, row, 0xb, attr);
      break;
    case 13:
      if (*parameter_feedback_mode_ptr == 0) disp_string((int)0x6038, row, 0xb, attr);
      else disp_string((int)0x6040, row, 0xb, attr);
      break;
    case 14:
      if (*parameter_input_mode_ptr == 0) disp_string((int)0x6048, row, 0xb, attr);
      else disp_string((int)0x6050, row, 0xb, attr);
      break;
    case 15: disp_number3(*parameter_start_phase_ptr, row, 0xb, attr); break;
  }
}

/* =============================================================================
 * case3 整页值渲染 (0x7458-0x7C1A)：ui_statistics_timeout_ticks_ptr 计数到 0xFB 后重绘当前页全部 4 项值，
 * 当前项高亮(attr=1)、其余正常(attr=0)。原厂导航/编辑后仅重画标签会把值列清掉，
 * 靠此公共尾部整页重绘恢复——正是"未选中行值被清除"的修复点。
 * ========================================================================== */
static void sm3_draw_page(uint32_t item_index)
{
  uint32_t page = item_index >> 2;
  uint32_t item_offset;
  for (item_offset = 0; item_offset < 4; item_offset++)
    sm3_draw_item((page << 2) + item_offset, item_offset, ((page << 2) + item_offset) == item_index ? 1 : 0);
}

/* =============================================================================
 * case4 单项目值渲染 (0x8252-0x85BE)：按项号 item_index 显示保护参数值+单位到 (row,0xb)。
 * attr=1 高亮当前项；0 普通。值串/单位地址全部 bin 校验（§13），禁止臆造。
 * ========================================================================== */
static void sm4_draw_value(uint32_t item_index, uint32_t row, uint32_t attr)
{
  switch (item_index) {
    case 0:
      if (*parameter_overvoltage_limit_ptr) { disp_uint4(*parameter_overvoltage_limit_ptr,row,0xb,attr); disp_string(0x7974,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 1: disp_uint4(*parameter_overvoltage_time_ptr,row,0xb,attr); break;
    case 2:
      if (*parameter_undervoltage_limit_ptr) { disp_uint4(*parameter_undervoltage_limit_ptr,row,0xb,attr); disp_string(0x7974,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 3: disp_uint4(*parameter_undervoltage_time_ptr,row,0xb,attr); break;
    case 4:
      if (*parameter_if_overload_limit_ptr) { disp_uint4(*parameter_if_overload_limit_ptr,row,0xb,attr); disp_string(0x7980,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 5: disp_uint4(*parameter_if_overload_time_ptr,row,0xb,attr); break;
    case 6:
      if (*parameter_ct_overload_limit_ptr) { disp_uint4(*parameter_ct_overload_limit_ptr,row,0xb,attr); disp_string(0x7980,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 7: disp_uint4(*parameter_ct_overload_time_ptr,row,0xb,attr); break;
    case 8:
      if (*parameter_phase_loss_enable_ptr) disp_string(0x6a94,row,0xb,attr);
      else disp_string(0x6038,row,0xb,attr);
      break;
    case 9:
      if (*parameter_phase_balance_ptr >= 0xa) { disp_uint4(*parameter_phase_balance_ptr,row,0xb,attr); disp_string(0x86e0,row,0xf,0); }
      else disp_string(0x6038,row,0xb,attr);
      break;
  }
}

/* 绘制当前项所在整页（10 项 = 页0:0-3, 页1:4-7, 页2:8-9），高亮当前项 */
static void sm4_draw_page(uint32_t item_index)
{
  uint32_t page = item_index >> 2;
  uint32_t start = page << 2;
  uint32_t item_count = (page < 2) ? 4 : 2;
  uint32_t item_offset;
  for (item_offset = 0; item_offset < item_count; item_offset++) {
    uint32_t item = start + item_offset;
    sm4_draw_value(item, item_offset, (item == item_index) ? 1 : 0);
  }
  /* 页0/1 第 4 行之后若仍有空行则留空对齐（页2 padding 已在 item_count=2 覆盖） */
  if (page == 2) { /* rows 2,3 由标题符串 0x5ba4 已画空格，无需再清 */ }
}

/* case5 通讯：单行值渲染（item_index=0..3 本机地址/波特率/校验位/通讯校验，row=item_index，attr=0/1 高亮） */
static void sm5_draw_value(uint32_t item_index, uint32_t attr)
{
  switch (item_index) {
    case 0: disp_uint5(*communication_address_ptr, 0, 0xb, attr); break;
    case 1: disp_number((int)baud_rate_runtime_table_ptr[*baud_rate_index_ptr], 1, 0xa, attr); break;
    case 2:
      if (*communication_parity_ptr == 0) disp_string(0x6a78, 2, 0xa, attr);
      else if (*communication_parity_ptr == 1) disp_string(0x6a80, 2, 0xa, attr);
      else if (*communication_parity_ptr == 2) disp_string(0x6a88, 2, 0xa, attr);
      else disp_string(0x8b2c, 2, 0xa, attr);   /* '1 ST0P' 校验名 */
      break;
    case 3:
      if (*communication_check_ptr) disp_string(0x6a94, 3, 0xb, attr);
      else disp_string(0x6038, 3, 0xb, attr);
      break;
  }
}

/* case5 通讯：整页重绘（4 项单页，高亮当前项） */
static void sm5_draw_page(uint32_t item_index)
{
  uint32_t item_offset;
  for (item_offset = 0; item_offset < 4; item_offset++) sm5_draw_value(item_offset, (item_offset == item_index) ? 1 : 0);
}

/* case6 密码错/对的延时循环（0x8C5A-0x8C8E 段）：
 * 外层 watchdog_delay_outer_ticks_ptr 计数到 0x2710=10000，内层 watchdog_delay_inner_ticks_ptr 计数到 0x3E8=1000，
 * 每个外层进位喂一次狗（wd_feed@0x238）。 */
static void sm6_delay_loop(void)
{
  *watchdog_delay_outer_ticks_ptr = 0;
  for (;;) {
    *watchdog_delay_inner_ticks_ptr = 0;
    do { (*watchdog_delay_inner_ticks_ptr)++; } while (*watchdog_delay_inner_ticks_ptr < 0x3e8);
    wd_feed();
    (*watchdog_delay_outer_ticks_ptr)++;
    if (*watchdog_delay_outer_ticks_ptr >= 0x2710) break;
  }
}

/* =============================================================================
 * state_machine(0x458C)
 * 流程：entry 公共逻辑（state_entry_ticks_ptr/去抖/故障码/启停/统计）→ ui_screen_id_ptr 分发到各 case。
 * 各 case 以顺序 if 级联实现；case 内部按 key_code 分发。
 * ========================================================================== */

static void state_machine_update_entry(KeyCode key_code)
{
/* ================= entry 公共逻辑 (0x458C-0x4B16) ================= */
  (*state_entry_ticks_ptr)++;
  if (key_code != KEY_NONE) { *state_entry_ticks_ptr = 0; lcd_ctrl_line(1); }
  if (*state_entry_ticks_ptr > 0x1388) { *state_entry_ticks_ptr = 0; lcd_ctrl_line(0); }

  if (debounce_p09() == 1) {                    /* P0.9 高电平：累计/影子值→EEPROM */
    if (*runtime_total_hour_ptr != *eeprom_shadow_runtime_hours) {
      *eeprom_shadow_runtime_hours = *runtime_total_hour_ptr;
      i2c_write_reg((*runtime_total_hour_ptr >> 8) & 0xff, 0x97);
      i2c_write_reg(*runtime_total_hour_ptr & 0xff, 0x98);
    }
    if (*runtime_total_minute_ptr != *eeprom_shadow_runtime_minutes) {
      *eeprom_shadow_runtime_minutes = *runtime_total_minute_ptr;
      i2c_write_reg((*runtime_total_minute_ptr >> 8) & 0xff, 0x99);
      i2c_write_reg(*runtime_total_minute_ptr & 0xff, 0x9a);
    }
    if (*runtime_persist_counter_ptr != *runtime_persisted_value_ptr) {
      *runtime_persisted_value_ptr = *runtime_persist_counter_ptr;
      i2c_write_reg((*runtime_persisted_value_ptr >> 8) & 0xff, 0x9b);
      i2c_write_reg(*runtime_persisted_value_ptr & 0xff, 0x9c);
    }
    if (*manual_reference_value_ptr != *eeprom_shadow_edit_value) {
      *eeprom_shadow_edit_value = *manual_reference_value_ptr;
      i2c_write_reg((*manual_reference_value_ptr >> 8) & 0xff, 0x1d);
      i2c_write_reg(*manual_reference_value_ptr & 0xff, 0x1e);
    }
  }

  if (*output_fault_flags_ptr != 0) {                            /* 故障：置事件码 + 关输出 + 停机 */
    *ui_event_code_ptr = 0;
    if (*output_fault_flags_ptr & 0x4) *ui_event_code_ptr = 1;
    if (*output_fault_flags_ptr & 0x2) *ui_event_code_ptr = 1;
    if (*output_fault_flags_ptr & 0x1) *ui_event_code_ptr = 1;
    if (*output_fault_flags_ptr & 0x8) *ui_event_code_ptr = 2;
    if (*output_fault_flags_ptr & 0x200) *ui_event_code_ptr = 3;
    if (*output_fault_flags_ptr & 0x40) *ui_event_code_ptr = 4;
    if (*output_fault_flags_ptr & 0x400) *ui_event_code_ptr = 5;
    if (*output_fault_flags_ptr & 0x10) *ui_event_code_ptr = 6;
    if (*output_fault_flags_ptr & 0x20) *ui_event_code_ptr = 7;
    if (*output_fault_flags_ptr & 0x100) *ui_event_code_ptr = 8;
    if (*output_fault_flags_ptr & 0x80) *ui_event_code_ptr = 9;
    if (*output_fault_flags_ptr & 0x4000) *ui_event_code_ptr = 0xa;
    if (*output_fault_flags_ptr & 0x8000) *ui_event_code_ptr = 0xb;
    if (*output_fault_flags_ptr & 0x800) *ui_event_code_ptr = 0xc;
    if (*output_fault_flags_ptr & 0x2000) *ui_event_code_ptr = 0xd;
    if (*output_fault_flags_ptr & 0x1000) *ui_event_code_ptr = 0xe;
    fio0_pin22_ctrl(0);
    fio1_pin22_ctrl(0);
    out_relay_p021(1);
    fio1_pin23_ctrl(1);
    *run_stop_state_ptr = 0;
    *stop_pending_ptr = 1;
    *stop_request_ptr = 0;
    *operation_configuration_ptr = 0;
    *output_setpoint_ptr = 0;
    *output_ramp_counter_ptr = 0;
    *output_ramp_value_ptr = 0;
    *output_ramp_target_ptr = 0;
    *output_run_state_ptr = 0;
    gpio_outputs_set();                          /* 0xE79A */
  } else {
    out_relay_p021(0);
    fio1_pin23_ctrl(0);
    *ui_event_code_ptr = 0;
  }

  if (*operation_configuration_ptr != 0) {                              /* 运行统计 */
    fio0_pin22_ctrl(1);
    fio1_pin22_ctrl(1);
    (*runtime_tick_ptr)++;
    if (*runtime_tick_ptr > 0x7530) {
      *runtime_tick_ptr = 0;
      (*runtime_current_minute_ptr)++;
      (*runtime_total_minute_ptr)++;
      if (*runtime_current_minute_ptr >= 0x3c) { *runtime_current_minute_ptr = 0; (*runtime_current_hour_ptr)++; }
      if (*runtime_total_minute_ptr >= 0x3c) {
        *runtime_total_minute_ptr = 0;
        (*runtime_total_hour_ptr)++;
        (*runtime_persist_counter_ptr)++;
      }
      if (*runtime_persist_counter_ptr >= 0x140) *runtime_persist_counter_ptr = 0;
    }
    if (*runtime_persist_counter_ptr == 0x78 && *parameter_sync_pending_ptr == 0) {
      *parameter_sync_pending_ptr = 1; *parameter_sync_retry_count_ptr = 0; param_sync_live_to_eeprom();
    }
    if (*runtime_persist_counter_ptr == 0x12c && *parameter_sync_pending_ptr == 2) {
      *parameter_sync_pending_ptr = 0; *parameter_sync_retry_count_ptr = 1; param_sync_live_to_eeprom();
    }
  }

  if (*parameter_sync_state_ptr != 1) { out_relay_p020(1); out_relay_p021(1); fio0_pin22_ctrl(1); }

  *run_stop_debounce_result_ptr = debounce_p116();
  if (*output_fault_flags_ptr == 0 && *run_stop_debounce_result_ptr == 2) *output_fault_flags_ptr |= 0x4000;

  (*phase_loss_check_interval_ptr)++;
  if (*phase_loss_check_interval_ptr > 0x64) {
  *phase_loss_check_interval_ptr = 0;
  if (*output_eint1_flag_ptr == 0 && *parameter_phase_loss_enable_ptr > 0) {
    (*phase_loss_counter_a_ptr)++;
    if (*phase_loss_counter_a_ptr == 5) { *phase_loss_counter_a_ptr = 0; *output_fault_flags_ptr |= 0x1; }
  } else { *phase_loss_counter_a_ptr = 0; *output_fault_flags_ptr &= ~0x1; }
  if (*output_eint2_flag_ptr == 0 && *parameter_phase_loss_enable_ptr > 0) {
    (*phase_loss_counter_b_ptr)++;
    if (*phase_loss_counter_b_ptr == 5) { *phase_loss_counter_b_ptr = 0; *output_fault_flags_ptr |= 0x2; }
  } else { *phase_loss_counter_b_ptr = 0; *output_fault_flags_ptr &= ~0x2; }
  if (*output_eint3_flag_ptr == 0 && *parameter_phase_loss_enable_ptr > 0) {
    (*phase_loss_counter_c_ptr)++;
    if (*phase_loss_counter_c_ptr == 5) { *phase_loss_counter_c_ptr = 0; *output_fault_flags_ptr |= 0x4; }
  } else { *phase_loss_counter_c_ptr = 0; *output_fault_flags_ptr &= ~0x4; }
  *output_eint1_flag_ptr = 0;
  *output_eint2_flag_ptr = 0;
  *output_eint3_flag_ptr = 0;
  }
}

static void state_machine_dispatch_pages(KeyCode key_code);

void state_machine(KeyCode key_code)
{
  state_machine_update_entry(key_code);
  state_machine_dispatch_pages(key_code);
}

static void state_machine_page_main(KeyCode key_code)
{
    /* ---------- case1 运行状态屏 (0x4B16-0x541C) ---------- */
    if (key_code == KEY_CLEAR_STATISTICS && *operation_configuration_ptr == 0) {
      *ui_screen_id_ptr = UI_SCREEN_RUNTIME_CLEAR; *ui_item_index_ptr = 0; *ui_idle_timeout_ticks_ptr = 0;
      disp_clear();
      disp_string((int)0x4d58, 0, 0, 0);
      disp_string((int)0x4d6c, 1, 0, 0);
      disp_string((int)0x4d80, 2, 0, 0);
      disp_string((int)0x4d6c, 3, 0, 0);
      disp_uint5(*runtime_current_hour_ptr, 1, 3, 0);
      disp_uint2(*runtime_current_minute_ptr, 1, 0xa, 0);
      disp_uint5(*runtime_total_hour_ptr, 3, 3, 0);
      disp_uint2(*runtime_total_minute_ptr, 3, 0xa, 0);
      return;
    }
    if (key_code == KEY_CONFIRM && *operation_configuration_ptr == 0) {
      *ui_screen_id_ptr = UI_SCREEN_CALIBRATION_MENU;
      *ui_item_index_ptr = 0; *ui_idle_timeout_ticks_ptr = 0;
      disp_clear();
      disp_string((int)0x4d9c, 1, 0, 0);
      disp_string((int)0x4dac, 3, 7, 0);
      *ui_calibration_timeout_ticks_ptr = 0x3c; *ui_idle_refresh_ticks_ptr = 0; return;
    }
    if (key_code == KEY_INITIAL_PARAMETER_PASSWORD) {
      *ui_screen_id_ptr = UI_SCREEN_CALIBRATION_ACTIVE;
      *ui_item_index_ptr = 0; *ui_idle_timeout_ticks_ptr = 0;
      disp_clear();
      disp_string((int)0x4db4, 0, 0, 0);
      disp_string((int)0x4dc8, 1, 0, 0);
      disp_string((int)0x4dac, 3, 7, 0); return;
    }
    if (key_code == KEY_BACK && *operation_configuration_ptr == 0 && *output_fault_flags_ptr != 0) {
      *ui_screen_id_ptr = UI_SCREEN_STATUS_MONITOR; *ui_statistics_timeout_ticks_ptr = 0x1f4;
      *ui_item_index_ptr = 0; *ui_idle_timeout_ticks_ptr = 0;
      disp_clear(); return;
    }
    (*ui_idle_refresh_ticks_ptr)++;
    if (*ui_idle_refresh_ticks_ptr >= 0x15e) {
      *ui_idle_refresh_ticks_ptr = 0;
      if (*parameter_control_method_ptr == 0) disp_fixed_1dec(*frequency_reference_ptr, 0, 9, 0);
      else if (*parameter_control_method_ptr == 1) disp_fixed_1dec(*output_reference_value_ptr, 0, 9, 0);
      else disp_fixed_1dec(*manual_reference_value_ptr, 0, 9, 0);
      disp_uint4(*adc_voltage_output_ptr, 1, 9, 0);
      disp_uint4(*adc_field_output_ptr, 2, 9, 0);
      if (*output_fault_flags_ptr != 0) { *ui_system_status_ptr = 0; disp_string((int)0x47dc, 3, 0xa, 0); }
      else if (*operation_configuration_ptr == 0 && *ui_system_status_ptr != 1) { *ui_system_status_ptr = 1; disp_string((int)0x47e8, 3, 0xa, 0); }
      if (*parameter_control_mode_ptr == 0 && *ui_control_display_mode_ptr != 1) {
        *ui_control_display_mode_ptr = 1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);  /* 0x4E44：r0=1 → P1.20 置位高；反编译曾误作 0，已还原 */
        disp_string((int)0x47fc, 3, 0, 0);
      } else if (*parameter_control_mode_ptr == 1 && *ui_control_display_mode_ptr != 2) {
        *ui_control_display_mode_ptr = 2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
        disp_string((int)0x4804, 3, 0, 0);
      } else if (*parameter_control_mode_ptr == 2 && *ui_control_display_mode_ptr != 3) {
        *ui_control_display_mode_ptr = 3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0);
        disp_string((int)0x480c, 3, 0, 0);
      }
    }

    /* 0x4CF2 直接跳到 0x4EBC：显示刷新未到 350 tick 时仍须执行输入扫描。
     * 这段不能放在 ui_idle_refresh_ticks_ptr>=0x15e 内，否则复位/急停/operation_configuration_ptr-STOP 只会每 350 次扫描一次。 */
    *emergency_stop_debounce_ptr = debounce_p117();
      if (*output_fault_flags_ptr != 0) {
        if (*emergency_stop_debounce_ptr == 2 && *parameter_auxiliary_mode_ptr == 0) {  /* 复位流程 */
          *output_fault_flags_ptr = 0; *operation_configuration_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0;
          disp_string((int)0x522c, 3, 0xa, 0);   /* BIN 0x4EF6：复位(行3,列0xa) */
          /* 双层延时：watchdog_delay_inner_ticks_ptr 内层 0→0x7d0（do-while），watchdog_delay_outer_ticks_ptr 外层到 0xbb8（0x4EFA-0x4F36） */
          *watchdog_delay_outer_ticks_ptr = 0;
          for (;;) { *watchdog_delay_inner_ticks_ptr = 0; do { (*watchdog_delay_inner_ticks_ptr)++; } while (*watchdog_delay_inner_ticks_ptr < 0x7d0); wd_feed(); (*watchdog_delay_outer_ticks_ptr)++; if (*watchdog_delay_outer_ticks_ptr >= 0xbb8) break; }
          disp_string((int)0x523c, 3, 0xa, 0);   /* BIN 0x4F40：重启(行3,列0xa) */
          *watchdog_delay_outer_ticks_ptr = 0;
          for (;;) { *watchdog_delay_inner_ticks_ptr = 0; do { (*watchdog_delay_inner_ticks_ptr)++; } while (*watchdog_delay_inner_ticks_ptr < 0x7d0); wd_feed(); (*watchdog_delay_outer_ticks_ptr)++; if (*watchdog_delay_outer_ticks_ptr >= 0xbb8) break; }
          while (1) {}
        }
      }
        if (*parameter_auxiliary_mode_ptr == 1) {
          if (*emergency_stop_debounce_ptr != 2 && *ui_control_display_mode_ptr != 1) {
            *ui_secondary_display_mode_ptr = 1; *parameter_control_mode_ptr = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
            disp_string((int)0x47fc, 3, 0, 0);
          }
          if (*emergency_stop_debounce_ptr == 2 && *ui_control_display_mode_ptr != 2) {
            *ui_secondary_display_mode_ptr = 2; *parameter_control_mode_ptr = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
            disp_string((int)0x4804, 3, 0, 0);
          }
        }
        if (*parameter_auxiliary_mode_ptr == 2) {
          if (*emergency_stop_debounce_ptr != 2) *reset_output_state_ptr = 0; else *reset_output_state_ptr = 1;
        }
        *emergency_stop_debounce_ptr = debounce_p06();
        if (*output_fault_flags_ptr == 0 && *emergency_stop_debounce_ptr == 2 && *parameter_emergency_stop_ptr == 0) {
          *run_request_ptr = 1; *operation_configuration_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0;
          if (*status_message_shown_ptr == 0) { disp_string((int)0x47e8, 3, 0xa, 0); *status_message_shown_ptr = 1; }
          return;
        }
        /* parameter_emergency_stop_ptr(0x10001657)==1/2：恒压切换/复位设置（逻辑同 parameter_auxiliary_mode_ptr==1/2） */
        if (*parameter_emergency_stop_ptr == 1) {
          if (*emergency_stop_debounce_ptr != 2 && *ui_control_display_mode_ptr != 1) {
            *ui_secondary_display_mode_ptr = 1; *parameter_control_mode_ptr = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0);
            disp_string((int)0x47fc, 3, 0, 0);
          }
          if (*emergency_stop_debounce_ptr == 2 && *ui_control_display_mode_ptr != 2) {
            *ui_secondary_display_mode_ptr = 2; *parameter_control_mode_ptr = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1);
            disp_string((int)0x4804, 3, 0, 0);
          }
        }
        if (*parameter_emergency_stop_ptr == 2) {
          if (*emergency_stop_debounce_ptr != 2) *reset_output_state_ptr = 0; else *reset_output_state_ptr = 1;
        }
        if (*parameter_auxiliary_mode_ptr != 2 && *parameter_emergency_stop_ptr != 2) *reset_output_state_ptr = 0;
        *ui_scan_stop_flag_ptr = scan_run_stop();
        if (*output_fault_flags_ptr == 0 && *stop_request_ptr == 0 && *run_stop_state_ptr == 1 && *parameter_control_method_ptr == 0) {
          *run_stop_state_ptr = 1; *stop_request_ptr = 1; *stop_pending_ptr = 0; *run_request_ptr = 0;
          *operation_configuration_ptr = 1; *status_message_shown_ptr = 0; *runtime_tick_ptr = 0; *runtime_current_minute_ptr = 0; *runtime_current_hour_ptr = 0;
          disp_string((int)0x47f0, 3, 0xa, 0);
        }
        if (*output_fault_flags_ptr == 0 && *stop_pending_ptr == 0 && *run_stop_state_ptr == 0 && *parameter_control_method_ptr == 0) {
          *stop_pending_ptr = 1; *stop_request_ptr = 0; *operation_configuration_ptr = 0;
          disp_string((int)0x47e8, 3, 0xa, 0);
        }
        if (*operation_configuration_ptr == 0 && *parameter_control_method_ptr != 0) *run_stop_state_ptr = 0;
        if (*output_fault_flags_ptr == 0 && *stop_request_ptr == 0 && (key_code == KEY_START || *ui_scan_stop_flag_ptr == 7)) {
          if (*parameter_start_mode_ptr == 0) {
            *run_stop_state_ptr = 1; *stop_request_ptr = 1; *stop_pending_ptr = 0; *run_request_ptr = 0;
            *operation_configuration_ptr = 1; *status_message_shown_ptr = 0; *runtime_tick_ptr = 0; *runtime_current_minute_ptr = 0; *runtime_current_hour_ptr = 0;
            disp_string((int)0x47f0, 3, 0xa, 0);
          } else if (*ui_scan_stop_flag_ptr == 7 && *parameter_start_mode_ptr == 1 && *parameter_control_method_ptr != 0) {
            *run_stop_state_ptr = 1; *stop_request_ptr = 1; *stop_pending_ptr = 0; *run_request_ptr = 0;
            *operation_configuration_ptr = 1; *status_message_shown_ptr = 0; *runtime_tick_ptr = 0; *runtime_current_minute_ptr = 0; *runtime_current_hour_ptr = 0;
            disp_string((int)0x47f0, 3, 0xa, 0);
          }
        }
        if (*output_fault_flags_ptr == 0 && *stop_pending_ptr == 0 && (key_code == KEY_STOP || *ui_scan_stop_flag_ptr == 8)) {
          if (*parameter_start_mode_ptr == 0) {
            *run_stop_state_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0; *operation_configuration_ptr = 0;
            disp_string((int)0x47e8, 3, 0xa, 0);
          } else if (*ui_scan_stop_flag_ptr == 8 && *parameter_start_mode_ptr == 1 && *parameter_control_method_ptr != 0) {
            *run_stop_state_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0; *operation_configuration_ptr = 0;
            disp_string((int)0x47e8, 3, 0xa, 0);
          }
        }
      /* 0x52FA：parameter_control_method_ptr 三分支不受 output_fault_flags_ptr 门控——原厂 0x52A2 在 output_fault_flags_ptr!=0 时
       * cbnz 跳到 0x52FA 仍执行三分支，仅 operation_configuration_ptr/STOP 逻辑(0x52A8-0x52F6)被 output_fault_flags_ptr 跳过。
       * 若放回 output_fault_flags_ptr==0 分支内，output_fault_flags_ptr=7(缺相)时首页第一行上下键将永远不执行。 */
      if (*parameter_control_method_ptr == 0) {
        *output_reference_value_ptr = *frequency_reference_ptr;
        if (*parameter_control_mode_ptr == 0) *target_amplitude_ptr = (*frequency_reference_ptr * *parameter_voltage_range_ptr) / 1000;
        else *target_amplitude_ptr = (*frequency_reference_ptr * *parameter_current_range_ptr) / 1000;
        *output_reference_average_ptr = *target_amplitude_ptr; *output_secondary_reference_ptr = *target_amplitude_ptr;
      } else if (*parameter_control_method_ptr == 1) {
        *output_reference_average_ptr = *output_secondary_reference_ptr;
      } else {
        if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {
          (*manual_reference_value_ptr)++; if (*manual_reference_value_ptr > 0x3e8) *manual_reference_value_ptr = 0x3e8;
          if (*manual_reference_value_ptr < 0xa) *manual_reference_value_ptr = 0xa;
          disp_fixed_1dec(*manual_reference_value_ptr, 0, 9, 0);
        }
        if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
          if (*manual_reference_value_ptr > 0xa) (*manual_reference_value_ptr)--;
          else { *manual_reference_value_ptr = 1; (*manual_reference_value_ptr)--; }
          disp_fixed_1dec(*manual_reference_value_ptr, 0, 9, 0);
        }
        *output_reference_value_ptr = *manual_reference_value_ptr;
        if (*parameter_control_mode_ptr == 0) *manual_scaled_output_ptr = (*manual_reference_value_ptr * *parameter_voltage_range_ptr) / 1000;
        else *manual_scaled_output_ptr = (*manual_reference_value_ptr * *parameter_current_range_ptr) / 1000;
        *output_reference_average_ptr = *manual_scaled_output_ptr;   /* 0x541A→0x541E：仅 parameter_control_method_ptr==2 覆盖；0/1 走 0x4C1C→0x541C 跳过 */
        *output_secondary_reference_ptr = *manual_scaled_output_ptr;
      }
    return;

}

static void state_machine_page_calibration_menu(KeyCode key_code)
{
  uint32_t delay_iteration;
    /* ---------- caseA 参数密码屏 (0x541C-0x5572) ---------- */
    if (key_code == KEY_CONFIRM) {
      *ui_item_index_ptr = 0;
      while (*ui_item_index_ptr < 6) {
        if (password_input_buffer_ptr[*ui_item_index_ptr] != password_part_a_ptr[*ui_item_index_ptr]) {
          disp_clear();
          disp_string((int)0x56dc, 1, 4, 0);
          /* 密码错延时：delay_iteration 计 0x3e8 次，循环体喂狗(wd_feed)并累加 watchdog_delay_outer_ticks_ptr；外层至 0x2710 */
          *watchdog_delay_inner_ticks_ptr = 0;
          for (delay_iteration = 0; delay_iteration < 0x3e8; delay_iteration++) { wd_feed(); (*watchdog_delay_outer_ticks_ptr)++; }
          *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return;
        }
        password_input_buffer_ptr[*ui_item_index_ptr] = 0; (*ui_item_index_ptr)++;
      }
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS; *ui_item_index_ptr = 0; disp_screen_static(); return;
    }
    if (key_code == KEY_BACK) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    if (key_code != KEY_NONE) {
      if (*ui_item_index_ptr < 6) { password_input_buffer_ptr[*ui_item_index_ptr] = key_code; disp_render_char8('*', 1, *ui_item_index_ptr + 7, 0); (*ui_item_index_ptr)++; }
      return;
    }
    (*ui_idle_refresh_ticks_ptr)++;
    if (*ui_idle_refresh_ticks_ptr >= 0x1f4) {
      *ui_idle_refresh_ticks_ptr = 0; (*ui_calibration_timeout_ticks_ptr)--;
      disp_number3(*ui_calibration_timeout_ticks_ptr, 3, 6, 0);
      if (*ui_calibration_timeout_ticks_ptr == 0) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    }
    return;

}

static void state_machine_page_calibration_active(KeyCode key_code)
{
  uint32_t delay_iteration;
    /* ---------- case62 初始密码屏 (0x5572-0x5748) ---------- */
    if (key_code == KEY_CONFIRM) {
      *ui_item_index_ptr = 0;
      while (*ui_item_index_ptr < 6) {
        if (password_input_buffer_ptr[*ui_item_index_ptr] != password_part_c_ptr[*ui_item_index_ptr]) {
          disp_clear();
          disp_string((int)0x56dc, 1, 4, 0);
          /* 密码错延时（同 caseA）：delay_iteration 计 0x3e8 次，喂狗 + watchdog_delay_outer_ticks_ptr++ */
          *watchdog_delay_inner_ticks_ptr = 0;
          for (delay_iteration = 0; delay_iteration < 0x3e8; delay_iteration++) { wd_feed(); (*watchdog_delay_outer_ticks_ptr)++; }
          *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return;
        }
        password_input_buffer_ptr[*ui_item_index_ptr] = 0; (*ui_item_index_ptr)++;
      }
      *ui_screen_id_ptr = UI_SCREEN_CALIBRATION_RESULT; *ui_item_index_ptr = 0; disp_clear(); disp_screen_calib(); return;
    }
    if (key_code == KEY_BACK) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    if (key_code != KEY_NONE) {
      if (*ui_item_index_ptr < 6) { password_input_buffer_ptr[*ui_item_index_ptr] = key_code; disp_render_char8('*', 1, *ui_item_index_ptr + 7, 0); (*ui_item_index_ptr)++; }
      return;
    }
    (*ui_idle_refresh_ticks_ptr)++;
    if (*ui_idle_refresh_ticks_ptr >= 0x1f4) {
      *ui_idle_refresh_ticks_ptr = 0; (*ui_calibration_timeout_ticks_ptr)--;
      disp_number3(*ui_calibration_timeout_ticks_ptr, 3, 6, 0);
      if (*ui_calibration_timeout_ticks_ptr == 0) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    }
    return;

}

static void state_machine_page_calibration_result(KeyCode key_code)
{
    /* ---------- case63 初始参数 (0x5748-0x6134)，ui_item_index_ptr=项0-10，ui_view_mode_ptr=0导航/1编辑 ---------- */
    if (key_code == KEY_CONFIRM) {
      *ui_idle_timeout_ticks_ptr = 0; (*ui_view_mode_ptr)++; if (*ui_view_mode_ptr > 1) *ui_view_mode_ptr = 0;
      if (*ui_view_mode_ptr == 0) *ui_statistics_timeout_ticks_ptr = 0xfa; else *ui_statistics_timeout_ticks_ptr = 0x1f4;
    }
    if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0; param_sync_live_to_eeprom(); *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return;
    }
    if (key_code == KEY_DOWN || key_code == KEY_UP) {
      if (*ui_view_mode_ptr == 0) {
        *ui_idle_timeout_ticks_ptr = 0;
        if (key_code == KEY_UP) { (*ui_item_index_ptr)++; if (*ui_item_index_ptr > 0xa) *ui_item_index_ptr = 0xa; }
        if (key_code == KEY_DOWN) { if (*ui_item_index_ptr > 0) (*ui_item_index_ptr)--; }
        if (*ui_item_index_ptr < 4) {
          disp_string((int)0x4854, 0, 0, 0); disp_string((int)0x4868, 1, 0, 0);
          disp_string((int)0x487c, 2, 0, 0); disp_string((int)0x4890, 3, 0, 0);
        }
        if (*ui_item_index_ptr >= 4 && *ui_item_index_ptr < 8) {
          disp_string((int)0x5b18, 0, 0, 0); disp_string((int)0x5b2c, 1, 0, 0);
          disp_string((int)0x5b40, 2, 0, 0); disp_string((int)0x5b54, 3, 0, 0);
        }
        if (*ui_item_index_ptr >= 8 && *ui_item_index_ptr < 0xc) {
          disp_string((int)0x5b68, 0, 0, 0); disp_string((int)0x5b7c, 1, 0, 0);
          disp_string((int)0x5b90, 2, 0, 0); disp_string((int)0x5ba4, 3, 0, 0);
        }
        *ui_statistics_timeout_ticks_ptr = 0xfa;
      } else {
        if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {
          *ui_idle_timeout_ticks_ptr = 0;
          if (*ui_item_index_ptr == 0) { (*parameter_current_calibration_a_ptr)++; if (*parameter_current_calibration_a_ptr > 0x1194) *parameter_current_calibration_a_ptr = 0x1194; }
          if (*ui_item_index_ptr == 1) { (*parameter_current_calibration_b_ptr)++; if (*parameter_current_calibration_b_ptr > 0x1194) *parameter_current_calibration_b_ptr = 0x1194; }
          if (*ui_item_index_ptr == 2) { (*parameter_current_calibration_c_ptr)++; if (*parameter_current_calibration_c_ptr > 0x1194) *parameter_current_calibration_c_ptr = 0x1194; }
          if (*ui_item_index_ptr == 3) { (*parameter_field_calibration_ptr)++; if (*parameter_field_calibration_ptr > 0x1194) *parameter_field_calibration_ptr = 0x1194; }
          if (*ui_item_index_ptr == 4) { (*parameter_voltage_calibration_ptr)++; if (*parameter_voltage_calibration_ptr > 0x1194) *parameter_voltage_calibration_ptr = 0x1194; }
          if (*ui_item_index_ptr == 5) { (*parameter_emergency_stop_ptr)++; if (*parameter_emergency_stop_ptr > 2) *parameter_emergency_stop_ptr = 2; }
          if (*ui_item_index_ptr == 6) { (*parameter_auxiliary_mode_ptr)++; if (*parameter_auxiliary_mode_ptr > 2) *parameter_auxiliary_mode_ptr = 2; }
          if (*ui_item_index_ptr == 7) { (*parameter_feedback_mode_ptr)++; if (*parameter_feedback_mode_ptr > 1) *parameter_feedback_mode_ptr = 1; }
          if (*ui_item_index_ptr == 8) { (*parameter_input_mode_ptr)++; if (*parameter_input_mode_ptr > 1) *parameter_input_mode_ptr = 1; }
          if (*ui_item_index_ptr == 9) { (*parameter_output_phase_ptr)++; if (*parameter_output_phase_ptr > 1) *parameter_output_phase_ptr = 1; }
          if (*ui_item_index_ptr == 0xa) { (*parameter_start_phase_ptr)++; if (*parameter_start_phase_ptr > 0xb4) *parameter_start_phase_ptr = 0xb4; }
        }
        if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
          *ui_idle_timeout_ticks_ptr = 0;
          if (*ui_item_index_ptr == 0) { if (*parameter_current_calibration_a_ptr > 0xdac) (*parameter_current_calibration_a_ptr)--; }
          if (*ui_item_index_ptr == 1) { if (*parameter_current_calibration_b_ptr > 0xdac) (*parameter_current_calibration_b_ptr)--; }
          if (*ui_item_index_ptr == 2) { if (*parameter_current_calibration_c_ptr > 0xdac) (*parameter_current_calibration_c_ptr)--; }
          if (*ui_item_index_ptr == 3) { if (*parameter_field_calibration_ptr > 0xdac) (*parameter_field_calibration_ptr)--; }
          if (*ui_item_index_ptr == 4) { if (*parameter_voltage_calibration_ptr > 0xdac) (*parameter_voltage_calibration_ptr)--; }
          if (*ui_item_index_ptr == 5) { if (*parameter_emergency_stop_ptr > 0) (*parameter_emergency_stop_ptr)--; }
          if (*ui_item_index_ptr == 6) { if (*parameter_auxiliary_mode_ptr > 0) (*parameter_auxiliary_mode_ptr)--; }
          if (*ui_item_index_ptr == 7) { if (*parameter_feedback_mode_ptr > 0) (*parameter_feedback_mode_ptr)--; }
          if (*ui_item_index_ptr == 8) { if (*parameter_input_mode_ptr > 0) (*parameter_input_mode_ptr)--; }
          if (*ui_item_index_ptr == 9) { if (*parameter_output_phase_ptr > 0) (*parameter_output_phase_ptr)--; }
          if (*ui_item_index_ptr == 0xa) { if (*parameter_start_phase_ptr > 0) (*parameter_start_phase_ptr)--; }
        }
        *ui_statistics_timeout_ticks_ptr = 0xfa;
      }
    }
    (*ui_statistics_timeout_ticks_ptr)++;
    if (*ui_statistics_timeout_ticks_ptr == 0xfb) {
      /* 0x5C82-0x5F72：整页重绘，当前项反显（attr=1），其余 attr=0。
       * 行号=项号%4：项0-3→row0-3、项4-7→row0-3、项8-0xa→row0-2。
       * 项0-4=4位数值(disp_uint4，地址 0x10001698/a0/a8/b0/b8)；
       * 项5=parameter_emergency_stop_ptr、项6=parameter_auxiliary_mode_ptr、项7=parameter_feedback_mode_ptr、项8=parameter_input_mode_ptr、
       * 项9=0x1000165b、项0xa=起始相位(8位装入 disp_number3)。 */
      if (*ui_item_index_ptr < 4) {
        disp_uint4(*parameter_current_calibration_a_ptr, 0, 0xb, (*ui_item_index_ptr == 0) ? 1 : 0);
        disp_uint4(*parameter_current_calibration_b_ptr, 1, 0xb, (*ui_item_index_ptr == 1) ? 1 : 0);
        disp_uint4(*parameter_current_calibration_c_ptr, 2, 0xb, (*ui_item_index_ptr == 2) ? 1 : 0);
        disp_uint4(*parameter_field_calibration_ptr, 3, 0xb, (*ui_item_index_ptr == 3) ? 1 : 0);
      } else if (*ui_item_index_ptr < 8) {
        /* item4=输出电压数值 row0 */
        disp_uint4(*parameter_voltage_calibration_ptr, 0, 0xb, (*ui_item_index_ptr == 4) ? 1 : 0);
        /* item5=parameter_emergency_stop_ptr row1：0=急停/1=外控/2=限相 */
        if (*parameter_emergency_stop_ptr == 0) disp_string((int)0x6018, 1, 0xb, (*ui_item_index_ptr == 5) ? 1 : 0);
        else if (*parameter_emergency_stop_ptr == 1) disp_string((int)0x6020, 1, 0xb, (*ui_item_index_ptr == 5) ? 1 : 0);
        else disp_string((int)0x6028, 1, 0xb, (*ui_item_index_ptr == 5) ? 1 : 0);
        /* item6=parameter_auxiliary_mode_ptr row2：0=复位/1=外控/2=限相 */
        if (*parameter_auxiliary_mode_ptr == 0) disp_string((int)0x6030, 2, 0xb, (*ui_item_index_ptr == 6) ? 1 : 0);
        else if (*parameter_auxiliary_mode_ptr == 1) disp_string((int)0x6020, 2, 0xb, (*ui_item_index_ptr == 6) ? 1 : 0);
        else disp_string((int)0x6028, 2, 0xb, (*ui_item_index_ptr == 6) ? 1 : 0);
        /* item7=parameter_feedback_mode_ptr row3：==0→关闭(0x6038)；!=0→检测(0x6040) */
        if (*parameter_feedback_mode_ptr == 0) disp_string((int)0x6038, 3, 0xb, (*ui_item_index_ptr == 7) ? 1 : 0);
        else disp_string((int)0x6040, 3, 0xb, (*ui_item_index_ptr == 7) ? 1 : 0);
      } else if (*ui_item_index_ptr < 0xc) {
        /* item8=parameter_input_mode_ptr row0：0=电压/1=电流 */
        if (*parameter_input_mode_ptr == 0) disp_string((int)0x6048, 0, 0xb, (*ui_item_index_ptr == 8) ? 1 : 0);
        else disp_string((int)0x6050, 0, 0xb, (*ui_item_index_ptr == 8) ? 1 : 0);
        /* item9=控制方式 row1：0=全控/1=半控 */
        if (*parameter_output_phase_ptr == 0) disp_string((int)0x6058, 1, 0xb, (*ui_item_index_ptr == 9) ? 1 : 0);
        else disp_string((int)0x6060, 1, 0xb, (*ui_item_index_ptr == 9) ? 1 : 0);
        /* item0xa=起始相位 row2（8 位装入） */
        disp_number3(*parameter_start_phase_ptr, 2, 0xb, (*ui_item_index_ptr == 0xa) ? 1 : 0);
      }
    }
    /* ui_statistics_timeout_ticks_ptr 超 0x1F4 → 回绕为 0；编辑态按 ui_item_index_ptr 用空格串擦除当前项值列
     * （与 0xFB 整页重绘交替 → 值"反显/消失"闪烁，周期≈501 帧，0x5F74-0x60FE）。
     * 擦除串：0x5B38=4空格(项0-4 数值)、0x6474=5空格(项5-9 文字)、0x647C=3空格(项0xa 3位)。
     * 行号=项号%4。 */
    if (*ui_statistics_timeout_ticks_ptr > 0x1f4) {
      *ui_statistics_timeout_ticks_ptr = 0;
      if (*ui_view_mode_ptr == 0) return;              /* 查看态：本帧提前返回，跳过 ui_idle_timeout_ticks_ptr++ */
      if (*ui_item_index_ptr == 0) disp_string(0x5b38, 0, 0xb, 0);
      if (*ui_item_index_ptr == 1) disp_string(0x5b38, 1, 0xb, 0);
      if (*ui_item_index_ptr == 2) disp_string(0x5b38, 2, 0xb, 0);
      if (*ui_item_index_ptr == 3) disp_string(0x5b38, 3, 0xb, 0);
      if (*ui_item_index_ptr == 4) disp_string(0x5b38, 0, 0xb, 0);
      if (*ui_item_index_ptr == 5) disp_string(0x6474, 1, 0xb, 0);
      if (*ui_item_index_ptr == 6) disp_string(0x6474, 2, 0xb, 0);
      if (*ui_item_index_ptr == 7) disp_string(0x6474, 3, 0xb, 0);
      if (*ui_item_index_ptr == 8) disp_string(0x6474, 0, 0xb, 0);
      if (*ui_item_index_ptr == 9) disp_string(0x6474, 1, 0xb, 0);
      if (*ui_item_index_ptr == 0xa) disp_string(0x647c, 2, 0xb, 0);
    }
    /* 编辑空闲超时回主屏（0x6102-0x6132）：ui_idle_timeout_ticks_ptr 每帧累加（非仅擦除帧）；ui_view_mode_ptr==0 帧已提前返回 */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; param_sync_live_to_eeprom(); *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    return;

}

static void state_machine_page_basic_parameters(KeyCode key_code)
{
    /* ---------- case2 主菜单页1 (0x6134-0x69D6)，ui_item_index_ptr=选项0-8 ---------- */
    if (key_code == KEY_BACK) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    if (key_code == KEY_DOWN || key_code == KEY_UP) {
      *ui_idle_timeout_ticks_ptr = 0;
      if (key_code == KEY_UP) { (*ui_item_index_ptr)++; if (*ui_item_index_ptr > 7) *ui_item_index_ptr = 7; }
      if (key_code == KEY_DOWN) { if (*ui_item_index_ptr > 0) (*ui_item_index_ptr)--; }
      if (*ui_item_index_ptr < 4) {
        disp_string((int)0x6488, 0, 0, 0); disp_string((int)0x649c, 1, 0, 0);
        disp_string((int)0x64b0, 2, 0, 0); disp_string((int)0x64c4, 3, 0, 0);
      }
      if (*ui_item_index_ptr >= 4 && *ui_item_index_ptr < 8) {
        disp_string((int)0x64d8, 0, 0, 0); disp_string((int)0x64ec, 1, 0, 0);
        disp_string((int)0x6500, 2, 0, 0); disp_string((int)0x6514, 3, 0, 0);
      }
      if (*ui_item_index_ptr >= 8 && *ui_item_index_ptr < 0xc) {
        disp_string((int)0x6528, 0, 0, 0); disp_string((int)0x5ba4, 1, 0, 0);
        disp_string((int)0x5ba4, 2, 0, 0); disp_string((int)0x5ba4, 3, 0, 0);
      }
      if (*ui_item_index_ptr == 0) disp_string((int)0x6488, 0, 0, 1);
      if (*ui_item_index_ptr == 1) disp_string((int)0x649c, 1, 0, 1);
      if (*ui_item_index_ptr == 2) disp_string((int)0x64b0, 2, 0, 1);
      if (*ui_item_index_ptr == 3) disp_string((int)0x64c4, 3, 0, 1);
      if (*ui_item_index_ptr == 4) disp_string((int)0x64d8, 0, 0, 1);
      if (*ui_item_index_ptr == 5) disp_string((int)0x64ec, 1, 0, 1);
      if (*ui_item_index_ptr == 6) disp_string((int)0x6500, 2, 0, 1);
      if (*ui_item_index_ptr == 7) disp_string((int)0x6514, 3, 0, 1);
      if (*ui_item_index_ptr == 8) disp_string((int)0x6528, 0, 0, 1);
    }
    if (key_code == KEY_CONFIRM) {
      *ui_idle_timeout_ticks_ptr = 0; *ui_view_mode_ptr = 0;
      if (*ui_item_index_ptr == 0) {
        *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETER_EDIT; *ui_item_index_ptr = 0;
        disp_string((int)0x6540, 0, 0, 0); disp_string((int)0x6554, 1, 0, 0);
        disp_string((int)0x6568, 2, 0, 0); disp_string((int)0x657c, 3, 0, 0);
        if (*parameter_control_mode_ptr == 0) { disp_string((int)0x6594, 0, 0xb, 1); fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); }
        if (*parameter_control_mode_ptr == 1) { disp_string((int)0x659c, 0, 0xb, 1); fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); }
        if (*parameter_control_mode_ptr == 2) { disp_string((int)0x65a4, 0, 0xb, 1); fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); }
        disp_uint4(*parameter_voltage_range_ptr, 1, 0xb, 0);
        disp_uint4(*parameter_current_range_ptr, 2, 0xb, 0);
        disp_uint4(*parameter_transformer_ratio_ptr, 3, 0xb, 0);
        *ui_statistics_timeout_ticks_ptr = 0xfa;
      }
      if (*ui_item_index_ptr == 1) {
        *ui_screen_id_ptr = UI_SCREEN_PROTECTION_PARAMETERS; *ui_item_index_ptr = 0;
        disp_string((int)0x65bc, 0, 0, 0); disp_string((int)0x65d0, 1, 0, 0);
        disp_string((int)0x65e4, 2, 0, 0); disp_string((int)0x65f8, 3, 0, 0);
        disp_uint4(*parameter_overvoltage_limit_ptr, 0, 0xb, 1);
        disp_uint4(*parameter_overvoltage_time_ptr, 1, 0xb, 0);
        disp_uint4(*parameter_undervoltage_limit_ptr, 2, 0xb, 0);
        disp_uint4(*parameter_undervoltage_time_ptr, 3, 0xb, 0);
        *ui_statistics_timeout_ticks_ptr = 0xfa;
      }
      if (*ui_item_index_ptr == 2) {
        *ui_screen_id_ptr = UI_SCREEN_COMMUNICATION_PARAMETERS; *ui_item_index_ptr = 0;
        disp_string((int)0x6a18, 0, 0, 0); disp_string((int)0x6a2c, 1, 0, 0);
        disp_string((int)0x6a40, 2, 0, 0); disp_string((int)0x6a54, 3, 0, 0);
        disp_uint5(*communication_address_ptr, 0, 0xb, 1);
        disp_number(baud_rate_runtime_table_ptr[*baud_rate_index_ptr], 1, 0xa, 0);
        if (*communication_parity_ptr == 0) disp_string((int)0x6a78, 2, 0xa, 0);
        if (*communication_parity_ptr == 1) disp_string((int)0x6a80, 2, 0xa, 0);
        if (*communication_parity_ptr == 2) disp_string((int)0x6a88, 2, 0xa, 0);
        if (*communication_check_ptr == 0) disp_string((int)0x6038, 3, 0xb, 0);
        else disp_string((int)0x6a94, 3, 0xb, 0);
        *ui_statistics_timeout_ticks_ptr = 0xfa;
      }
      if (*ui_item_index_ptr == 3) {
        *ui_screen_id_ptr = UI_SCREEN_RUNTIME_HOURS; *ui_item_index_ptr = 0; *watchdog_delay_outer_ticks_ptr = 0; disp_clear();
        /* BIN 0x6712 是 ldr r0,[0x6aa0] 取字面量值 0x4D9C（"  密码:------"），
         * 不是取 0x6aa0 处的指令字节——0x6AA0 处 4 字节=9C 4D 00 00 会只显示"M"。 */
        disp_string((int)0x4d9c, 1, 0, 0);
      }
      if (*ui_item_index_ptr == 4) {
        *ui_screen_id_ptr = UI_SCREEN_PID_PARAMETERS; *ui_item_index_ptr = 0;
        disp_string((int)0x6aa4, 0, 0, 0); disp_string((int)0x6ab8, 1, 0, 0);
        disp_string((int)0x6acc, 2, 0, 0); disp_string((int)0x6ae0, 3, 0, 0);
        if (*parameter_pid_profile_ptr == 1) { disp_string((int)0x6af8, 0, 0xb, 1); disp_uint2(*parameter_profile1_gain_a_ptr, 1, 0xb, 0); disp_uint2(*parameter_profile1_gain_b_ptr, 2, 0xb, 0); }
        if (*parameter_pid_profile_ptr == 2) { disp_string((int)0x6b08, 0, 0xb, 1); disp_uint2(*parameter_profile2_gain_a_ptr, 1, 0xb, 0); disp_uint2(*parameter_profile1_gain_b_ptr, 2, 0xb, 0); }
        if (*parameter_pid_profile_ptr == 3) { disp_string((int)0x6b14, 0, 0xb, 1); disp_uint2(*parameter_profile3_gain_a_ptr, 1, 0xb, 0); disp_uint2(*parameter_profile3_gain_b_ptr, 2, 0xb, 0); }
        if (*parameter_pid_profile_ptr == 4) { disp_string((int)0x6b24, 0, 0xb, 1); disp_uint2(*parameter_profile4_gain_a_ptr, 1, 0xb, 0); disp_uint2(*parameter_profile4_gain_b_ptr, 2, 0xb, 0); }
        *ui_statistics_timeout_ticks_ptr = 0xfa;
      }
      if (*ui_item_index_ptr == 5) {
        *ui_screen_id_ptr = UI_SCREEN_PHASE_CALIBRATION; *ui_item_index_ptr = 0; disp_clear();
        disp_string((int)0x6b34, 0, 4, 0); disp_string((int)0x6b40, 1, 2, 0);
        disp_string((int)0x6b4c, 2, 2, 0); disp_offset(*frequency_adjustment_ptr, 2, 7, 1);
        disp_string((int)0x6b58, 3, 0, 0);
      }
      if (*ui_item_index_ptr == 6) {
        *ui_screen_id_ptr = 0xb; *ui_item_index_ptr = 0; *watchdog_delay_outer_ticks_ptr = 0; disp_clear();
        disp_string((int)0x4d58, 0, 0, 0); disp_string((int)0x4d6c, 1, 0, 0);
        disp_string((int)0x4d80, 2, 0, 0); disp_string((int)0x4d6c, 3, 0, 0);
        disp_uint5(*runtime_current_hour_ptr, 1, 3, 0); disp_uint2(*runtime_current_minute_ptr, 1, 0xa, 0);
        disp_uint5(*runtime_total_hour_ptr, 3, 3, 0); disp_uint2(*runtime_total_minute_ptr, 3, 0xa, 0);
      }
      if (*ui_item_index_ptr == 7) {
        *ui_screen_id_ptr = UI_SCREEN_VERSION; *ui_item_index_ptr = 0; disp_clear();
        disp_string((int)0x6b78, 0, 0, 0); disp_string((int)0x6b84, 1, 0, 0);
        disp_string((int)0x6b94, 2, 0, 0); disp_string((int)0x6ba4, 3, 0, 0);
      }
      if (*ui_item_index_ptr == 8) {
        *ui_screen_id_ptr = UI_SCREEN_MANUAL_BALANCE; *ui_item_index_ptr = 0; disp_clear();
        disp_string((int)0x6bb8, 0, 4, 0); disp_string((int)0x6b40, 1, 2, 0);
        disp_string((int)0x6b4c, 2, 2, 0); disp_signed_angle(*phase_balance_angle_ptr, 2, 7, 1);
        disp_string((int)0x6b58, 3, 0, 0);
      }
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_basic_parameter_edit(KeyCode key_code)
{
    uint32_t item_index = *ui_item_index_ptr;                              /* 项号 */

    /* 原汇编公共尾部 0x7446-0x744E 对刷新计数加一。提前执行可保持所有
     * 提前返回/辅助绘制调用下的同一可观察结果；按键分支在下方写入最终值。 */
    (*ui_statistics_timeout_ticks_ptr)++;

    /* ---- key_code==1：在 查看/编辑 之间切换 ui_view_mode_ptr，并复位修改空闲计时 ---- */
    if (key_code == KEY_CONFIRM) {
      *ui_idle_timeout_ticks_ptr = 0;
      (*ui_view_mode_ptr)++;
      if (*ui_view_mode_ptr > 1) *ui_view_mode_ptr = 0;
      *ui_statistics_timeout_ticks_ptr = 0xfb;
      if (*ui_view_mode_ptr == 1) *ui_statistics_timeout_ticks_ptr = 0x1f5;
    }

    /* ---- key_code==4：保存并退回 参数子菜单(type2 屏) ---- */
    else if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS; *ui_item_index_ptr = 0;
      param_sync_live_to_eeprom(); disp_clear();
      disp_string((int)0x4814,0,0,1); disp_string((int)0x4824,1,0,0);
      disp_string((int)0x4834,2,0,0); disp_string((int)0x4844,3,0,0);
    }

    /* ---- key_code==2/3 且 *ui_view_mode_ptr==0：项间导航 (ui_item_index_ptr=0..15) ---- */
    else if ((key_code == KEY_DOWN || key_code == KEY_UP) && *ui_view_mode_ptr == 0) {
      *ui_idle_timeout_ticks_ptr = 0;
      if (key_code == KEY_UP) { (*ui_item_index_ptr)++; if (*ui_item_index_ptr > 0xf) *ui_item_index_ptr = 0xf; }
      else          { if (*ui_item_index_ptr > 0) (*ui_item_index_ptr)--; }
      *ui_statistics_timeout_ticks_ptr = 0xfb;
      /* 重绘新项所在页标题(值列清空)；值由尾部整页重绘恢复 */
      item_index = *ui_item_index_ptr;
      switch (item_index >> 2) {
        case 0: disp_string((int)0x6540,0,0,0); disp_string((int)0x6554,1,0,0);
                disp_string((int)0x6568,2,0,0); disp_string((int)0x657c,3,0,0); break;
        case 1: disp_string((int)0x6fe4,0,0,0); disp_string((int)0x6ff8,1,0,0);
                disp_string((int)0x700c,2,0,0); disp_string((int)0x7020,3,0,0); break;
        case 2: disp_string((int)0x7034,0,0,0); disp_string((int)0x7048,1,0,0);
                disp_string((int)0x705c,2,0,0); disp_string((int)0x7070,3,0,0); break;
        default: disp_string((int)0x7084,0,0,0); disp_string((int)0x7098,1,0,0);
                disp_string((int)0x70ac,2,0,0); disp_string((int)0x70c0,3,0,0); break;
      }
    }

    /* ---- key_code==2/0x16/3/0x21 且 *ui_view_mode_ptr==1：修改当前项值 ---- */
    else if ((key_code == KEY_DOWN || key_code == KEY_FAST_UP || key_code == KEY_UP || key_code == KEY_FAST_DOWN) && *ui_view_mode_ptr == 1) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_statistics_timeout_ticks_ptr = 0xfb;
      item_index = *ui_item_index_ptr;
      /* 步进：数字项 key_code == KEY_FAST_UP 快加 +5、key_code == KEY_FAST_DOWN 快减 -5 */
      if (item_index >= 1 && item_index <= 5) {
        if (key_code == KEY_FAST_UP) {                     /* 快加 +5 */
          if (item_index == 1) { *parameter_voltage_range_ptr += 5; if (*parameter_voltage_range_ptr > 0x1770) *parameter_voltage_range_ptr = 0x1770; }
          else if (item_index == 2) { *parameter_current_range_ptr += 5; if (*parameter_current_range_ptr > 0x1770) *parameter_current_range_ptr = 0x1770; }
          else if (item_index == 3) { *parameter_transformer_ratio_ptr += 5;      if (*parameter_transformer_ratio_ptr > 0x1770) *parameter_transformer_ratio_ptr = 0x1770; }
          else if (item_index == 4) { *parameter_voltage_limit_ptr += 5;      if (*parameter_voltage_limit_ptr > *parameter_voltage_range_ptr + 1) *parameter_voltage_limit_ptr = *parameter_voltage_range_ptr + 1; }
          else              { *parameter_current_limit_ptr += 5;      if (*parameter_current_limit_ptr > *parameter_current_range_ptr + 1) *parameter_current_limit_ptr = *parameter_current_range_ptr + 1; }
        } else if (key_code == KEY_FAST_DOWN) {              /* 快减 -5 (数字项下限 0xf) */
          if (item_index == 1) { if (*parameter_voltage_range_ptr < 0x10) *parameter_voltage_range_ptr = 0xf; *parameter_voltage_range_ptr -= 5; }
          else if (item_index == 2) { if (*parameter_current_range_ptr < 0x10) *parameter_current_range_ptr = 0xf; *parameter_current_range_ptr -= 5; }
          else if (item_index == 3) { if (*parameter_transformer_ratio_ptr < 0x10) *parameter_transformer_ratio_ptr = 0xf;      *parameter_transformer_ratio_ptr -= 5; }
          else if (item_index == 4) { if (*parameter_voltage_limit_ptr < 0x10) *parameter_voltage_limit_ptr = 0xf;      *parameter_voltage_limit_ptr -= 5; }
          else              { if (*parameter_current_limit_ptr < 0x10) *parameter_current_limit_ptr = 0xf;      *parameter_current_limit_ptr -= 5; }
        } else {                               /* key_code==2/3：+1/-1 */
          if (key_code == KEY_DOWN) {
            if (item_index == 1) { (*parameter_voltage_range_ptr)++; if (*parameter_voltage_range_ptr > 0x1770) *parameter_voltage_range_ptr = 0x1770; }
            else if (item_index == 2) { (*parameter_current_range_ptr)++; if (*parameter_current_range_ptr > 0x1770) *parameter_current_range_ptr = 0x1770; }
            else if (item_index == 3) { (*parameter_transformer_ratio_ptr)++; if (*parameter_transformer_ratio_ptr > 0x1770) *parameter_transformer_ratio_ptr = 0x1770; }
            else if (item_index == 4) { (*parameter_voltage_limit_ptr)++; if (*parameter_voltage_limit_ptr > *parameter_voltage_range_ptr + 1) *parameter_voltage_limit_ptr = *parameter_voltage_range_ptr + 1; }
            else { (*parameter_current_limit_ptr)++; if (*parameter_current_limit_ptr > *parameter_current_range_ptr + 1) *parameter_current_limit_ptr = *parameter_current_range_ptr + 1; }
          } else {                             /* key_code==3：-1 (数字项下限 0xb) */
            if (item_index == 1) { if (*parameter_voltage_range_ptr > 0xa) (*parameter_voltage_range_ptr)--; }
            else if (item_index == 2) { if (*parameter_current_range_ptr > 0xa) (*parameter_current_range_ptr)--; }
            else if (item_index == 3) { if (*parameter_transformer_ratio_ptr > 0xa) (*parameter_transformer_ratio_ptr)--; }
            else if (item_index == 4) { if (*parameter_voltage_limit_ptr > 0xa) (*parameter_voltage_limit_ptr)--; }
            else { if (*parameter_current_limit_ptr > 0xa) (*parameter_current_limit_ptr)--; }
          }
        }
      } else {
        /* 非数字项(0,6..15)：key_code==2/0x16 +1、key_code==3/0x21 -1 */
        if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {         /* 减 */
          switch (item_index) {
            case 0: if (*parameter_control_mode_ptr == 0) *parameter_control_mode_ptr = 3; (*parameter_control_mode_ptr)--; break;
            case 6: if (*parameter_soft_start_time_ptr > 0) (*parameter_soft_start_time_ptr)--; break;
            case 7: if (*parameter_soft_stop_time_ptr > 0) (*parameter_soft_stop_time_ptr)--; break;
            case 8: if (*parameter_phase_limit_ptr != 0) (*parameter_phase_limit_ptr)--; break;
            case 9: if (*parameter_master_slave_offset_ptr > 0x28) (*parameter_master_slave_offset_ptr)--; break;
            case 10: if (*parameter_control_method_ptr > 0) (*parameter_control_method_ptr)--; break;
            case 11: if (*parameter_start_mode_ptr > 0) (*parameter_start_mode_ptr)--; break;
            case 12: if (*parameter_emergency_stop_ptr > 0) (*parameter_emergency_stop_ptr)--; break;
            case 13: if (*parameter_feedback_mode_ptr > 0) (*parameter_feedback_mode_ptr)--; break;
            case 14: if (*parameter_input_mode_ptr > 0) (*parameter_input_mode_ptr)--; break;
            case 15: if (*parameter_start_phase_ptr != 0) (*parameter_start_phase_ptr)--; break;
          }
        } else {                               /* key_code==2/0x16：加 */
          switch (item_index) {
            case 0: (*parameter_control_mode_ptr)++; if (*parameter_control_mode_ptr > 2) *parameter_control_mode_ptr = 0; break;
            case 6: (*parameter_soft_start_time_ptr)++; if (*parameter_soft_start_time_ptr > 0xc8) *parameter_soft_start_time_ptr = 0xc8; break;
            case 7: (*parameter_soft_stop_time_ptr)++; if (*parameter_soft_stop_time_ptr > 0xc8) *parameter_soft_stop_time_ptr = 0xc8; break;
            case 8: (*parameter_phase_limit_ptr)++; if (*parameter_phase_limit_ptr > 0xb4) *parameter_phase_limit_ptr = 0xb4; break;
            case 9: (*parameter_master_slave_offset_ptr)++; if (*parameter_master_slave_offset_ptr > 0xa0) *parameter_master_slave_offset_ptr = 0xa0; break;
            case 10: (*parameter_control_method_ptr)++; if (*parameter_control_method_ptr > 2) *parameter_control_method_ptr = 2; break;   /* BIN 钳位 */
            case 11: (*parameter_start_mode_ptr)++; if (*parameter_start_mode_ptr > 1) *parameter_start_mode_ptr = 1; break;                  /* BIN 钳位 */
            case 12: (*parameter_emergency_stop_ptr)++; if (*parameter_emergency_stop_ptr > 2) *parameter_emergency_stop_ptr = 2; break;            /* BIN 钳位 */
            case 13: (*parameter_feedback_mode_ptr)++; if (*parameter_feedback_mode_ptr > 1) *parameter_feedback_mode_ptr = 1; break;   /* BIN 钳位 */
            case 14: (*parameter_input_mode_ptr)++; if (*parameter_input_mode_ptr > 1) *parameter_input_mode_ptr = 1; break; /* BIN 钳位 */
            case 15: (*parameter_start_phase_ptr)++; if (*parameter_start_phase_ptr > 0xb4) *parameter_start_phase_ptr = 0xb4; break;
          }
        }
      }
    }

    /* ---- ui_statistics_timeout_ticks_ptr 计数到 0xFB：整页重绘当前页全部 4 项值(当前项高亮)，恢复被标签重绘清掉的值列 ---- */
    if (*ui_statistics_timeout_ticks_ptr == 0xfb) sm3_draw_page(*ui_item_index_ptr);

    /* ---- ui_statistics_timeout_ticks_ptr 超过 0x1F4：回绕为 0；编辑态按 ui_item_index_ptr 用空格擦除当前项值列 ---- */
    /*     与 0xFB 整页重绘(当前项反显)交替 → 值"反显/消失"闪烁，周期≈501 帧 (0x7A32-0x7BD8)。
     *     空格串：0x6474=5空格(item0/10..14)、0x7068=4空格(item1..7)、0x647C=0x6474+8=3空格(item8/9/15)。
     *     item4/5 按 parameter_voltage_range_ptr/parameter_current_range_ptr 是否顶到限位选宽/窄擦除串。 */
    if (*ui_statistics_timeout_ticks_ptr > 0x1f4) {
      *ui_statistics_timeout_ticks_ptr = 0;
      if (*ui_view_mode_ptr == 0) return;              /* 查看态：本帧提前返回(b.adjusted_value 0x4ba8=pop{r4})，跳过 ui_idle_timeout_ticks_ptr++ */
      switch (item_index) {
        case 0:  disp_string(0x6474, 0, 0xb, 0); break;
        case 1:  disp_string(0x7068, 1, 0xb, 0); break;
        case 2:  disp_string(0x7068, 2, 0xb, 0); break;
        case 3:  disp_string(0x7068, 3, 0xb, 0); break;
        case 4:  if (*parameter_voltage_range_ptr >= *parameter_voltage_limit_ptr) disp_string(0x7068, 0, 0xb, 0);
                 else disp_string(0x6474, 0, 0xb, 0);
                 break;
        case 5:  if (*parameter_current_range_ptr >= *parameter_current_limit_ptr) disp_string(0x7068, 1, 0xb, 0);
                 else disp_string(0x6474, 1, 0xb, 0);
                 break;
        case 6:  disp_string(0x7068, 2, 0xb, 0); break;
        case 7:  disp_string(0x7068, 3, 0xb, 0); break;
        case 8:  disp_string(0x6474 + 0x8, 0, 0xb, 0); break;
        case 9:  disp_string(0x6474 + 0x8, 1, 0xb, 0); break;
        case 10: disp_string(0x6474, 2, 0xb, 0); break;
        case 11: disp_string(0x6474, 3, 0xb, 0); break;
        case 12: disp_string(0x6474, 0, 0xb, 0); break;
        case 13: disp_string(0x6474, 1, 0xb, 0); break;
        case 14: disp_string(0x6474, 2, 0xb, 0); break;
        case 15: disp_string(0x6474 + 0x8, 3, 0xb, 0); break;
      }
    }

    /* ---- 恒压/恒流(parameter_control_mode_ptr<2)且软起时间 parameter_soft_start_time_ptr 未配置时自动置 1 (0x7BE2-0x7BF4)；
     *     随后编辑空闲超时回到主屏 (0x7BD8-0x7C16) ---- */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*parameter_control_mode_ptr < 2 && *parameter_soft_start_time_ptr == 0) *parameter_soft_start_time_ptr = 1;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_protection(KeyCode key_code)
{
    (*ui_statistics_timeout_ticks_ptr)++;
    /* ---- key_code==1：切换编辑态 ---- */
    if (key_code == KEY_CONFIRM) {
      *ui_idle_timeout_ticks_ptr = 0;
      (*ui_view_mode_ptr)++;
      if (*ui_view_mode_ptr > 1) *ui_view_mode_ptr = 0;
      *ui_statistics_timeout_ticks_ptr = (*ui_view_mode_ptr == 0) ? 0xfb : 0x1f5;
    }

    /* ---- key_code==4：保存并退回 参数子菜单(type2 屏) ---- */
    else if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS; *ui_item_index_ptr = 1;
      param_sync_live_to_eeprom(); disp_clear();
      disp_string((int)0x4814,0,0,0); disp_string((int)0x4824,1,0,1);
      disp_string((int)0x4834,2,0,0); disp_string((int)0x4844,3,0,0);
    }

    /* ---- key_code==2/3 且 *ui_view_mode_ptr==0：项间导航 (ui_item_index_ptr=0..9) + 画页标题 ---- */
    else if ((key_code == KEY_DOWN || key_code == KEY_UP) && *ui_view_mode_ptr == 0) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_statistics_timeout_ticks_ptr = 0xfb;
      if (key_code == KEY_UP) { (*ui_item_index_ptr)++; if (*ui_item_index_ptr > 9) *ui_item_index_ptr = 9; }
      else          { if (*ui_item_index_ptr > 0) (*ui_item_index_ptr)--; }
      /* 画当前项所在页 4 行标题；页2 的 2/3 行用空串占位 */
      if (*ui_item_index_ptr < 4) {
        disp_string((int)0x65bc,0,0,0); disp_string((int)0x65d0,1,0,0);
        disp_string((int)0x65e4,2,0,0); disp_string((int)0x65f8,3,0,0);
      } else if (*ui_item_index_ptr < 8) {
        disp_string((int)0x7e10,0,0,0); disp_string((int)0x7e24,1,0,0);
        disp_string((int)0x7e38,2,0,0); disp_string((int)0x7e4c,3,0,0);
      } else {
        disp_string((int)0x7e60,0,0,0); disp_string((int)0x7e74,1,0,0);
        disp_string((int)0x5ba4,2,0,0); disp_string((int)0x5ba4,3,0,0);
      }
    }

    /* ---- key_code==2/0x16/3/0x21 且 *ui_view_mode_ptr==1：修改当前项值 ---- */
    else if ((key_code == KEY_DOWN || key_code == KEY_FAST_UP || key_code == KEY_UP || key_code == KEY_FAST_DOWN) && *ui_view_mode_ptr == 1) {
      uint32_t item_index = *ui_item_index_ptr;
      *ui_idle_timeout_ticks_ptr = 0;
      /* 增：key_code==2 / key_code == KEY_FAST_UP → uint8_t 项 +1（时间/缺相/三相平衡），word 项 +1(2)/+5(0x16) */
      if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {
        /* uint8_t 项 1/3/5/7 +1 clamp 0xc8；8 +1 clamp 1；9 +1 clamp 0x3c */
        if (item_index == 1) { (*parameter_overvoltage_time_ptr)++; if (*parameter_overvoltage_time_ptr > 0xc8) *parameter_overvoltage_time_ptr = 0xc8; }
        else if (item_index == 3) { (*parameter_undervoltage_time_ptr)++; if (*parameter_undervoltage_time_ptr > 0xc8) *parameter_undervoltage_time_ptr = 0xc8; }
        else if (item_index == 5) { (*parameter_if_overload_time_ptr)++; if (*parameter_if_overload_time_ptr > 0xc8) *parameter_if_overload_time_ptr = 0xc8; }
        else if (item_index == 7) { (*parameter_ct_overload_time_ptr)++; if (*parameter_ct_overload_time_ptr > 0xc8) *parameter_ct_overload_time_ptr = 0xc8; }
        else if (item_index == 8) { (*parameter_phase_loss_enable_ptr)++; if (*parameter_phase_loss_enable_ptr > 1) *parameter_phase_loss_enable_ptr = 1; }
        else if (item_index == 9) { (*parameter_phase_balance_ptr)++; if (*parameter_phase_balance_ptr > 0x3c) *parameter_phase_balance_ptr = 0x3c; }
        /* word 项 0/2/4/6：key_code==2 +1 / key_code == KEY_FAST_UP +5，上限 parameter_voltage_range_ptr / parameter_current_range_ptr / parameter_transformer_ratio_ptr */
        if (item_index == 0) {
          *parameter_overvoltage_limit_ptr += (key_code == KEY_FAST_UP) ? 5 : 1;
          if (*parameter_overvoltage_limit_ptr > *parameter_voltage_range_ptr) *parameter_overvoltage_limit_ptr = *parameter_voltage_range_ptr;
        } else if (item_index == 2) {
          *parameter_undervoltage_limit_ptr += (key_code == KEY_FAST_UP) ? 5 : 1;
          if (*parameter_undervoltage_limit_ptr > *parameter_voltage_range_ptr) *parameter_undervoltage_limit_ptr = *parameter_voltage_range_ptr;
        } else if (item_index == 4) {
          *parameter_if_overload_limit_ptr += (key_code == KEY_FAST_UP) ? 5 : 1;
          if (*parameter_if_overload_limit_ptr > *parameter_current_range_ptr) *parameter_if_overload_limit_ptr = *parameter_current_range_ptr;
        } else if (item_index == 6) {
          *parameter_ct_overload_limit_ptr += (key_code == KEY_FAST_UP) ? 5 : 1;
          if (*parameter_ct_overload_limit_ptr > *parameter_transformer_ratio_ptr) *parameter_ct_overload_limit_ptr = *parameter_transformer_ratio_ptr;
        }
      } else if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
        /* 减：uint8_t 项 1/3/5/7/8 +1... 实为 >0 → -1；item9 下限 0x9 */
        if (item_index == 1) { uint8_t current_value=*parameter_overvoltage_time_ptr; if (current_value) *parameter_overvoltage_time_ptr = current_value-1; }
        else if (item_index == 3) { uint8_t current_value=*parameter_undervoltage_time_ptr; if (current_value) *parameter_undervoltage_time_ptr = current_value-1; }
        else if (item_index == 5) { uint8_t current_value=*parameter_if_overload_time_ptr; if (current_value) *parameter_if_overload_time_ptr = current_value-1; }
        else if (item_index == 7) { uint8_t current_value=*parameter_ct_overload_time_ptr; if (current_value) *parameter_ct_overload_time_ptr = current_value-1; }
        else if (item_index == 8) { uint8_t current_value=*parameter_phase_loss_enable_ptr; if (current_value) *parameter_phase_loss_enable_ptr = current_value-1; }
        else if (item_index == 9) { uint8_t current_value=*parameter_phase_balance_ptr; if (current_value > 9) *parameter_phase_balance_ptr = current_value-1; }
        /* 减：word 项 0/2/4/6：key_code==3 若 current_value>0 → -1(下限0)；key_code == KEY_FAST_DOWN 若 current_value<6 先置5，再 -5(下限0) */
        if (item_index == 0) { uint32_t current_value=*parameter_overvoltage_limit_ptr; if (key_code == KEY_FAST_DOWN) { uint32_t adjusted_value=(current_value<6)?5:current_value; *parameter_overvoltage_limit_ptr = adjusted_value-5; } else if (current_value) *parameter_overvoltage_limit_ptr=current_value-1; }
        else if (item_index == 2) { uint32_t current_value=*parameter_undervoltage_limit_ptr; if (key_code == KEY_FAST_DOWN) { uint32_t adjusted_value=(current_value<6)?5:current_value; *parameter_undervoltage_limit_ptr = adjusted_value-5; } else if (current_value) *parameter_undervoltage_limit_ptr=current_value-1; }
        else if (item_index == 4) { uint32_t current_value=*parameter_if_overload_limit_ptr; if (key_code == KEY_FAST_DOWN) { uint32_t adjusted_value=(current_value<6)?5:current_value; *parameter_if_overload_limit_ptr = adjusted_value-5; } else if (current_value) *parameter_if_overload_limit_ptr=current_value-1; }
        else if (item_index == 6) { uint32_t current_value=*parameter_ct_overload_limit_ptr; if (key_code == KEY_FAST_DOWN) { uint32_t adjusted_value=(current_value<6)?5:current_value; *parameter_ct_overload_limit_ptr = adjusted_value-5; } else if (current_value) *parameter_ct_overload_limit_ptr=current_value-1; }
      }
      *ui_statistics_timeout_ticks_ptr = 0xfb;
    }

    /* ---- 刷新节流：ui_statistics_timeout_ticks_ptr==0xfb 时重绘当前页（高亮当前项） ---- */
    if (*ui_statistics_timeout_ticks_ptr == 0xfb) sm4_draw_page(*ui_item_index_ptr);

    /* ---- ui_statistics_timeout_ticks_ptr 超过 0x1F4：回绕为 0；编辑态按 ui_item_index_ptr 用空格擦除当前项值列 ---- */
    /*     与 0xFB 整页重绘(当前项反显)交替 → 值"反显/消失"闪烁，周期≈501 帧 (0x85C4-0x874E)。
     *     窄串 0x7E6C=4 空格（word 项 0/2/4/6 值≠0、uint8_t 项 1/3/5/7）；
     *     宽串 0x6474=5 空格（word 项值==0、uint8_t 项 8/9）。 */
    if (*ui_statistics_timeout_ticks_ptr > 0x1f4) {
      *ui_statistics_timeout_ticks_ptr = 0;
      if (*ui_view_mode_ptr == 0) return;              /* 查看态：本帧提前返回 */
      switch (*ui_item_index_ptr) {
        case 0:  if (*parameter_overvoltage_limit_ptr != 0) disp_string(0x7e6c, 0, 0xb, 0);
                 else disp_string(0x6474, 0, 0xb, 0);
                 break;
        case 1:  disp_string(0x7e6c, 1, 0xb, 0); break;
        case 2:  if (*parameter_undervoltage_limit_ptr != 0) disp_string(0x7e6c, 2, 0xb, 0);
                 else disp_string(0x6474, 2, 0xb, 0);
                 break;
        case 3:  disp_string(0x7e6c, 3, 0xb, 0); break;
        case 4:  if (*parameter_if_overload_limit_ptr != 0) disp_string(0x7e6c, 0, 0xb, 0);
                 else disp_string(0x6474, 0, 0xb, 0);
                 break;
        case 5:  disp_string(0x7e6c, 1, 0xb, 0); break;
        case 6:  if (*parameter_ct_overload_limit_ptr != 0) disp_string(0x7e6c, 2, 0xb, 0);
                 else disp_string(0x6474, 2, 0xb, 0);
                 break;
        case 7:  disp_string(0x7e6c, 3, 0xb, 0); break;
        case 8:  disp_string(0x6474, 0, 0xb, 0); break;
        case 9:  disp_string(0x6474, 1, 0xb, 0); break;
      }
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_communication(KeyCode key_code)
{
    (*ui_statistics_timeout_ticks_ptr)++;
    /* key_code==1：进入/退出编辑模式（ui_view_mode_ptr 0<->1） */
    if (key_code == KEY_CONFIRM) {
      *ui_idle_timeout_ticks_ptr = 0;
      (*ui_view_mode_ptr)++;
      if (*ui_view_mode_ptr > 1) *ui_view_mode_ptr = 0;
      *ui_statistics_timeout_ticks_ptr = (*ui_view_mode_ptr == 0) ? 0xfb : 0x1f5;
    }
    /* key_code==4：返回基本参数主菜单（ui_screen_id_ptr=2/ui_item_index_ptr=2），立即写回 EEPROM */
    else if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 2;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x4814, 0, 0, 0);
      disp_string(0x4824, 1, 0, 0);
      disp_string(0x4834, 2, 0, 1);
      disp_string(0x4844, 3, 0, 0);
    }
    /* 导航（ui_view_mode_ptr==0，key2/3 上下移，仅 4 项） */
    else if ((key_code == KEY_DOWN || key_code == KEY_UP) && *ui_view_mode_ptr == 0) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_statistics_timeout_ticks_ptr = 0xfb;
      if (key_code == KEY_UP) { (*ui_item_index_ptr)++; if (*ui_item_index_ptr > 3) *ui_item_index_ptr = 3; }
      if (key_code == KEY_DOWN) { if (*ui_item_index_ptr > 0) (*ui_item_index_ptr)--; }
      if (*ui_item_index_ptr < 4) {   /* 重绘通讯页标题帧（值列 0xb/0xa 由刷新块绘制） */
        disp_string(0x6a18, 0, 0, 0);
        disp_string(0x6a2c, 1, 0, 0);
        disp_string(0x6a40, 2, 0, 0);
        disp_string(0x6a54, 3, 0, 0);
      }
    }
    /* 编辑（ui_view_mode_ptr==1，key2/0x16 增、key3/0x21 减；uint8_t 项恒 ±1，word 项恒 ±1） */
    else if ((key_code == KEY_DOWN || key_code == KEY_FAST_UP || key_code == KEY_UP || key_code == KEY_FAST_DOWN) && *ui_view_mode_ptr == 1) {
      *ui_statistics_timeout_ticks_ptr = 0xfb;
      if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {   /* 增 */
        *ui_idle_timeout_ticks_ptr = 0;
        if (*ui_item_index_ptr == 0) { if (*communication_address_ptr >= 0xf6) *communication_address_ptr = 0xf6; (*communication_address_ptr)++; }
        if (*ui_item_index_ptr == 1) { if (*baud_rate_index_ptr >= 7) *baud_rate_index_ptr = 6; (*baud_rate_index_ptr)++; }
        if (*ui_item_index_ptr == 2) { (*communication_parity_ptr)++; if (*communication_parity_ptr > 3) *communication_parity_ptr = 3; }
        if (*ui_item_index_ptr == 3) *communication_check_ptr = 1;
      }
      if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {   /* 减 */
        *ui_idle_timeout_ticks_ptr = 0;
        if (*ui_item_index_ptr == 0) { if (*communication_address_ptr > 1) (*communication_address_ptr)--; }
        if (*ui_item_index_ptr == 1) { if (*baud_rate_index_ptr != 0) (*baud_rate_index_ptr)--; }
        if (*ui_item_index_ptr == 2) { if (*communication_parity_ptr > 0) (*communication_parity_ptr)--; }
        if (*ui_item_index_ptr == 3) *communication_check_ptr = 0;
      }
    }

    /* ---- 刷新节流：ui_statistics_timeout_ticks_ptr==0xfb 时整页重绘（高亮当前项） ---- */
    if (*ui_statistics_timeout_ticks_ptr == 0xfb) { if (*ui_item_index_ptr < 4) sm5_draw_page(*ui_item_index_ptr); }

    /* ---- 编辑空闲超时：清空当前项所在行（闪烁）后返回主屏 ---- */
    if (*ui_statistics_timeout_ticks_ptr > 0x1f4) {
      *ui_statistics_timeout_ticks_ptr = 0;
      if (*ui_view_mode_ptr == 0) return;
      if (*ui_item_index_ptr == 0 || *ui_item_index_ptr == 4) disp_string(0x6474, 0, 0xb, 0);
      if (*ui_item_index_ptr == 1 || *ui_item_index_ptr == 5) disp_string(0x8f44, 1, 0xa, 0);
      if (*ui_item_index_ptr == 2 || *ui_item_index_ptr == 6) disp_string(0x8f44, 2, 0xa, 0);
      if (*ui_item_index_ptr == 3 || *ui_item_index_ptr == 7) disp_string(0x6474, 3, 0xb, 0);
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_phase_calibration(KeyCode key_code)
{
    *output_enable_state_byte_ptr = 1;
    /* 相位偏移 增（key3/0x21 上限 0x2b0）/ 减（key2/0x16 下限 0x45） */
    if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
      *ui_idle_timeout_ticks_ptr = 0;
      (*frequency_adjustment_ptr)++;
      if (*frequency_adjustment_ptr > 0x2b0) *frequency_adjustment_ptr = 0x2b0;
      disp_offset(*frequency_adjustment_ptr, 2, 7, 1);
    }
    if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {
      *ui_idle_timeout_ticks_ptr = 0;
      if (*frequency_adjustment_ptr < 0x45) *frequency_adjustment_ptr = 0x45;
      (*frequency_adjustment_ptr)--;
      disp_offset(*frequency_adjustment_ptr, 2, 7, 1);
    }
    /* 运行状态行显示（output_fault_flags_ptr!=0 停机 / operation_configuration_ptr 控制 ui_system_status_ptr 0/1/2） */
    if (*output_fault_flags_ptr != 0) {
      *ui_system_status_ptr = 0;
      disp_string(0x47dc, 3, 0xa, 0);
    } else {
      if (*operation_configuration_ptr == 0 && *ui_system_status_ptr != 1) { *ui_system_status_ptr = 1; disp_string(0x47dc + 0xc, 3, 0xa, 0); }
      if (*operation_configuration_ptr == 1 && *ui_system_status_ptr != 2) { *ui_system_status_ptr = 2; disp_string(0x47dc + 0x14, 3, 0xa, 0); }
    }
    if (key_code == KEY_START) { *operation_configuration_ptr = 1; *ui_idle_timeout_ticks_ptr = 0; }
    if (key_code == KEY_STOP) { *operation_configuration_ptr = 0; *ui_idle_timeout_ticks_ptr = 0; gpio_outputs_set(); }
    run_stop_preset();
    if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 5;
      disp_clear();
      disp_string(0x6474 + 0x64, 0, 0, 0);
      disp_string(0x6474 + 0x78, 1, 0, 1);
      disp_string(0x6474 + 0x8c, 2, 0, 0);
      disp_string(0x6474 + 0xa0, 3, 0, 0);
      if (*frequency_adjustment_ptr != *frequency_adjust_shadow_ptr) {  /* 写 EEPROM reg 0xc9/0xca */
        *frequency_adjust_shadow_ptr = *frequency_adjustment_ptr;
        i2c_write_reg((uint16_t)*frequency_adjust_shadow_ptr >> 8, 0xc9);
        i2c_write_reg((uint8_t)*frequency_adjust_shadow_ptr, 0xca);
      }
      gpio_outputs_set();
      *operation_configuration_ptr = 0;
      run_stop_preset();
      *output_enable_state_byte_ptr = 0;
      *ui_system_status_ptr = 0;
      return;
    }
    /* 超时尾（0x3a98=15000） */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x3a98) {
      *ui_idle_timeout_ticks_ptr = 0;
      *operation_configuration_ptr = 0;
      gpio_outputs_set();
      *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU;
      disp_splash_screen();
      *output_enable_state_byte_ptr = 0;
      *ui_system_status_ptr = 0;
    }
    return;

}

static void state_machine_page_runtime_query(KeyCode key_code)
{
    if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 6;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x6474 + 0x64, 0, 0, 0);
      disp_string(0x6474 + 0x78, 1, 0, 0);
      disp_string(0x6474 + 0x8c, 2, 0, 1);
      disp_string(0x6514, 3, 0, 0);
      return;
    }
    /* key_code == KEY_CLEAR_STATISTICS 统计清零 → 4 个时间 word 归零并重显 */
    if (key_code == KEY_CLEAR_STATISTICS) {
      *runtime_current_hour_ptr = 0;
      *runtime_current_minute_ptr = 0;
      *runtime_total_hour_ptr = 0;
      *runtime_total_minute_ptr = 0;
      disp_uint5(*runtime_current_hour_ptr, 1, 3, 0);
      disp_uint2(*runtime_current_minute_ptr, 1, 0xa, 0);
      disp_uint5(*runtime_total_hour_ptr, 3, 3, 0);
      disp_uint2(*runtime_total_minute_ptr, 3, 0xa, 0);
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_version(KeyCode key_code)
{
    if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 7;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x6514 - 0x3c, 0, 0, 0);
      disp_string(0x6514 - 0x28, 1, 0, 0);
      disp_string(0x6514 - 0x14, 2, 0, 0);
      disp_string(0x6514, 3, 0, 1);
      return;
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x3a98) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_manual_balance(KeyCode key_code)
{
    *output_enable_state_byte_ptr = 1;
    /* phase_balance_angle_ptr 增（key2/0x16 上限 0xc7=199）/ 减（key3/0x21 下限 2） */
    if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {
      *ui_idle_timeout_ticks_ptr = 0;
      (*phase_balance_angle_ptr)++;
      if (*phase_balance_angle_ptr > 0xc7) *phase_balance_angle_ptr = 0xc7;
      disp_signed_angle(*phase_balance_angle_ptr, 2, 7, 1);
    }
    if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
      *ui_idle_timeout_ticks_ptr = 0;
      if (*phase_balance_angle_ptr < 2) *phase_balance_angle_ptr = 2;
      (*phase_balance_angle_ptr)--;
      disp_signed_angle(*phase_balance_angle_ptr, 2, 7, 1);
    }
    /* 运行状态行显示（与 case8 相同） */
    if (*output_fault_flags_ptr != 0) {
      disp_string(0x47dc, 3, 0xa, 0);
    } else {
      if (*operation_configuration_ptr == 0 && *ui_system_status_ptr != 1) { *ui_system_status_ptr = 1; disp_string(0x47dc + 0xc, 3, 0xa, 0); }
      if (*operation_configuration_ptr == 1 && *ui_system_status_ptr != 2) { *ui_system_status_ptr = 2; disp_string(0x47dc + 0x14, 3, 0xa, 0); }
    }
    if (key_code == KEY_START) { *operation_configuration_ptr = 1; *ui_idle_timeout_ticks_ptr = 0; }
    if (key_code == KEY_STOP) { *operation_configuration_ptr = 0; *ui_idle_timeout_ticks_ptr = 0; }
    run_stop_preset();
    if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 8;
      disp_clear();
      disp_string((int)0xa130, 0, 0, 1);
      disp_string((int)0xa140, 1, 0, 0);
      disp_string((int)0xa140, 2, 0, 0);
      disp_string((int)0xa140, 3, 0, 0);
      if (*phase_balance_angle_ptr != *eeprom_shadow_phase_calib) {  /* 写 EEPROM reg 0x1c */
        *eeprom_shadow_phase_calib = *phase_balance_angle_ptr;
        i2c_write_reg(*eeprom_shadow_phase_calib, 0x1c);
      }
      *operation_configuration_ptr = 0;
      run_stop_preset();
      *output_enable_state_byte_ptr = 0;
      return;
    }
    /* 超时尾 */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x3a98) {
      *ui_idle_timeout_ticks_ptr = 0;
      *operation_configuration_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU;
      disp_splash_screen();
      *output_enable_state_byte_ptr = 0;
    }
    return;

}

static void state_machine_page_runtime_clear(KeyCode key_code)
{
    if (key_code == KEY_BACK) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    if (key_code == KEY_CLEAR_STATISTICS) {
      *runtime_current_hour_ptr = 0;
      *runtime_current_minute_ptr = 0;
      *runtime_total_hour_ptr = 0;
      *runtime_total_minute_ptr = 0;
      disp_uint5(*runtime_current_hour_ptr, 1, 3, 0);
      disp_uint2(*runtime_current_minute_ptr, 1, 0xa, 0);
      disp_uint5(*runtime_total_hour_ptr, 3, 3, 0);
      disp_uint2(*runtime_total_minute_ptr, 3, 0xa, 0);
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_status_monitor(KeyCode key_code)
{
    if (key_code == KEY_BACK) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); return; }
    /* 每 0xfa 次刷新状态行（标题 + 各故障位对应状态串） */
    (*ui_statistics_timeout_ticks_ptr)++;
    if (*ui_statistics_timeout_ticks_ptr > 0xfa) {
      *ui_statistics_timeout_ticks_ptr = 0;
      disp_string((int)0xa158, 0, 4, 0);
      if (*output_fault_flags_ptr == 0) {
        disp_string((int)0xa164, 2, 0, 0);
      } else {
        if (*output_fault_flags_ptr & 0x4)   disp_string((int)0xa178, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x2)   disp_string((int)0xa178, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x1)   disp_string((int)0xa178, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x8)   disp_string((int)0xa578, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x10)  disp_string((int)0xa590, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x20)  disp_string((int)0xa5a4, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x40)  disp_string((int)0xa5b8, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x80)  disp_string((int)0xa5cc, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x100) disp_string((int)0xa5e0, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x200) disp_string((int)0xa5f4, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x400) disp_string((int)0xa608, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x800) disp_string((int)0xa61c, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x1000) disp_string((int)0xa630, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x4000) disp_string((int)0xa644, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x8000) disp_string((int)0xa658, 2, 0, 0);
        if (*output_fault_flags_ptr & 0x2000) disp_string((int)0xa66c, 2, 0, 0);
      }
    }
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_runtime_hours(KeyCode key_code)
{
    if (key_code == KEY_CONFIRM) {
      /* 输入前密码 password_part_b_ptr（0x100015e6）：逐位校验 password_input_buffer_ptr(0x100015f2) */
      *ui_item_index_ptr = 0;
      while (*ui_item_index_ptr < 6) {
        if (password_input_buffer_ptr[*ui_item_index_ptr] == password_part_b_ptr[*ui_item_index_ptr]) {
          password_input_buffer_ptr[*ui_item_index_ptr] = 0;
          (*ui_item_index_ptr)++;
        } else {
          /* 密码错：显示 '密码错' 标题（0x56dc）+ 延时 3 段 → 回主菜单 4 行 */
          disp_clear();
          disp_string(0x56dc, 1, 4, 0);
          sm6_delay_loop();
          *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
          *ui_item_index_ptr = 3;
          disp_clear();
          disp_string(0x4814, 0, 0, 0);
          disp_string(0x4814 + 0x10, 1, 0, 0);
          disp_string(0x4814 + 0x20, 2, 0, 0);
          disp_string(0x4814 + 0x30, 3, 0, 1);
          return;
        }
      }
      /* 密码对：提示 0x8f6c → 延时 → 0x8f78 → 延时 → 0x8f88 → 延时 → 清 EEPROM reg5/6 → 死等 */
      disp_clear();
      disp_string((int)0x8f6c, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f78, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f88, 1, 0, 0);
      sm6_delay_loop();
      i2c_write_reg(0, 5);
      i2c_write_reg(0, 6);
      for (;;) {}   /* 0x8DEE 死等（系统重置进入初始参数） */
    }
    else if (key_code == KEY_INITIAL_PARAMETER_PASSWORD) {
      /* 初始密码 password_part_c_ptr（0x100015ec）：校验 password_input_buffer_ptr */
      *ui_item_index_ptr = 0;
      while (*ui_item_index_ptr < 6) {
        if (password_input_buffer_ptr[*ui_item_index_ptr] == password_part_c_ptr[*ui_item_index_ptr]) {
          password_input_buffer_ptr[*ui_item_index_ptr] = 0;
          (*ui_item_index_ptr)++;
        } else {
          disp_clear();
          disp_string(0x56dc, 1, 4, 0);
          sm6_delay_loop();
          *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
          *ui_item_index_ptr = 3;
          disp_clear();
          disp_string(0x4814, 0, 0, 0);
          disp_string(0x4814 + 0x10, 1, 0, 0);
          disp_string(0x4814 + 0x20, 2, 0, 0);
          disp_string(0x4814 + 0x30, 3, 0, 1);
          return;
        }
      }
      /* 密码对：清 EEPROM reg5/6/7/8 → 死等 */
      disp_clear();
      disp_string((int)0x8f9c, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f78, 1, 0, 0);
      sm6_delay_loop();
      disp_string((int)0x8f88, 1, 0, 0);
      sm6_delay_loop();
      i2c_write_reg(0, 5);
      i2c_write_reg(0, 6);
      i2c_write_reg(0, 7);
      i2c_write_reg(0, 8);
      for (;;) {}
    }
    else if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 3;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x4814, 0, 0, 0);
      disp_string(0x4814 + 0x10, 1, 0, 0);
      disp_string(0x4814 + 0x20, 2, 0, 0);
      disp_string(0x4814 + 0x30, 3, 0, 1);
      return;
    }
    else if (key_code != KEY_NONE) {
      /* 密码数字输入（key_code==其它正值都当数字）——key_code<=0 走超时尾 */
      if (*ui_item_index_ptr < 6) {
        password_input_buffer_ptr[*ui_item_index_ptr] = key_code;
        disp_render_char8(0x2a, 1, (uint8_t)(*ui_item_index_ptr + 7), 0);
        (*ui_item_index_ptr)++;
      }
      return;
    }
    /* 超时尾（0x1388=5000） */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) { *ui_idle_timeout_ticks_ptr = 0; *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); }
    return;

}

static void state_machine_page_pid(KeyCode key_code)
{
    (*ui_statistics_timeout_ticks_ptr)++;
    /* key_code==1 编辑/浏览切换：ui_view_mode_ptr 在 0/1 间翻转，随之调整 O 刷新节奏 */
    if (key_code == KEY_CONFIRM) {
      *ui_idle_timeout_ticks_ptr = 0;
      (*ui_view_mode_ptr)++;
      if (*ui_view_mode_ptr > 1) *ui_view_mode_ptr = 0;
      if (*ui_view_mode_ptr == 0) *ui_statistics_timeout_ticks_ptr = 0xfb;
      if (*ui_view_mode_ptr == 1) *ui_statistics_timeout_ticks_ptr = 0x1f5;
    }
    /* key_code==4 回主菜单：当前 PID 模式槽复制到显示缓冲 0x1000170e/0x1000170f */
    else if (key_code == KEY_BACK) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_BASIC_PARAMETERS;
      *ui_item_index_ptr = 4;
      param_sync_live_to_eeprom();
      disp_clear();
      disp_string(0x64d8, 0, 0, 1);
      disp_string(0x64d8 + 0x14, 1, 0, 0);
      disp_string(0x64d8 + 0x28, 2, 0, 0);
      disp_string(0x64d8 + 0x3c, 3, 0, 0);
      if (*parameter_pid_profile_ptr == 1) { *parameter_active_gain_a_ptr = *parameter_profile1_gain_a_ptr; *parameter_active_gain_b_ptr = *parameter_profile1_gain_b_ptr; }
      if (*parameter_pid_profile_ptr == 2) { *parameter_active_gain_a_ptr = *parameter_profile2_gain_a_ptr; *parameter_active_gain_b_ptr = *parameter_profile2_gain_b_ptr; }
      if (*parameter_pid_profile_ptr == 3) { *parameter_active_gain_a_ptr = *parameter_profile3_gain_a_ptr; *parameter_active_gain_b_ptr = *parameter_profile3_gain_b_ptr; }
      if (*parameter_pid_profile_ptr == 4) { *parameter_active_gain_a_ptr = *parameter_profile4_gain_a_ptr; *parameter_active_gain_b_ptr = *parameter_profile4_gain_b_ptr; }
    }
    /* key_code==2/3/0x16/0x21：*ui_view_mode_ptr==1 编辑，否则 (key_code==2/3 && PIDMODE==4) 导航 */
    if (key_code == KEY_DOWN || key_code == KEY_FAST_UP || key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
      if (*ui_view_mode_ptr == 1) {
        *ui_statistics_timeout_ticks_ptr = 0xfb;
        /* ---------- 编辑 ---------- */
        *ui_idle_timeout_ticks_ptr = 0;
        if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
          /* 降方向（0x933C-0x94E8） */
          if (*ui_item_index_ptr == 0) { (*parameter_pid_profile_ptr)++; if (*parameter_pid_profile_ptr >= 4) *parameter_pid_profile_ptr = 4; }
          if (*ui_item_index_ptr == 1) { if (*parameter_pid_profile_ptr == 4) { if (*parameter_profile4_gain_a_ptr > 1) (*parameter_profile4_gain_a_ptr)--; } }
          if (*ui_item_index_ptr == 2) { if (*parameter_pid_profile_ptr == 4) { if (*parameter_profile4_gain_b_ptr > 1) (*parameter_profile4_gain_b_ptr)--; } }
          if (*ui_item_index_ptr == 4) { if (*parameter_closed_loop_upper_ptr > 1) (*parameter_closed_loop_upper_ptr)--; }
          if (*ui_item_index_ptr == 5) { if (*parameter_closed_loop_lower_ptr > 1) (*parameter_closed_loop_lower_ptr)--; }
          if (*ui_item_index_ptr == 6) { if (*parameter_closed_loop_gain_high_ptr > 1) (*parameter_closed_loop_gain_high_ptr)--; }
          if (*ui_item_index_ptr == 7) { if (*parameter_closed_loop_gain_mid_ptr > 1) (*parameter_closed_loop_gain_mid_ptr)--; }
          if (*ui_item_index_ptr == 8) { if (*parameter_closed_loop_gain_low_ptr > 1) (*parameter_closed_loop_gain_low_ptr)--; }
        } else {
          /* 升方向（0x94EA-0x960E） */
          if (*ui_item_index_ptr == 0) { if (*parameter_pid_profile_ptr > 1) (*parameter_pid_profile_ptr)--; }
          if (*ui_item_index_ptr == 1) { if (*parameter_pid_profile_ptr == 4) { (*parameter_profile4_gain_a_ptr)++; if (*parameter_profile4_gain_a_ptr >= 0x80) *parameter_profile4_gain_a_ptr = 0x80; } }
          if (*ui_item_index_ptr == 2) { if (*parameter_pid_profile_ptr == 4) { (*parameter_profile4_gain_b_ptr)++; if (*parameter_profile4_gain_b_ptr >= 0x80) *parameter_profile4_gain_b_ptr = 0x80; } }
          if (*ui_item_index_ptr == 4) { (*parameter_closed_loop_upper_ptr)++; if (*parameter_closed_loop_upper_ptr >= 0xfa) *parameter_closed_loop_upper_ptr = 0xfa; }
          if (*ui_item_index_ptr == 5) { (*parameter_closed_loop_lower_ptr)++; if (*parameter_closed_loop_lower_ptr > *parameter_closed_loop_upper_ptr) *parameter_closed_loop_lower_ptr = *parameter_closed_loop_upper_ptr; }
          if (*ui_item_index_ptr == 6) { (*parameter_closed_loop_gain_high_ptr)++; if (*parameter_closed_loop_gain_high_ptr >= 0xfa) *parameter_closed_loop_gain_high_ptr = 0xfa; }
          if (*ui_item_index_ptr == 7) { (*parameter_closed_loop_gain_mid_ptr)++; if (*parameter_closed_loop_gain_mid_ptr > *parameter_closed_loop_gain_high_ptr) *parameter_closed_loop_gain_mid_ptr = *parameter_closed_loop_gain_high_ptr; }
          if (*ui_item_index_ptr == 8) { (*parameter_closed_loop_gain_low_ptr)++; if (*parameter_closed_loop_gain_low_ptr > *parameter_closed_loop_gain_mid_ptr) *parameter_closed_loop_gain_low_ptr = *parameter_closed_loop_gain_mid_ptr; }
        }
      }
      else if ((key_code == KEY_DOWN || key_code == KEY_UP) && *parameter_pid_profile_ptr == 4) {
        /* ---------- 导航（0x921E-0x931A）：切换子项页标题 ---------- */
        *ui_idle_timeout_ticks_ptr = 0;
        *ui_statistics_timeout_ticks_ptr = 0xfb;
        if (key_code == KEY_UP) { (*ui_item_index_ptr)++; if (*ui_item_index_ptr > 8) *ui_item_index_ptr = 8; }
        if (key_code == KEY_DOWN) { if (*ui_item_index_ptr > 0) (*ui_item_index_ptr)--; }
        if (*ui_item_index_ptr < 4) {
          disp_string(0x6aa4, 0, 0, 0);
          disp_string(0x6aa4 + 0x14, 1, 0, 0);
          disp_string(0x6aa4 + 0x28, 2, 0, 0);
          disp_string(0x6aa4 + 0x3c, 3, 0, 0);
        }
        if (*ui_item_index_ptr >= 4 && *ui_item_index_ptr < 8) {
          disp_string((int)0x9400, 0, 0, 0);
          disp_string((int)0x9414, 1, 0, 0);
          disp_string((int)0x9428, 2, 0, 0);
          disp_string((int)0x943c, 3, 0, 0);
        }
        if (*ui_item_index_ptr >= 8 && *ui_item_index_ptr < 0xc) {
          disp_string((int)0x9450, 0, 0, 0);
          disp_string(0x5ba4, 1, 0, 0);
          disp_string(0x5ba4, 2, 0, 0);
          disp_string(0x5ba4, 3, 0, 0);
        }
      }
    }
    /* ---------- 刷新区（0x9614-0x9984）：ui_statistics_timeout_ticks_ptr 计到 0xfb 时按 ui_item_index_ptr 重绘 ---------- */
    if (*ui_statistics_timeout_ticks_ptr == 0xfb) {
      if (*ui_item_index_ptr < 4) {
        /* 模式名 row0 col0xb（PIDMODE 1-4 → 0x6af8 基） */
        if (*ui_item_index_ptr == 0) {
          if (*parameter_pid_profile_ptr == 1) disp_string(0x6af8, 0, 0xb, 1);
          if (*parameter_pid_profile_ptr == 2) disp_string(0x6af8 + 0x10, 0, 0xb, 1);
          if (*parameter_pid_profile_ptr == 3) disp_string(0x6af8 + 0x1c, 0, 0xb, 1);
          if (*parameter_pid_profile_ptr == 4) disp_string(0x6af8 + 0x2c, 0, 0xb, 1);
        } else {
          if (*parameter_pid_profile_ptr == 1) disp_string(0x6af8, 0, 0xb, 0);
          if (*parameter_pid_profile_ptr == 2) disp_string(0x6af8 + 0x10, 0, 0xb, 0);
          if (*parameter_pid_profile_ptr == 3) disp_string(0x6af8 + 0x1c, 0, 0xb, 0);
          if (*parameter_pid_profile_ptr == 4) disp_string(0x6af8 + 0x2c, 0, 0xb, 0);
        }
        /* P 值 row1（PIDMODE 1-4 槽 = 0x10001711/13/15/17） */
        if (*ui_item_index_ptr == 1) {
          if (*parameter_pid_profile_ptr == 1) disp_uint4(*parameter_profile1_gain_a_ptr, 1, 0xb, 1);
          if (*parameter_pid_profile_ptr == 2) disp_uint4(*parameter_profile2_gain_a_ptr, 1, 0xb, 1);
          if (*parameter_pid_profile_ptr == 3) disp_uint4(*parameter_profile3_gain_a_ptr, 1, 0xb, 1);
          if (*parameter_pid_profile_ptr == 4) disp_uint4(*parameter_profile4_gain_a_ptr, 1, 0xb, 1);
        } else {
          if (*parameter_pid_profile_ptr == 1) disp_uint4(*parameter_profile1_gain_a_ptr, 1, 0xb, 0);
          if (*parameter_pid_profile_ptr == 2) disp_uint4(*parameter_profile2_gain_a_ptr, 1, 0xb, 0);
          if (*parameter_pid_profile_ptr == 3) disp_uint4(*parameter_profile3_gain_a_ptr, 1, 0xb, 0);
          if (*parameter_pid_profile_ptr == 4) disp_uint4(*parameter_profile4_gain_a_ptr, 1, 0xb, 0);
        }
        /* I 值 row2（PIDMODE 1-4 槽 = 0x10001712/14/16/18） */
        if (*ui_item_index_ptr == 2) {
          if (*parameter_pid_profile_ptr == 1) disp_uint4(*parameter_profile1_gain_b_ptr, 2, 0xb, 1);
          if (*parameter_pid_profile_ptr == 2) disp_uint4(*parameter_profile2_gain_b_ptr, 2, 0xb, 1);
          if (*parameter_pid_profile_ptr == 3) disp_uint4(*parameter_profile3_gain_b_ptr, 2, 0xb, 1);
          if (*parameter_pid_profile_ptr == 4) disp_uint4(*parameter_profile4_gain_b_ptr, 2, 0xb, 1);
        } else {
          if (*parameter_pid_profile_ptr == 1) disp_uint4(*parameter_profile1_gain_b_ptr, 2, 0xb, 0);
          if (*parameter_pid_profile_ptr == 2) disp_uint4(*parameter_profile2_gain_b_ptr, 2, 0xb, 0);
          if (*parameter_pid_profile_ptr == 3) disp_uint4(*parameter_profile3_gain_b_ptr, 2, 0xb, 0);
          if (*parameter_pid_profile_ptr == 4) disp_uint4(*parameter_profile4_gain_b_ptr, 2, 0xb, 0);
        }
        /* ui_item_index_ptr<4 仅 mode/P/I 三行（PI 控制无 D 槽）。ui_item_index_ptr==3 无值可显，走下行 inv=0 */
        /* （反汇编 cmp *ui_item_index_ptr,#4 / blt 0x994e 确认：ui_item_index_ptr<4 全送 <8→0x9984，无 row3 绘制） */
      } else if (*ui_item_index_ptr < 8) {
        /* 增益子项 row0-3（0x10001722-25） */
        if (*ui_item_index_ptr == 4) disp_uint4(*parameter_closed_loop_upper_ptr, 0, 0xb, 1);
        else           disp_uint4(*parameter_closed_loop_upper_ptr, 0, 0xb, 0);
        if (*ui_item_index_ptr == 5) disp_uint4(*parameter_closed_loop_lower_ptr, 1, 0xb, 1);
        else           disp_uint4(*parameter_closed_loop_lower_ptr, 1, 0xb, 0);
        if (*ui_item_index_ptr == 6) disp_uint4(*parameter_closed_loop_gain_high_ptr, 2, 0xb, 1);
        else           disp_uint4(*parameter_closed_loop_gain_high_ptr, 2, 0xb, 0);
        if (*ui_item_index_ptr == 7) disp_uint4(*parameter_closed_loop_gain_mid_ptr, 3, 0xb, 1);
        else           disp_uint4(*parameter_closed_loop_gain_mid_ptr, 3, 0xb, 0);
      } else if (*ui_item_index_ptr >= 8 && *ui_item_index_ptr < 0xc) {
        /* 增益子项 row0（0x10001726） */
        if (*ui_item_index_ptr == 8) disp_uint4(*parameter_closed_loop_gain_low_ptr, 0, 0xb, 1);
        else           disp_uint4(*parameter_closed_loop_gain_low_ptr, 0, 0xb, 0);
      }
    }
    /* ---------- 超时清高亮（0x9984-0x9A56） ---------- */
    if (*ui_statistics_timeout_ticks_ptr > 0x1f4) {
      *ui_statistics_timeout_ticks_ptr = 0;
      if (*ui_view_mode_ptr == 0) return;                 /* b.adjusted_value 0x4ba8 回到 case1 主界面 */
      /* 按 ui_item_index_ptr 清当前行（0x6474 空格，col0xb） */
      if (*ui_item_index_ptr == 0) disp_string(0x6474, 0, 0xb, 0);
      if (*ui_item_index_ptr == 1) disp_string(0x6474, 1, 0xb, 0);
      if (*ui_item_index_ptr == 2) disp_string(0x6474, 2, 0xb, 0);
      if (*ui_item_index_ptr == 3) disp_string(0x6474, 3, 0xb, 0);
      if (*ui_item_index_ptr == 4) disp_string(0x6474, 0, 0xb, 0);
      if (*ui_item_index_ptr == 5) disp_string(0x6474, 1, 0xb, 0);
      if (*ui_item_index_ptr == 6) disp_string(0x6474, 2, 0xb, 0);
      if (*ui_item_index_ptr == 7) disp_string(0x6474, 3, 0xb, 0);
      if (*ui_item_index_ptr == 8) disp_string(0x6474, 0, 0xb, 0);
    }
    /* ---------- 超时尾（0x9A56-0x9A82，0xc350=50000） ---------- */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0xc350) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU;
      disp_splash_screen();
    }
    return;

}

static void state_machine_page_authentication(KeyCode key_code)
{
    /* ---- key_code==4 回主菜单（0xA2D0-A2E4） ---- */
    if (key_code == KEY_BACK) { *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU; disp_splash_screen(); *ui_idle_timeout_ticks_ptr = 0; return; }

    /* ---- key_code==1 且当前停机 → 初始参数屏（0xA2E6-A32C） ---- */
    if (key_code == KEY_CONFIRM && *operation_configuration_ptr == 0) {
      disp_clear();
      *ui_screen_id_ptr = 0xa; *ui_item_index_ptr = 0; *ui_idle_timeout_ticks_ptr = 0; *ui_calibration_timeout_ticks_ptr = 0x3c; *ui_idle_refresh_ticks_ptr = 0;
      disp_string((int)0x4d9c, 1, 0, 0);
      disp_string((int)0x4d9c + 0x10, 3, 7, 0);
      return;
    }

    /* ---- ui_idle_refresh_ticks_ptr 周期刷新 + 继电器/模式切换（0xA32E-A43C） ---- */
    (*ui_idle_refresh_ticks_ptr)++;
    if (*ui_idle_refresh_ticks_ptr >= 0x15e) {
      *ui_idle_refresh_ticks_ptr = 0;
      disp_uint4(*adc_output_current_a_ptr, 0, 9, 0);   /* 0x10001598 */
      disp_uint4(*adc_output_current_b_ptr, 1, 9, 0);   /* 0x1000159c */
      disp_uint4(*adc_output_current_c_ptr, 2, 9, 0);   /* 0x100015a0 */
      if (*output_fault_flags_ptr != 0) {
        *ui_system_status_ptr = 0;
        disp_string((int)0x47dc, 3, 0xa, 0);
      } else if (*operation_configuration_ptr == 0 && *ui_system_status_ptr != 1) {
        *ui_system_status_ptr = 1;
        disp_string((int)0x47dc + 0xc, 3, 0xa, 0);
      }
      if (*parameter_control_mode_ptr == 0) {
        if (*ui_control_display_mode_ptr != 1) { *ui_control_display_mode_ptr = 1; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string((int)0x47dc + 0x20, 3, 0, 0); }  /* 0xA3B0/B8：parameter_control_mode_ptr!=0 或 ui_control_display_mode_ptr==1 时跳过 */
      }
      if (*parameter_control_mode_ptr == 1) {
        if (*ui_control_display_mode_ptr != 2) { *ui_control_display_mode_ptr = 2; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string((int)0x47dc + 0x28, 3, 0, 0); }
      }
      if (*parameter_control_mode_ptr == 2) {
        if (*ui_control_display_mode_ptr != 3) { *ui_control_display_mode_ptr = 3; fio1_pin20_ctrl(0); fio1_pin21_ctrl(0); disp_string((int)0x47dc + 0x30, 3, 0, 0); }
      }
    }

    /* ---- 状态机：复位按键去抖 → 故障停机斜坡 / STOP 段（0xA43C-A708） ---- */
    *emergency_stop_debounce_ptr = debounce_p117();
    if (*output_fault_flags_ptr != 0 && *emergency_stop_debounce_ptr == 2) {
      if (*parameter_auxiliary_mode_ptr == 0) {
        *output_fault_flags_ptr = 0; *operation_configuration_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0;
        disp_string((int)0x522c, 3, 0xa, 0);
        *watchdog_delay_outer_ticks_ptr = 0;
        for (;;) { *watchdog_delay_inner_ticks_ptr = 0; do { (*watchdog_delay_inner_ticks_ptr)++; } while (*watchdog_delay_inner_ticks_ptr < 0x7d0); wd_feed(); (*watchdog_delay_outer_ticks_ptr)++; if (*watchdog_delay_outer_ticks_ptr >= 0xbb8) break; }
        disp_string((int)0x522c + 0x10, 3, 0xa, 0);
        *watchdog_delay_outer_ticks_ptr = 0;
        for (;;) { *watchdog_delay_inner_ticks_ptr = 0; do { (*watchdog_delay_inner_ticks_ptr)++; } while (*watchdog_delay_inner_ticks_ptr < 0x7d0); wd_feed(); (*watchdog_delay_outer_ticks_ptr)++; if (*watchdog_delay_outer_ticks_ptr >= 0xbb8) break; }
        for (;;) { }              /* 0xA50A 故障停机后锁定，等看门狗复位 */
      }
    }
    if (*parameter_auxiliary_mode_ptr == 1) {
      if (*emergency_stop_debounce_ptr != 2 && *ui_control_display_mode_ptr != 1) { *ui_secondary_display_mode_ptr = 1; *parameter_control_mode_ptr = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string((int)0x47dc + 0x20, 3, 0, 0); }
      if (*emergency_stop_debounce_ptr == 2 && *ui_control_display_mode_ptr != 2) { *ui_secondary_display_mode_ptr = 2; *parameter_control_mode_ptr = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string((int)0x4804, 3, 0, 0); }
    }
    if (*parameter_auxiliary_mode_ptr == 2) {
      if (*emergency_stop_debounce_ptr != 2) *reset_output_state_ptr = 0;
      if (*emergency_stop_debounce_ptr == 2) *reset_output_state_ptr = 1;
    }
    *emergency_stop_debounce_ptr = debounce_p06();     /* 急停去抖覆盖 emergency_stop_debounce_ptr */

    /* ---- 故障/急停/启停逻辑（0xA710-A96C） ---- */
    if (*output_fault_flags_ptr == 0 && *emergency_stop_debounce_ptr == 2 && *parameter_emergency_stop_ptr == 0) {
      *run_request_ptr = 1; *operation_configuration_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0;
      if (*status_message_shown_ptr == 0) { disp_string((int)0x4804 - 0x1c, 3, 0xa, 0); *status_message_shown_ptr = 1; }
      return;
    }
    if (*parameter_emergency_stop_ptr == 1) {
      if (*emergency_stop_debounce_ptr != 2 && *ui_control_display_mode_ptr != 1) { *ui_secondary_display_mode_ptr = 1; *parameter_control_mode_ptr = 0; fio1_pin20_ctrl(1); fio1_pin21_ctrl(0); disp_string((int)0x4804 - 0x8, 3, 0, 0); }
      if (*emergency_stop_debounce_ptr == 2 && *ui_control_display_mode_ptr != 2) { *ui_secondary_display_mode_ptr = 2; *parameter_control_mode_ptr = 1; fio1_pin20_ctrl(0); fio1_pin21_ctrl(1); disp_string((int)0x4804, 3, 0, 0); }
    }
    if (*parameter_emergency_stop_ptr == 2) { if (*emergency_stop_debounce_ptr != 2) *reset_output_state_ptr = 0; if (*emergency_stop_debounce_ptr == 2) *reset_output_state_ptr = 1; }
    if (*parameter_auxiliary_mode_ptr != 2 && *parameter_emergency_stop_ptr != 2) *reset_output_state_ptr = 0;
    *ui_scan_stop_flag_ptr = scan_run_stop();
    if (*output_fault_flags_ptr == 0 && *stop_request_ptr == 0 && *run_stop_state_ptr == 1 && *parameter_control_method_ptr == 0) {
      *stop_request_ptr = 1; *stop_pending_ptr = 0; *run_request_ptr = 0; *operation_configuration_ptr = 1; *status_message_shown_ptr = 0;
      *runtime_tick_ptr = 0; *runtime_current_minute_ptr = 0; *runtime_current_hour_ptr = 0;
      disp_string((int)0x4804 - 0x14, 3, 0xa, 0);
    }
    if (*output_fault_flags_ptr == 0 && *stop_pending_ptr == 0 && *run_stop_state_ptr == 0 && *parameter_control_method_ptr == 0) {  /* 0xA85E 块A停机（run_stop_state_ptr==0，无 key_code 要求） */
      *stop_pending_ptr = 1; *stop_request_ptr = 0; *operation_configuration_ptr = 0;
      disp_string((int)0x4804 - 0x1c, 3, 0xa, 0);
    }
    if (*operation_configuration_ptr == 0 && *parameter_control_method_ptr != 0) *run_stop_state_ptr = 0;
    if (*output_fault_flags_ptr == 0 && *stop_request_ptr == 0) {
      if (key_code == KEY_START || *ui_scan_stop_flag_ptr == 7) {
        if (*parameter_start_mode_ptr == 0 || (*ui_scan_stop_flag_ptr == 7 && *parameter_start_mode_ptr == 1 && *parameter_control_method_ptr != 0)) {
          *run_stop_state_ptr = 1; *stop_request_ptr = 1; *stop_pending_ptr = 0; *run_request_ptr = 0; *operation_configuration_ptr = 1; *status_message_shown_ptr = 0;
          *runtime_tick_ptr = 0; *runtime_current_minute_ptr = 0; *runtime_current_hour_ptr = 0;
          disp_string((int)0x4804 - 0x14, 3, 0xa, 0);
        }
      }
    }
    if (*output_fault_flags_ptr == 0 && *stop_pending_ptr == 0) {
      if (key_code == KEY_STOP || *ui_scan_stop_flag_ptr == 8) {
        if (*parameter_start_mode_ptr == 0 || (*ui_scan_stop_flag_ptr == 8 && *parameter_start_mode_ptr == 1 && *parameter_control_method_ptr != 0)) {
          *run_stop_state_ptr = 0; *stop_pending_ptr = 1; *stop_request_ptr = 0; *operation_configuration_ptr = 0;
          disp_string((int)0x4804 - 0x1c, 3, 0xa, 0);
        }
      }
    }

    /* ---- 幅值/频率计算 + 手动调节（0xA96C-AA9A） ---- */
    if (*parameter_control_method_ptr == 0) {
      *output_reference_value_ptr = *frequency_reference_ptr;
      if (*parameter_control_mode_ptr == 0) *target_amplitude_ptr = (*frequency_reference_ptr * *parameter_voltage_range_ptr) / 1000;
      if (*parameter_control_mode_ptr == 1) *target_amplitude_ptr = (*frequency_reference_ptr * *parameter_current_range_ptr) / 1000;
      *output_reference_average_ptr = *target_amplitude_ptr;
      *output_secondary_reference_ptr = *target_amplitude_ptr;
    }
    if (*parameter_control_method_ptr == 1) *output_reference_average_ptr = *output_secondary_reference_ptr;
    if (*parameter_control_method_ptr == 2) {
      if (key_code == KEY_DOWN || key_code == KEY_FAST_UP) {
        (*manual_reference_value_ptr)++;
        if (*manual_reference_value_ptr >= 0x3e8) *manual_reference_value_ptr = 0x3e8;
        if (*manual_reference_value_ptr <= 0xa) *manual_reference_value_ptr = 0xa;
        disp_fixed_1dec(*manual_reference_value_ptr, 0, 9, 0);
      }
      if (key_code == KEY_UP || key_code == KEY_FAST_DOWN) {
        if (*manual_reference_value_ptr <= 0xa) *manual_reference_value_ptr = 0x1;
        (*manual_reference_value_ptr)--;
        disp_fixed_1dec(*manual_reference_value_ptr, 0, 9, 0);
      }
      *output_reference_value_ptr = *manual_reference_value_ptr;
      if (*parameter_control_mode_ptr == 0) *manual_scaled_output_ptr = (*manual_reference_value_ptr * *parameter_voltage_range_ptr) / 1000;
      if (*parameter_control_mode_ptr == 1) *manual_scaled_output_ptr = (*manual_reference_value_ptr * *parameter_current_range_ptr) / 1000;
      *output_reference_average_ptr = *manual_scaled_output_ptr;
      *output_secondary_reference_ptr = *manual_scaled_output_ptr;
    }

    /* ---- 超时尾（0xAA9A，0x1388=5000） ---- */
    (*ui_idle_timeout_ticks_ptr)++;
    if (*ui_idle_timeout_ticks_ptr >= 0x1388) {
      *ui_idle_timeout_ticks_ptr = 0;
      *ui_screen_id_ptr = UI_SCREEN_MAIN_MENU;
      disp_splash_screen();
    }
    return;

}

static void state_machine_dispatch_pages(KeyCode key_code)
{
  switch (*ui_screen_id_ptr) {
  case UI_SCREEN_MAIN_MENU:
    state_machine_page_main(key_code);
    return;
  case UI_SCREEN_CALIBRATION_MENU:
    state_machine_page_calibration_menu(key_code);
    return;
  case UI_SCREEN_CALIBRATION_ACTIVE:
    state_machine_page_calibration_active(key_code);
    return;
  case UI_SCREEN_CALIBRATION_RESULT:
    state_machine_page_calibration_result(key_code);
    return;
  case UI_SCREEN_BASIC_PARAMETERS:
    state_machine_page_basic_parameters(key_code);
    return;
  case UI_SCREEN_BASIC_PARAMETER_EDIT:
    state_machine_page_basic_parameter_edit(key_code);
    return;
  case UI_SCREEN_PROTECTION_PARAMETERS:
    state_machine_page_protection(key_code);
    return;
  case UI_SCREEN_COMMUNICATION_PARAMETERS:
    state_machine_page_communication(key_code);
    return;
  case UI_SCREEN_PHASE_CALIBRATION:
    state_machine_page_phase_calibration(key_code);
    return;
  case UI_SCREEN_RUNTIME_QUERY:
    state_machine_page_runtime_query(key_code);
    return;
  case UI_SCREEN_VERSION:
    state_machine_page_version(key_code);
    return;
  case UI_SCREEN_MANUAL_BALANCE:
    state_machine_page_manual_balance(key_code);
    return;
  case UI_SCREEN_RUNTIME_CLEAR:
    state_machine_page_runtime_clear(key_code);
    return;
  case UI_SCREEN_STATUS_MONITOR:
    state_machine_page_status_monitor(key_code);
    return;
  case UI_SCREEN_RUNTIME_HOURS:
    state_machine_page_runtime_hours(key_code);
    return;
  case UI_SCREEN_PID_PARAMETERS:
    state_machine_page_pid(key_code);
    return;
  case UI_SCREEN_AUTHENTICATION:
    state_machine_page_authentication(key_code);
    return;
  default:
    return;
  }
}
