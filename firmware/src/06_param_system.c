/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 06：参数系统（EEPROM 装载 / live→EEPROM 同步）
 *
 * 存储：AT24C02C EEPROM @0x53（uint8_t 寻址，见 04_i2c.c），双银行备份：
 *   银行 A 魔数 reg5/6 == 'U'(0x55)
 *       有效 → 从芯片 regs 0x0A..0x9C 读入 live 区 0x10002A0C..0x10002ABC
 *       无效 → 用默认区 0x10002EC4..0x10002F68 回写芯片 + reg5/6=0x55
 *   银行 B 魔数 reg7/8 == 'f'(0x66)
 *       有效 → 从芯片 regs 0x1F..0xBF 读入 live 区 0x10003398..0x100033D4
 *       无效 → 用默认区回写芯片 + reg7/8=0x66
 *   随后 shadow→live 拷贝（0x100033D8→0x100033DC … 0x10003970→0x10003978），
 *   再按 parameter_pid_profile(0x100038C4，控制方式) 选择活动增益对 → 0x10003980/0x10003984
 *
 * param_sync_live_to_eeprom：live(0x10003988..) 与 EEPROM 缓存副本(0x1000398C..)
 *   逐参数比对，不一致即写回芯片对应寄存器（16 位分高低两字节）。
 *   寄存器号→语义对应关系见 MENU_PARAMETER_MAPPING.md / load_config 地址映射。
 * 导出：2026-08-20
 *
 * 交叉引用：
 *   · 61 组 live↔shadow↔EEPROM 同步全表 → docs/PROGRESS_2026-08-20.md §2
 *   · 双银行魔数 'U'(0x55) / 'f'(0x66) → docs/i2c_param_sync.md
 *   · 参数组 / 活动增益对 → docs/PLAN.md「关键符号速查」
 * ========================================================================== */

/* =============================================================================
 * src/06_param_system.c — 反编译模块 06（参数系统：live↔shadow↔EEPROM 同步）可编译副本
 * 目标B 阶段4：补 include。&parameter_address 地址伪影/符号语义按需修正。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include "inc/firmware_parameters.h"

/* 0x000025DC —— 上电装载配置（main 启动序列第 11 步 load_config()）
 *   · 银行 A：reg5/6 任一 == 'U' 则整组从 EEPROM 读入 live；
 *     否则以 0x10002EC4.. 默认值整组回写并置魔数 0x55
 *   · 银行 B：reg7/8 任一 == 'f' 则整组读入；否则回写默认 + 置魔数 0x66
 *   · shadow→live 拷贝 + 增益对选择
 * 局部（i2c_read_reg 逐字节读回）：
 *   rd_lo / rd_hi —— 读回字节临时值；16 位参数各占一个 EEPROM 字节，
 *     拼合成 *(uint32_t*)parameter_address = (rd_hi<<8)|rd_lo；单字节参数仅用 rd_lo
 *   dst_shadow —— 银行 B shadow→live 拷贝目的指针（0x1000390C） */
void load_config(void)
{
  volatile uint8_t *dst_shadow;
  uint8_t rd_hi;
  uint8_t rd_lo;

  rd_lo = 0;
  rd_hi = 0;
  i2c_read_reg(&rd_lo,5);
  i2c_read_reg(&rd_hi,6);
  if (((char)rd_lo == 'U') || ((char)rd_hi == 'U')) {
    i2c_read_reg(&rd_lo,10);
    *config_bank_a_control_mode = (char)rd_lo;
    i2c_read_reg(&rd_lo,0xb);
    i2c_read_reg(&rd_hi,0xc);
    *(volatile uint32_t *)config_bank_a_transformer_ratio = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd);
    i2c_read_reg(&rd_hi,0xe);
    *(volatile uint32_t *)config_bank_a_current_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xf);
    *config_bank_a_soft_start_time = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x10);
    i2c_read_reg(&rd_hi,0x11);
    *config_bank_a_voltage_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x12);
    *config_bank_a_soft_stop_time = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x13);
    i2c_read_reg(&rd_hi,0x14);
    *config_bank_a_phase_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x15);
    *config_bank_a_master_slave_offset = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x16);
    i2c_read_reg(&rd_hi,0x17);
    *config_bank_a_current_range = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x18);
    i2c_read_reg(&rd_hi,0x19);
    *config_bank_a_voltage_range = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x1a);
    *config_bank_a_control_method = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x1b);
    *config_bank_a_start_mode = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x1c);
    *config_bank_a_phase_calibration = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x1d);
    i2c_read_reg(&rd_hi,0x1e);
    *config_bank_a_edit_value = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x32);
    i2c_read_reg(&rd_hi,0x33);
    *config_bank_a_overvoltage_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x34);
    *config_bank_a_overvoltage_time = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x35);
    i2c_read_reg(&rd_hi,0x36);
    *config_bank_a_undervoltage_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x37);
    *config_bank_a_undervoltage_time = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x38);
    i2c_read_reg(&rd_hi,0x39);
    *config_bank_a_if_overload_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x3a);
    *config_bank_a_if_overload_time = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x3b);
    i2c_read_reg(&rd_hi,0x3c);
    *config_bank_a_ct_overload_limit = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x3d);
    *config_bank_a_ct_overload_time = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x3e);
    *config_bank_a_phase_loss_enable = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x3f);
    *config_bank_a_phase_balance = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5a);
    *config_bank_a_pid_profile = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5b);
    *config_bank_a_profile1_gain_a = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5c);
    *config_bank_a_profile1_gain_b = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5d);
    *config_bank_a_profile2_gain_a = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5e);
    *config_bank_a_profile2_gain_b = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x5f);
    *config_bank_a_profile3_gain_a = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x60);
    *config_bank_a_profile3_gain_b = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x61);
    *config_bank_a_profile4_gain_a = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x62);
    *config_bank_a_profile4_gain_b = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x6e);
    *config_bank_a_closed_loop_upper = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x6f);
    *config_bank_a_closed_loop_lower = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x70);
    *config_bank_a_closed_loop_gain_high = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x71);
    *config_bank_a_closed_loop_gain_mid = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x72);
    *config_bank_a_closed_loop_gain_low = (char)rd_lo;
    i2c_read_reg(&rd_lo,100);
    *config_bank_a_slave_address = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x65);
    i2c_read_reg(&rd_hi,0x66);
    *config_bank_a_baud_index = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x67);
    *config_bank_a_frame_mode = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x68);
    *config_bank_a_comm_detection = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x97);
    i2c_read_reg(&rd_hi,0x98);
    *config_bank_a_runtime_hours = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x99);
    i2c_read_reg(&rd_hi,0x9a);
    *config_bank_a_runtime_minutes = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x9b);
    i2c_read_reg(&rd_hi,0x9c);
    *config_bank_a_runtime_total = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
  }
  else {
    /* —— 银行 A 无魔数：用默认区整组回写 —— */
    i2c_write_reg(*config_bank_a_control_mode,10);
    i2c_write_reg(*config_bank_a_transformer_ratio >> 8,0xb);
    i2c_write_reg((char)*config_bank_a_transformer_ratio,0xc);
    i2c_write_reg(*config_bank_a_current_limit >> 8,0xd);
    i2c_write_reg((char)*config_bank_a_current_limit,0xe);
    i2c_write_reg(*default_soft_start_time,0xf);
    i2c_write_reg(*default_voltage_limit >> 8,0x10);
    i2c_write_reg((char)*default_voltage_limit,0x11);
    i2c_write_reg(*default_soft_stop_time,0x12);
    i2c_write_reg(*default_phase_limit >> 8,0x13);
    i2c_write_reg((char)*default_phase_limit,0x14);
    i2c_write_reg(*default_master_slave_offset,0x15);
    i2c_write_reg(*default_current_range >> 8,0x16);
    i2c_write_reg((char)*default_current_range,0x17);
    i2c_write_reg(*default_voltage_range >> 8,0x18);
    i2c_write_reg((char)*default_voltage_range,0x19);
    i2c_write_reg(*default_control_method,0x1a);
    i2c_write_reg(*default_start_mode,0x1b);
    i2c_write_reg(*default_phase_calibration,0x1c);
    i2c_write_reg(*default_edit_value >> 8,0x1d);
    i2c_write_reg((char)*default_edit_value,0x1e);
    i2c_write_reg(*default_overvoltage_limit >> 8,0x32);
    i2c_write_reg((char)*default_overvoltage_limit,0x33);
    i2c_write_reg(*default_overvoltage_time,0x34);
    i2c_write_reg(*default_undervoltage_limit >> 8,0x35);
    i2c_write_reg((char)*default_undervoltage_limit,0x36);
    i2c_write_reg(*default_undervoltage_time,0x37);
    i2c_write_reg(*default_if_overload_limit >> 8,0x38);
    i2c_write_reg((char)*default_if_overload_limit,0x39);
    i2c_write_reg(*default_if_overload_time,0x3a);
    i2c_write_reg(*default_ct_overload_limit >> 8,0x3b);
    i2c_write_reg((char)*default_ct_overload_limit,0x3c);
    i2c_write_reg(*default_ct_overload_time,0x3d);
    i2c_write_reg(*default_phase_loss_enable,0x3e);
    i2c_write_reg(*default_phase_balance,0x3f);
    i2c_write_reg(*default_pid_profile,0x5a);
    i2c_write_reg(*default_profile1_gain_a,0x5b);
    i2c_write_reg(*default_profile1_gain_b,0x5c);
    i2c_write_reg(*default_profile2_gain_a,0x5d);
    i2c_write_reg(*default_profile2_gain_b,0x5e);
    i2c_write_reg(*default_profile3_gain_a,0x5f);
    i2c_write_reg(*default_profile3_gain_b,0x60);
    i2c_write_reg(*default_profile4_gain_a,0x61);
    i2c_write_reg(*default_profile4_gain_b,0x62);
    i2c_write_reg(*default_closed_loop_upper,0x6e);
    i2c_write_reg(*default_closed_loop_lower,0x6f);
    i2c_write_reg(*default_closed_loop_gain_high,0x70);
    i2c_write_reg(*default_closed_loop_gain_mid,0x71);
    i2c_write_reg(*default_closed_loop_gain_low,0x72);
    i2c_write_reg(*default_slave_address,100);
    i2c_write_reg(*default_baud_index >> 8,0x65);
    i2c_write_reg((char)*default_baud_index,0x66);
    i2c_write_reg(*default_frame_mode,0x67);
    i2c_write_reg(*default_comm_detection,0x68);
    i2c_write_reg(*default_runtime_hours >> 8,0x97);
    i2c_write_reg((char)*default_runtime_hours,0x98);
    i2c_write_reg(*default_runtime_minutes >> 8,0x99);
    i2c_write_reg((char)*default_runtime_minutes,0x9a);
    i2c_write_reg(*default_runtime_total >> 8,0x9b);
    i2c_write_reg((char)*default_runtime_total,0x9c);
    i2c_write_reg(0x55,5);
    i2c_write_reg(0x55,6);
  }
  i2c_read_reg(&rd_lo,7);
  i2c_read_reg(&rd_hi,8);
  if (((char)rd_lo == 'f') || ((char)rd_hi == 'f')) {
    i2c_read_reg(&rd_lo,0x1f);
    *config_bank_b_emergency_stop = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x20);
    *config_bank_b_feedback_mode = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x21);
    *config_bank_b_input_mode = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x22);
    *config_bank_b_auxiliary_mode = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x23);
    *config_bank_b_output_phase = (char)rd_lo;
    i2c_read_reg(&rd_lo,0x24);
    i2c_read_reg(&rd_hi,0x25);
    *(volatile uint32_t *)config_bank_b_remote_enable = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0x26);
    i2c_read_reg(&rd_hi,0x27);
    *(volatile uint32_t *)config_bank_b_start_phase = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xc9);
    i2c_read_reg(&rd_hi,0xca);
    *(volatile uint32_t *)config_bank_b_frequency_adjust = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcb);
    i2c_read_reg(&rd_hi,0xcc);
    *(volatile uint32_t *)config_bank_b_current_calibration_a = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcd);
    i2c_read_reg(&rd_hi,0xce);
    *(volatile uint32_t *)config_bank_b_current_calibration_b = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xcf);
    i2c_read_reg(&rd_hi,0xd0);
    *(volatile uint32_t *)config_bank_b_current_calibration_c = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd1);
    i2c_read_reg(&rd_hi,0xd2);
    *(volatile uint32_t *)config_bank_b_field_calibration = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xd3);
    i2c_read_reg(&rd_hi,0xd4);
    *(volatile uint32_t *)config_bank_b_voltage_calibration = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xba);
    i2c_read_reg(&rd_hi,0xbb);
    *config_bank_b_access_status = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xbc);
    i2c_read_reg(&rd_hi,0xbd);
    *(volatile uint32_t *)config_bank_b_auth_status = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
    i2c_read_reg(&rd_lo,0xbe);
    i2c_read_reg(&rd_hi,0xbf);
    *(volatile uint32_t *)config_bank_b_state_code = (rd_hi & 0xff) + (rd_lo & 0xff) * 0x100;
  }
  else {
    /* —— 银行 B 无魔数：用默认区整组回写 —— */
    i2c_write_reg(*config_bank_b_emergency_stop,0x1f);
    i2c_write_reg(*config_bank_b_feedback_mode,0x20);
    i2c_write_reg(*config_bank_b_input_mode,0x21);
    i2c_write_reg(*config_bank_b_auxiliary_mode,0x22);
    i2c_write_reg(*config_bank_b_output_phase,0x23);
    i2c_write_reg(*config_bank_b_remote_enable >> 8,0x24);
    i2c_write_reg((char)*config_bank_b_remote_enable,0x25);
    i2c_write_reg(*config_bank_b_start_phase >> 8,0x26);
    i2c_write_reg((char)*config_bank_b_start_phase,0x27);
    i2c_write_reg(*config_bank_b_frequency_adjust >> 8,0xc9);
    i2c_write_reg((char)*config_bank_b_frequency_adjust,0xca);
    i2c_write_reg(*config_bank_b_current_calibration_a >> 8,0xcb);
    i2c_write_reg((char)*config_bank_b_current_calibration_a,0xcc);
    i2c_write_reg(*config_bank_b_current_calibration_b >> 8,0xcd);
    i2c_write_reg((char)*config_bank_b_current_calibration_b,0xce);
    i2c_write_reg(*config_bank_b_current_calibration_c >> 8,0xcf);
    i2c_write_reg((char)*config_bank_b_current_calibration_c,0xd0);
    i2c_write_reg(*config_bank_b_field_calibration >> 8,0xd1);
    i2c_write_reg((char)*config_bank_b_field_calibration,0xd2);
    i2c_write_reg(*config_bank_b_voltage_calibration >> 8,0xd3);
    i2c_write_reg((char)*config_bank_b_voltage_calibration,0xd4);
    i2c_write_reg(*config_bank_b_auth_status >> 8,0xbc);
    i2c_write_reg((char)*config_bank_b_auth_status,0xbd);
    i2c_write_reg(*config_bank_b_state_code >> 8,0xbe);
    i2c_write_reg((char)*config_bank_b_state_code,0xbf);
    i2c_write_reg(0x66,7);
    i2c_write_reg(0x66,8);
  }
  /* —— shadow→live 拷贝（银行 B 默认区 → 活动参数区）—— */
  *parameter_control_mode_ptr = *config_bank_a_control_mode;
  *parameter_transformer_ratio_ptr = *config_bank_a_transformer_ratio;
  *parameter_current_limit_ptr = *config_bank_a_current_limit;
  *parameter_soft_start_time_ptr = *config_bank_a_soft_start_time;
  *parameter_voltage_limit_ptr = *config_bank_a_voltage_limit;
  *parameter_soft_stop_time_ptr = *config_bank_a_soft_stop_time;
  *parameter_phase_limit_ptr = *config_bank_a_phase_limit;
  *parameter_master_slave_offset_ptr = *config_bank_a_master_slave_offset;
  *parameter_current_range_ptr = *config_bank_a_current_range;
  *parameter_voltage_range_ptr = *config_bank_a_voltage_range;
  *parameter_control_method_ptr = *config_bank_a_control_method;
  *parameter_start_mode_ptr = *config_bank_a_start_mode;
  *parameter_phase_calib_ptr = *config_bank_a_phase_calibration;
  *parameter_edit_value_ptr = *config_bank_a_edit_value;
  *parameter_emergency_stop_ptr = *config_bank_b_emergency_stop;      /* 保护参数首字节 */
  *parameter_auxiliary_mode_ptr = *config_bank_b_auxiliary_mode;
  *parameter_output_phase_ptr = *config_bank_b_output_phase;
  *parameter_feedback_mode_ptr = *config_bank_b_feedback_mode;
  *parameter_input_mode_ptr = *config_bank_b_input_mode;
  *parameter_current_calibration_a_ptr = *(volatile uint32_t *)config_bank_b_current_calibration_a;
  *parameter_current_calibration_b_ptr = *(volatile uint32_t *)config_bank_b_current_calibration_b;
  *parameter_current_calibration_c_ptr = *(volatile uint32_t *)config_bank_b_current_calibration_c;
  *parameter_field_calibration_ptr = *(volatile uint32_t *)config_bank_b_field_calibration;
  *parameter_voltage_calibration_ptr = *(volatile uint32_t *)config_bank_b_voltage_calibration;
  *parameter_remote_enable_ptr = *(volatile uint32_t *)config_bank_b_remote_enable;
  *parameter_start_phase_ptr = *(volatile uint32_t *)config_bank_b_start_phase;
  *parameter_overvoltage_limit_ptr = *config_bank_a_overvoltage_limit;
  *parameter_overvoltage_time_ptr = *config_bank_a_overvoltage_time;      /* PID / 通讯区 */
  *parameter_undervoltage_limit_ptr = *config_bank_a_undervoltage_limit;
  *parameter_undervoltage_time_ptr = *config_bank_a_undervoltage_time;
  *parameter_if_overload_limit_ptr = *config_bank_a_if_overload_limit;
  *parameter_if_overload_time_ptr = *config_bank_a_if_overload_time;
  *parameter_ct_overload_limit_ptr = *config_bank_a_ct_overload_limit;
  *parameter_ct_overload_time_ptr = *config_bank_a_ct_overload_time;
  *parameter_phase_loss_enable_ptr = *config_bank_a_phase_loss_enable;
  *parameter_phase_balance_ptr = *config_bank_a_phase_balance;
  *parameter_pid_profile_ptr = *config_bank_a_pid_profile;
  *parameter_profile1_gain_a_ptr = *config_bank_a_profile1_gain_a;
  *parameter_profile1_gain_b_ptr = *config_bank_a_profile1_gain_b;
  *parameter_profile2_gain_a_ptr = *config_bank_a_profile2_gain_a;
  *parameter_profile2_gain_b_ptr = *config_bank_a_profile2_gain_b;
  *parameter_profile3_gain_a_ptr = *config_bank_a_profile3_gain_a;
  *parameter_profile3_gain_b_ptr = *config_bank_a_profile3_gain_b;
  *parameter_profile4_gain_a_ptr = *config_bank_a_profile4_gain_a;
  *parameter_profile4_gain_b_ptr = *config_bank_a_profile4_gain_b;
  dst_shadow = closed_loop_threshold_upper_ptr;
  *closed_loop_threshold_upper_ptr = *eeprom_shadow_closed_loop_upper;
  *dst_shadow = *eeprom_shadow_closed_loop_upper;
  *closed_loop_gain_high_ptr = *eeprom_shadow_closed_loop_gain_high;
  *closed_loop_gain_mid_ptr = *eeprom_shadow_closed_loop_gain_mid;
  *closed_loop_gain_low_ptr = *eeprom_shadow_closed_loop_gain_low;
  *communication_slave_address_ptr = *eeprom_shadow_slave_address;
  *communication_baud_index_ptr = *eeprom_shadow_baud_index;
  *communication_frame_mode_ptr = *eeprom_shadow_frame_mode;
  *communication_detection_ptr = *eeprom_shadow_comm_detection;
  *parameter_runtime_hours_ptr = *eeprom_shadow_runtime_hours;
  *parameter_runtime_minutes_ptr = *eeprom_shadow_runtime_minutes;
  *parameter_runtime_total_ptr = *eeprom_shadow_runtime_total;
  *parameter_frequency_adjust_ptr = *eeprom_shadow_frequency_adjust;
  *parameter_access_status_ptr = *eeprom_shadow_access_status;
  *parameter_auth_status_ptr = *eeprom_shadow_auth_status;
  *parameter_state_code_ptr = *eeprom_shadow_state_code;
  /* —— 按控制方式选择活动增益对 → 0x10003980(gain_a 电压量程)/0x10003984(gain_b 电流量程) —— */
  if (*parameter_pid_profile_ptr == '\x01') {
    *parameter_active_gain_a_ptr = *parameter_profile1_gain_a_ptr;
    *parameter_active_gain_b_ptr = *parameter_profile1_gain_b_ptr;
  }
  if (*parameter_pid_profile_ptr == '\x02') {
    *parameter_active_gain_a_ptr = *parameter_profile2_gain_a_ptr;
    *parameter_active_gain_b_ptr = *parameter_profile2_gain_b_ptr;
  }
  if (*parameter_pid_profile_ptr == '\x03') {
    *parameter_active_gain_a_ptr = *parameter_profile3_gain_a_ptr;
    *parameter_active_gain_b_ptr = *parameter_profile3_gain_b_ptr;
  }
  if (*parameter_pid_profile_ptr == '\x04') {
    *parameter_active_gain_a_ptr = *parameter_profile4_gain_a_ptr;
    *parameter_active_gain_b_ptr = *parameter_profile4_gain_b_ptr;
  }
  return;
}


/* 0x000035F2 —— 参数 live→EEPROM 同步（认证通过后 param_sync_live_to_eeprom() 调用；
 *   主循环/菜单修改参数时也会触发）
 *   结构：对每个参数比对 live(0x10003988..) 与 EEPROM 缓存副本(0x1000398C..)，
 *   不等则更新缓存并 i2c_write_reg 写回芯片（16 位参数分高低字节：
 *     *reg >> 8 写高字节，低字节写下一寄存器 (char)*reg）。
 *   寄存器号序列（节选）：
 *     0x0A..0x1F 基本参数 | 0x20..0x3F | 0x5A..0x62 | 0x64..0x68 | 0x6E..0x72
 *     | 0x97..0x9C | 0xBA..0xBF 保护参数 | 0xC9..0xD4 PID/通讯参数
 *   局部：shadow = 当前参数的 EEPROM 镜像指针（16 位参数为 *(int*)shadow） */
void param_sync_live_to_eeprom(void)
{
  volatile uint8_t *shadow;

  shadow = eeprom_shadow_control_mode;
  if (*parameter_control_mode_ptr != *eeprom_shadow_control_mode) {
    *eeprom_shadow_control_mode = *parameter_control_mode_ptr;
    i2c_write_reg(*shadow,10);
  }
  if (*parameter_transformer_ratio_ptr != *(volatile int *)eeprom_shadow_transformer_ratio) {
    *(volatile int *)eeprom_shadow_transformer_ratio = *parameter_transformer_ratio_ptr;
    i2c_write_reg(*eeprom_shadow_transformer_ratio >> 8,0xb);
    i2c_write_reg((char)*eeprom_shadow_transformer_ratio,0xc);
  }
  if (*parameter_current_limit_ptr != *(volatile int *)eeprom_shadow_current_limit) {
    *(volatile int *)eeprom_shadow_current_limit = *parameter_current_limit_ptr;
    i2c_write_reg(*eeprom_shadow_current_limit >> 8,0xd);
    i2c_write_reg((char)*eeprom_shadow_current_limit,0xe);
  }
  shadow = eeprom_shadow_soft_start_time;
  if (*parameter_soft_start_time_ptr != *eeprom_shadow_soft_start_time) {
    *eeprom_shadow_soft_start_time = *parameter_soft_start_time_ptr;
    i2c_write_reg(*shadow,0xf);
  }
  if (*parameter_voltage_limit_ptr != *(volatile int *)eeprom_shadow_voltage_limit) {
    *(volatile int *)eeprom_shadow_voltage_limit = *parameter_voltage_limit_ptr;
    i2c_write_reg(*eeprom_shadow_voltage_limit >> 8,0x10);
    i2c_write_reg((char)*eeprom_shadow_voltage_limit,0x11);
  }
  shadow = eeprom_shadow_soft_stop_time;
  if (*parameter_soft_stop_time_ptr != *eeprom_shadow_soft_stop_time) {
    *eeprom_shadow_soft_stop_time = *parameter_soft_stop_time_ptr;
    i2c_write_reg(*shadow,0x12);
  }
  if (*parameter_phase_limit_ptr != *(volatile int *)eeprom_shadow_phase_limit) {
    *(volatile int *)eeprom_shadow_phase_limit = *parameter_phase_limit_ptr;
    i2c_write_reg(*eeprom_shadow_phase_limit >> 8,0x13);
    i2c_write_reg((char)*eeprom_shadow_phase_limit,0x14);
  }
  shadow = eeprom_shadow_master_slave_offset;
  if (*parameter_master_slave_offset_ptr != *eeprom_shadow_master_slave_offset) {
    *eeprom_shadow_master_slave_offset = *parameter_master_slave_offset_ptr;
    i2c_write_reg(*shadow,0x15);
  }
  if (*parameter_current_range_ptr != *(volatile int *)eeprom_shadow_current_range) {
    *(volatile int *)eeprom_shadow_current_range = *parameter_current_range_ptr;
    i2c_write_reg(*eeprom_shadow_current_range >> 8,0x16);
    i2c_write_reg((char)*eeprom_shadow_current_range,0x17);
  }
  if (*parameter_voltage_range_ptr != *(volatile int *)eeprom_shadow_voltage_range) {
    *(volatile int *)eeprom_shadow_voltage_range = *parameter_voltage_range_ptr;
    i2c_write_reg(*eeprom_shadow_voltage_range >> 8,0x18);
    i2c_write_reg((char)*eeprom_shadow_voltage_range,0x19);
  }
  shadow = eeprom_shadow_control_method;
  if (*parameter_control_method_ptr != *eeprom_shadow_control_method) {
    *eeprom_shadow_control_method = *parameter_control_method_ptr;
    i2c_write_reg(*shadow,0x1a);
  }
  shadow = eeprom_shadow_start_mode;
  if (*parameter_start_mode_ptr != *eeprom_shadow_start_mode) {
    *eeprom_shadow_start_mode = *parameter_start_mode_ptr;
    i2c_write_reg(*shadow,0x1b);
  }
  shadow = eeprom_shadow_phase_calib;
  if (*parameter_phase_calib_ptr != *eeprom_shadow_phase_calib) {
    *eeprom_shadow_phase_calib = *parameter_phase_calib_ptr;
    i2c_write_reg(*shadow,0x1c);
  }
  if (*parameter_edit_value_ptr != *(volatile int *)eeprom_shadow_edit_value) {
    *(volatile int *)eeprom_shadow_edit_value = *parameter_edit_value_ptr;
    i2c_write_reg(*eeprom_shadow_edit_value >> 8,0x1d);
    i2c_write_reg((char)*eeprom_shadow_edit_value,0x1e);
  }
  shadow = eeprom_shadow_emergency_stop;
  if (*parameter_emergency_stop_ptr != *eeprom_shadow_emergency_stop) {
    *eeprom_shadow_emergency_stop = *parameter_emergency_stop_ptr;
    i2c_write_reg(*shadow,0x1f);
  }
  shadow = eeprom_shadow_feedback_mode;
  if (*parameter_feedback_mode_ptr != *eeprom_shadow_feedback_mode) {
    *eeprom_shadow_feedback_mode = *parameter_feedback_mode_ptr;
    i2c_write_reg(*shadow,0x20);
  }
  shadow = eeprom_shadow_input_mode;
  if (*parameter_input_mode_ptr != *eeprom_shadow_input_mode) {
    *eeprom_shadow_input_mode = *parameter_input_mode_ptr;
    i2c_write_reg(*shadow,0x21);
  }
  shadow = eeprom_shadow_auxiliary_mode;
  if (*parameter_auxiliary_mode_ptr != *eeprom_shadow_auxiliary_mode) {
    *eeprom_shadow_auxiliary_mode = *parameter_auxiliary_mode_ptr;
    i2c_write_reg(*shadow,0x22);
  }
  shadow = eeprom_shadow_output_phase;
  if (*parameter_output_phase_ptr != *eeprom_shadow_output_phase) {
    *eeprom_shadow_output_phase = *parameter_output_phase_ptr;
    i2c_write_reg(*shadow,0x23);
  }
  if (*parameter_remote_enable_ptr != *(volatile int *)eeprom_shadow_remote_enable) {
    *(volatile int *)eeprom_shadow_remote_enable = *parameter_remote_enable_ptr;
    i2c_write_reg(*eeprom_shadow_remote_enable >> 8,0x24);
    i2c_write_reg((char)*eeprom_shadow_remote_enable,0x25);
  }
  if (*parameter_start_phase_ptr != *(volatile int *)eeprom_shadow_start_phase) {
    *(volatile int *)eeprom_shadow_start_phase = *parameter_start_phase_ptr;
    i2c_write_reg(*eeprom_shadow_start_phase >> 8,0x26);
    i2c_write_reg((char)*eeprom_shadow_start_phase,0x27);
  }
  if (*parameter_overvoltage_limit_ptr != *(volatile int *)eeprom_shadow_overvoltage_limit) {
    *(volatile int *)eeprom_shadow_overvoltage_limit = *parameter_overvoltage_limit_ptr;
    i2c_write_reg(*eeprom_shadow_overvoltage_limit >> 8,0x32);
    i2c_write_reg((char)*eeprom_shadow_overvoltage_limit,0x33);
  }
  shadow = eeprom_shadow_overvoltage_time;
  if (*parameter_overvoltage_time_ptr != *eeprom_shadow_overvoltage_time) {
    *eeprom_shadow_overvoltage_time = *parameter_overvoltage_time_ptr;
    i2c_write_reg(*shadow,0x34);
  }
  if (*parameter_undervoltage_limit_ptr != *(volatile int *)eeprom_shadow_undervoltage_limit) {
    *(volatile int *)eeprom_shadow_undervoltage_limit = *parameter_undervoltage_limit_ptr;
    i2c_write_reg(*eeprom_shadow_undervoltage_limit >> 8,0x35);
    i2c_write_reg((char)*eeprom_shadow_undervoltage_limit,0x36);
  }
  shadow = eeprom_shadow_undervoltage_time;
  if (*parameter_undervoltage_time_ptr != *eeprom_shadow_undervoltage_time) {
    *eeprom_shadow_undervoltage_time = *parameter_undervoltage_time_ptr;
    i2c_write_reg(*shadow,0x37);
  }
  if (*parameter_if_overload_limit_ptr != *(volatile int *)eeprom_shadow_if_overload_limit) {
    *(volatile int *)eeprom_shadow_if_overload_limit = *parameter_if_overload_limit_ptr;
    i2c_write_reg(*eeprom_shadow_if_overload_limit >> 8,0x38);
    i2c_write_reg((char)*eeprom_shadow_if_overload_limit,0x39);
  }
  shadow = eeprom_shadow_if_overload_time;
  if (*parameter_if_overload_time_ptr != *eeprom_shadow_if_overload_time) {
    *eeprom_shadow_if_overload_time = *parameter_if_overload_time_ptr;
    i2c_write_reg(*shadow,0x3a);
  }
  if (*parameter_ct_overload_limit_ptr != *(volatile int *)eeprom_shadow_ct_overload_limit) {
    *(volatile int *)eeprom_shadow_ct_overload_limit = *parameter_ct_overload_limit_ptr;
    i2c_write_reg(*eeprom_shadow_ct_overload_limit >> 8,0x3b);
    i2c_write_reg((char)*eeprom_shadow_ct_overload_limit,0x3c);
  }
  shadow = eeprom_shadow_ct_overload_time;
  if (*parameter_ct_overload_time_ptr != *eeprom_shadow_ct_overload_time) {
    *eeprom_shadow_ct_overload_time = *parameter_ct_overload_time_ptr;
    i2c_write_reg(*shadow,0x3d);
  }
  shadow = eeprom_shadow_phase_loss_enable;
  if (*parameter_phase_loss_enable_ptr != *eeprom_shadow_phase_loss_enable) {
    *eeprom_shadow_phase_loss_enable = *parameter_phase_loss_enable_ptr;
    i2c_write_reg(*shadow,0x3e);
  }
  shadow = eeprom_shadow_phase_balance;
  if (*parameter_phase_balance_ptr != *eeprom_shadow_phase_balance) {
    *eeprom_shadow_phase_balance = *parameter_phase_balance_ptr;
    i2c_write_reg(*shadow,0x3f);
  }
  shadow = eeprom_shadow_pid_profile;
  if (*parameter_pid_profile_ptr != *eeprom_shadow_pid_profile) {
    *eeprom_shadow_pid_profile = *parameter_pid_profile_ptr;
    i2c_write_reg(*shadow,0x5a);
  }
  shadow = eeprom_shadow_profile1_gain_a;
  if (*parameter_profile1_gain_a_ptr != *eeprom_shadow_profile1_gain_a) {
    *eeprom_shadow_profile1_gain_a = *parameter_profile1_gain_a_ptr;
    i2c_write_reg(*shadow,0x5b);
  }
  shadow = eeprom_shadow_profile1_gain_b;
  if (*parameter_profile1_gain_b_ptr != *eeprom_shadow_profile1_gain_b) {
    *eeprom_shadow_profile1_gain_b = *parameter_profile1_gain_b_ptr;
    i2c_write_reg(*shadow,0x5c);
  }
  shadow = eeprom_shadow_profile2_gain_a;
  if (*parameter_profile2_gain_a_ptr != *eeprom_shadow_profile2_gain_a) {
    *eeprom_shadow_profile2_gain_a = *parameter_profile2_gain_a_ptr;
    i2c_write_reg(*shadow,0x5d);
  }
  shadow = eeprom_shadow_profile2_gain_b;
  if (*parameter_profile2_gain_b_ptr != *eeprom_shadow_profile2_gain_b) {
    *eeprom_shadow_profile2_gain_b = *parameter_profile2_gain_b_ptr;
    i2c_write_reg(*shadow,0x5e);
  }
  shadow = eeprom_shadow_profile3_gain_a;
  if (*parameter_profile3_gain_a_ptr != *eeprom_shadow_profile3_gain_a) {
    *eeprom_shadow_profile3_gain_a = *parameter_profile3_gain_a_ptr;
    i2c_write_reg(*shadow,0x5f);
  }
  shadow = eeprom_shadow_profile3_gain_b;
  if (*parameter_profile3_gain_b_ptr != *eeprom_shadow_profile3_gain_b) {
    *eeprom_shadow_profile3_gain_b = *parameter_profile3_gain_b_ptr;
    i2c_write_reg(*shadow,0x60);
  }
  shadow = eeprom_shadow_profile4_gain_a;
  if (*parameter_profile4_gain_a_ptr != *eeprom_shadow_profile4_gain_a) {
    *eeprom_shadow_profile4_gain_a = *parameter_profile4_gain_a_ptr;
    i2c_write_reg(*shadow,0x61);
  }
  shadow = eeprom_shadow_profile4_gain_b;
  if (*parameter_profile4_gain_b_ptr != *eeprom_shadow_profile4_gain_b) {
    *eeprom_shadow_profile4_gain_b = *parameter_profile4_gain_b_ptr;
    i2c_write_reg(*shadow,0x62);
  }
  shadow = eeprom_shadow_closed_loop_upper;
  if (*closed_loop_threshold_upper_ptr != *eeprom_shadow_closed_loop_upper) {
    *eeprom_shadow_closed_loop_upper = *closed_loop_threshold_upper_ptr;
    i2c_write_reg(*shadow,0x6e);
  }
  shadow = eeprom_shadow_closed_loop_lower;
  if (*closed_loop_threshold_lower_ptr != *eeprom_shadow_closed_loop_lower) {
    *eeprom_shadow_closed_loop_lower = *closed_loop_threshold_lower_ptr;
    i2c_write_reg(*shadow,0x6f);
  }
  shadow = eeprom_shadow_closed_loop_gain_high;
  if (*closed_loop_gain_high_ptr != *eeprom_shadow_closed_loop_gain_high) {
    *eeprom_shadow_closed_loop_gain_high = *closed_loop_gain_high_ptr;
    i2c_write_reg(*shadow,0x70);
  }
  shadow = eeprom_shadow_closed_loop_gain_mid;
  if (*closed_loop_gain_mid_ptr != *eeprom_shadow_closed_loop_gain_mid) {
    *eeprom_shadow_closed_loop_gain_mid = *closed_loop_gain_mid_ptr;
    i2c_write_reg(*shadow,0x71);
  }
  shadow = eeprom_shadow_closed_loop_gain_low;
  if (*closed_loop_gain_low_ptr != *eeprom_shadow_closed_loop_gain_low) {
    *eeprom_shadow_closed_loop_gain_low = *closed_loop_gain_low_ptr;
    i2c_write_reg(*shadow,0x72);
  }
  shadow = eeprom_shadow_slave_address;
  if (*communication_slave_address_ptr != *eeprom_shadow_slave_address) {
    *eeprom_shadow_slave_address = *communication_slave_address_ptr;
    i2c_write_reg(*shadow,100);
  }
  if (*communication_baud_index_ptr != *(volatile int *)eeprom_shadow_baud_index) {
    *(volatile int *)eeprom_shadow_baud_index = *communication_baud_index_ptr;
    i2c_write_reg(*eeprom_shadow_baud_index >> 8,0x65);
    i2c_write_reg((char)*eeprom_shadow_baud_index,0x66);
  }
  shadow = eeprom_shadow_frame_mode;
  if (*communication_frame_mode_ptr != *eeprom_shadow_frame_mode) {
    *eeprom_shadow_frame_mode = *communication_frame_mode_ptr;
    i2c_write_reg(*shadow,0x67);
  }
  shadow = eeprom_shadow_comm_detection;
  if (*communication_detection_ptr != *eeprom_shadow_comm_detection) {
    *eeprom_shadow_comm_detection = *communication_detection_ptr;
    i2c_write_reg(*shadow,0x68);
  }
  if (*parameter_runtime_hours_ptr != *(volatile int *)eeprom_shadow_runtime_hours) {
    *(volatile int *)eeprom_shadow_runtime_hours = *parameter_runtime_hours_ptr;
    i2c_write_reg(*eeprom_shadow_runtime_hours >> 8,0x97);
    i2c_write_reg((char)*eeprom_shadow_runtime_hours,0x98);
  }
  if (*parameter_runtime_minutes_ptr != *(volatile int *)eeprom_shadow_runtime_minutes) {
    *(volatile int *)eeprom_shadow_runtime_minutes = *parameter_runtime_minutes_ptr;
    i2c_write_reg(*eeprom_shadow_runtime_minutes >> 8,0x99);
    i2c_write_reg((char)*eeprom_shadow_runtime_minutes,0x9a);
  }
  if (*parameter_runtime_total_ptr != *(volatile int *)eeprom_shadow_runtime_total) {
    *(volatile int *)eeprom_shadow_runtime_total = *parameter_runtime_total_ptr;
    i2c_write_reg(*eeprom_shadow_runtime_total >> 8,0x9b);
    i2c_write_reg((char)*eeprom_shadow_runtime_total,0x9c);
  }
  if (*parameter_frequency_adjust_ptr != *(volatile int *)eeprom_shadow_frequency_adjust) {
    *(volatile int *)eeprom_shadow_frequency_adjust = *parameter_frequency_adjust_ptr;
    i2c_write_reg(*eeprom_shadow_frequency_adjust >> 8,0xc9);
    i2c_write_reg((char)*eeprom_shadow_frequency_adjust,0xca);
  }
  if (*parameter_current_calibration_a_ptr != *(volatile int *)eeprom_shadow_current_calibration_a) {
    *(volatile int *)eeprom_shadow_current_calibration_a = *parameter_current_calibration_a_ptr;
    i2c_write_reg(*eeprom_shadow_current_calibration_a >> 8,0xcb);
    i2c_write_reg((char)*eeprom_shadow_current_calibration_a,0xcc);
  }
  if (*parameter_current_calibration_b_ptr != *(volatile int *)eeprom_shadow_current_calibration_b) {
    *(volatile int *)eeprom_shadow_current_calibration_b = *parameter_current_calibration_b_ptr;
    i2c_write_reg(*eeprom_shadow_current_calibration_b >> 8,0xcd);
    i2c_write_reg((char)*eeprom_shadow_current_calibration_b,0xce);
  }
  if (*parameter_current_calibration_c_ptr != *(volatile int *)eeprom_shadow_current_calibration_c) {
    *(volatile int *)eeprom_shadow_current_calibration_c = *parameter_current_calibration_c_ptr;
    i2c_write_reg(*eeprom_shadow_current_calibration_c >> 8,0xcf);
    i2c_write_reg((char)*eeprom_shadow_current_calibration_c,0xd0);
  }
  if (*parameter_field_calibration_ptr != *(volatile int *)eeprom_shadow_field_calibration) {
    *(volatile int *)eeprom_shadow_field_calibration = *parameter_field_calibration_ptr;
    i2c_write_reg(*eeprom_shadow_field_calibration >> 8,0xd1);
    i2c_write_reg((char)*eeprom_shadow_field_calibration,0xd2);
  }
  if (*parameter_voltage_calibration_ptr != *(volatile int *)eeprom_shadow_voltage_calibration) {
    *(volatile int *)eeprom_shadow_voltage_calibration = *parameter_voltage_calibration_ptr;
    i2c_write_reg(*eeprom_shadow_voltage_calibration >> 8,0xd3);
    i2c_write_reg((char)*eeprom_shadow_voltage_calibration,0xd4);
  }
  if (*parameter_access_status_ptr != *(volatile int *)eeprom_shadow_access_status) {
    *(volatile int *)eeprom_shadow_access_status = *parameter_access_status_ptr;
    i2c_write_reg(*eeprom_shadow_access_status >> 8,0xba);
    i2c_write_reg((char)*eeprom_shadow_access_status,0xbb);
  }
  if (*parameter_auth_status_ptr != *(volatile int *)eeprom_shadow_auth_status) {
    *(volatile int *)eeprom_shadow_auth_status = *parameter_auth_status_ptr;
    i2c_write_reg(*eeprom_shadow_auth_status >> 8,0xbc);
    i2c_write_reg((char)*eeprom_shadow_auth_status,0xbd);
  }
  if (*parameter_state_code_ptr != *(volatile int *)eeprom_shadow_state_code) {
    *(volatile int *)eeprom_shadow_state_code = *parameter_state_code_ptr;
    i2c_write_reg(*eeprom_shadow_state_code >> 8,0xbe);
    i2c_write_reg((char)*eeprom_shadow_state_code,0xbf);
  }
  return;
}
