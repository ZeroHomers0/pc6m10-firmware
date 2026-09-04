/* =============================================================================
 * 08_modbus_dispatch.c — modbus_dispatch(0xB642) C 级还原
 * 目标B W1a：替换 firmware/stub.c 占位。依据 evidence/reverse/disassembly/08_modbus_dispatch_asm.txt
 *   （5161 条指令）逐段还原，数据地址全部以反汇编字面量 SRAM 值为准。
 *
 * 函数：0x0000B642-0xE573（Modbus RTU 从站帧解析与分发主处理）
 * 调用点：main() 主循环 modbus_dispatch(0)
 * 调用关系：crc16(0xAF64)、uart3_tx_byte(0xAE0C)、param_sync_live_to_eeprom(0x35F2)、
 *   i2c_write_reg(0x1E88)×2、out_relay_p020(0x10588)×2、modbus_write_multi(0xB2E0)、
 *   modbus_read_reg(0xAF94)
 *
 * 关键 SRAM（真实地址，反汇编字面量确认）：
 *   0x100022A4 接收帧缓冲       0x1000236C 发送缓冲(TX)
 *   0x100016FF 本站从站地址     0x10001790 接收/发送状态（0 空闲/1 首字节/5 完整帧）
 *   0x10001792 接收帧长度       0x100017F0 写寄存器 16 位值缓存(word)
 *   0x100017F4 CRC 计算缓存     0x10002754 帧末 CRC 暂存
 *   0x2009C000 FIO 池(+0x3C modbus_error_indicator_clear_register)   0x4009C000 UART3(+0x04 IER)
 *   0x1000178C/0x100017B8 计数
 *   0x100017A4/0x10001798/0x1000179C/0x100017A0/0x100017A8/0x100017B0/0x100017B4
 *     —— 0x10 写多/0x03 读区工作变量
 *
 * 写分支（0x06 单写）53 个：寄存器号 0x1001-0x103E（缺 0x101E-0x1020、0x1029-0x102E、
 * 0x103F；0x101E/0x1F/0x20/0x29-0x2E 无写分支，0x103F 不存在）。每分支 =
 *   值缓存 store → 范围校验 → 参数槽 store（word 或 uint8_t）→ param_sync →
 *   8 字节响应 [地址,0x06,0x10,register_address,val_hi,val_lo,CRC16]。
 *   特殊分支：0x1001 控制方式(uint8_t 0x10001634,值<3)、0x1017 控制方式+增益组复制、
 *   0x1018/0x1019 第4组增益(控制方式==4 即时同步)、0x1021-0x1024 仅允许写 0、
 *   0x1025/0x1026 无 param_sync 特殊命令、0x102F 从站地址、0x103D 远程使能。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include "inc/firmware_parameters.h"

/* =============================================================================
 * modbus_dispatch(0x0000B642)
 * 流程（对照 evidence/reverse/disassembly/08_modbus_dispatch_asm.txt）：
 *   1) 帧态门控：状态==1 → 只清计数返回；==5 → 继续；否则返回
 *   2) 从站地址匹配：帧[0]==本站地址？不匹配 → modbus_error_indicator_clear_register P4.29 + 清状态 + IER + 返回
 *   3) 功能码分发：非 0x03/0x06/0x10 → 异常响应 [地址,func|0x80,0x01]
 *   4) CRC 校验：crc16(帧, len-2) vs 帧末两字节；不匹配 → 异常 [..,0x04]
 *   5) 0x06 写单：值=帧[4..5] → 值缓存 → 53 分支
 *   6) 0x10 写多：modbus_write_multi 循环 → 响应 [地址,0x10,0x10,起始reg,数量,CRC16]
 *   7) 0x03 读：modbus_read_reg 循环 → 响应 [地址,0x03,字节数,数据..,CRC16]
 *   8) 兜底帧尾：modbus_error_indicator_clear_register P4.29 + 清状态 + UART3 IER
 *   入口参数 arg：保留未用（主循环调用传 0；帧数据全部读自全局 SRAM，见 #define 数据指针）。
 * ========================================================================== */
void modbus_dispatch(int arg)
{
  volatile uint8_t *const frame_state = modbus_frame_state_ptr;
  volatile uint8_t *const frame_length = modbus_frame_length_ptr;
  volatile uint8_t *const slave_address = communication_slave_address_ptr;
  volatile uint32_t *const write_value_cache = modbus_write_value_cache_ptr;
  uint8_t *const rx_frame = modbus_rx_frame_buffer;
  uint8_t *const tx_frame = modbus_tx_frame_buffer;
  uint16_t frame_crc;
  uint32_t write_value;
  uint32_t register_address, register_count;
  uint32_t index;
  (void)arg;

  /* 1) 帧态门控 */
  if (*frame_state == 1) {
    *uart3_global_tick_ptr = 0;
    return;
  }
  if (*frame_state != 5) {
    return;
  }
  *uart3_rx_timeout_ptr = 0;
  *uart3_global_tick_ptr = 0;

  /* 2) 从站地址匹配 */
  if (rx_frame[0] != *slave_address) {
    fio_clear(FIO1, 0x20000000);
    *frame_state = 0;
    UART3->IER = UART3->IER | 1;
    return;
  }

  /* 3) 功能码分发（非法功能码） */
  if (rx_frame[1] != 0x03 && rx_frame[1] != 0x06 && rx_frame[1] != 0x10) {
    tx_frame[0] = *slave_address; tx_frame[1] = rx_frame[1] | 0x80; tx_frame[2] = 0x01;
    frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
    uart3_tx_byte(5);
    return;
  }

  /* 4) CRC 校验（帧末两字节 vs 计算值） */
  write_value = (uint32_t)*frame_length;
  frame_crc = crc16((uint8_t *)rx_frame, write_value - 2);
  if ((uint8_t)(frame_crc & 0xff) != rx_frame[write_value - 2] || (uint8_t)(frame_crc >> 8) != rx_frame[write_value - 1]) {
    tx_frame[0] = *slave_address; tx_frame[1] = rx_frame[1] | 0x80; tx_frame[2] = 0x04;
    frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
    uart3_tx_byte(5);
    return;
  }

  /* ================= 5) 0x06 写单寄存器（53 分支） ================= */
  if (rx_frame[1] == 0x06 && rx_frame[2] == 0x10) {
    write_value = ((uint32_t)rx_frame[4] << 8) | (uint32_t)rx_frame[5];   /* 原 BIN 0xB798: rx_frame[4]<<8|rx_frame[5] 大端 */
    *write_value_cache = write_value;
    switch (rx_frame[3]) {
    case 0x01:                    /* 0x1001 控制方式（uint8_t 参数，值<3） */
      if (write_value >= 3) { goto invalid_register_value; }
      *parameter_control_mode_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x02:                    /* 0x1002 值 10..6000 → word 0x1000163C */
      if (write_value > 6000 || write_value <= 9) { goto invalid_register_value; }
      *parameter_voltage_range_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x03:                    /* 0x1003 → word 0x10001638 */
      if (write_value > 6000 || write_value <= 9) { goto invalid_register_value; }
      *parameter_current_range_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x04:                    /* 0x1004 → word 0x10001640 */
      if (write_value > 6000 || write_value <= 9) { goto invalid_register_value; }
      *parameter_transformer_ratio_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x05:                    /* 0x1005 → word 0x10001648 */
      if (write_value > 6000 || write_value <= 9) { goto invalid_register_value; }
      *parameter_voltage_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x06:                    /* 0x1006 → word 0x10001644 */
      if (write_value > 6000 || write_value <= 9) { goto invalid_register_value; }
      *parameter_current_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x07:                    /* 0x1007 uint8_t 值<201 → 0x1000164C */
      if (write_value >= 201) { goto invalid_register_value; }
      *parameter_soft_start_time_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x08:                    /* 0x1008 uint8_t 值<201 → 0x1000164D */
      if (write_value >= 201) { goto invalid_register_value; }
      *parameter_soft_stop_time_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x09:                    /* 0x1009 word 值<181 → 0x10001650 */
      if (write_value >= 181) { goto invalid_register_value; }
      *parameter_phase_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x0A:                    /* 0x100A uint8_t 40..160 → 0x10001654 */
      if (write_value >= 161 || write_value <= 39) { goto invalid_register_value; }
      *parameter_master_slave_offset_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x0B:                    /* 0x100B uint8_t 值<3 → 0x10001655 */
      if (write_value >= 3) { goto invalid_register_value; }
      *parameter_control_method_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x0C:                    /* 0x100C uint8_t 值<2 → 0x10001656 */
      if (write_value >= 2) { goto invalid_register_value; }
      *parameter_start_mode_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x0D:                    /* 0x100D word 值<6001 → 0x100016C0 */
      if (write_value >= 6001) { goto invalid_register_value; }
      *parameter_overvoltage_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x0E:                    /* 0x100E uint8_t 值<201 → 0x100016C4 */
      if (write_value >= 201) { goto invalid_register_value; }
      *parameter_overvoltage_time_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x0F:                    /* 0x100F word 值<6001 → 0x100016C8 */
      if (write_value >= 6001) { goto invalid_register_value; }
      *parameter_undervoltage_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x10:                    /* 0x1010 uint8_t 值<201 → 0x100016CC */
      if (write_value >= 201) { goto invalid_register_value; }
      *parameter_undervoltage_time_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x11:                    /* 0x1011 word 值<6001 → 0x100016D0 */
      if (write_value >= 6001) { goto invalid_register_value; }
      *parameter_if_overload_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x12:                    /* 0x1012 uint8_t 值<201 → 0x100016D4 */
      if (write_value >= 201) { goto invalid_register_value; }
      *parameter_if_overload_time_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x13:                    /* 0x1013 word 值<6001 → 0x100016D8 */
      if (write_value >= 6001) { goto invalid_register_value; }
      *parameter_ct_overload_limit_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x14:                    /* 0x1014 uint8_t 值<201 → 0x100016DC */
      if (write_value >= 201) { goto invalid_register_value; }
      *parameter_ct_overload_time_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x15:                    /* 0x1015 uint8_t 值<2 → 0x100016DD */
      if (write_value >= 2) { goto invalid_register_value; }
      *parameter_phase_loss_enable_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x16:                    /* 0x1016 uint8_t 值<61 → 0x100016DE */
      if (write_value >= 61) { goto invalid_register_value; }
      *parameter_phase_balance_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x17:                    /* 0x1017 控制方式（1..4）+ 对应增益组复制到活动槽 */
      if (write_value >= 5 || write_value == 0) { goto invalid_register_value; }
      *parameter_pid_profile_ptr = (uint8_t)write_value;
      if (write_value == 1) { *parameter_active_gain_a_ptr = *parameter_profile1_gain_a_ptr;
                    *parameter_active_gain_b_ptr = *parameter_profile1_gain_b_ptr; }
      if (write_value == 2) { *parameter_active_gain_a_ptr = *parameter_profile2_gain_a_ptr;
                    *parameter_active_gain_b_ptr = *parameter_profile2_gain_b_ptr; }
      if (write_value == 3) { *parameter_active_gain_a_ptr = *parameter_profile3_gain_a_ptr;
                    *parameter_active_gain_b_ptr = *parameter_profile3_gain_b_ptr; }
      if (write_value == 4) { *parameter_active_gain_a_ptr = *parameter_profile4_gain_a_ptr;
                    *parameter_active_gain_b_ptr = *parameter_profile4_gain_b_ptr; }
      param_sync_live_to_eeprom();
      break;
    case 0x18:                    /* 0x1018 第4组增益1（1..128）；控制方式==4 即时同步 */
      if (write_value >= 129 || write_value == 0) { goto invalid_register_value; }
      *parameter_profile4_gain_a_ptr = (uint8_t)write_value;
      if (*parameter_pid_profile_ptr == 4) {
        *parameter_active_gain_a_ptr = *parameter_profile4_gain_a_ptr;
      }
      param_sync_live_to_eeprom();
      break;
    case 0x19:                    /* 0x1019 第4组增益2（1..128）；控制方式==4 即时同步 */
      if (write_value >= 129 || write_value == 0) { goto invalid_register_value; }
      *parameter_profile4_gain_b_ptr = (uint8_t)write_value;
      if (*parameter_pid_profile_ptr == 4) {
        *parameter_active_gain_b_ptr = *parameter_profile4_gain_b_ptr;
      }
      param_sync_live_to_eeprom();
      break;
    case 0x1A:                    /* 0x101A uint8_t 2..199 → 0x10001694（cmp 0xc8;bcs 上限 0xC8） */
      if (write_value >= 200 || write_value <= 1) { goto invalid_register_value; }
      *parameter_phase_calib_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x1B:                    /* 0x101B word 值<4501 → 0x10001698 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_current_calibration_a_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x1C:                    /* 0x101C word 值<4501 → 0x100016A0 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_current_calibration_b_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x1D:                    /* 0x101D word 值<4501 → 0x100016A8 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_current_calibration_c_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x21:                    /* 0x1021 仅允许写 0 → word 0x100015FC */
      if (write_value != 0) { goto invalid_register_value; }
      *runtime_current_minute_ptr = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x22:                    /* 0x1022 仅允许写 0 → word 0x100015F8 */
      if (write_value != 0) { goto invalid_register_value; }
      *runtime_current_hour_ptr = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x23:                    /* 0x1023 仅允许写 0 → word 0x10001608 */
      if (write_value != 0) { goto invalid_register_value; }
      *runtime_total_minute_ptr = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x24:                    /* 0x1024 仅允许写 0 → word 0x10001604 */
      if (write_value != 0) { goto invalid_register_value; }
      *runtime_total_hour_ptr = 0;
      param_sync_live_to_eeprom();
      break;
    case 0x25:                    /* 0x1025 特殊：仅允许写 0 → word 0x10001624（无 param_sync） */
      if (write_value != 0) { goto invalid_register_value; }
      *output_fault_flags_ptr = 0;
      break;
    case 0x26:                    /* 0x1026 特殊：仅允许写 0 → i2c_write_reg(0,5/6)（无 param_sync） */
      if (write_value != 0) { goto invalid_register_value; }
      i2c_write_reg(0, 5);
      i2c_write_reg(0, 6);
      break;
    case 0x27:                    /* 0x1027 uint8_t 值<2 → 0x10001785 */
      if (write_value >= 2) { goto invalid_register_value; }
      *run_stop_state_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x28:                    /* 0x1028 word 无范围 → 0x10001788 */
      *frequency_reference_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x2F:                    /* 0x102F 从站地址（1..247）→ uint8_t 0x100016FF（即本站地址） */
      if (write_value >= 248 || write_value == 0) { goto invalid_register_value; }
      *slave_address = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x30:                    /* 0x1030 word 值<8 → 0x10001700 */
      if (write_value >= 8) { goto invalid_register_value; }
      *communication_baud_index_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x31:                    /* 0x1031 uint8_t 值<4 → 0x10001704 */
      if (write_value >= 4) { goto invalid_register_value; }
      *communication_frame_mode_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x32:                    /* 0x1032 uint8_t 值<2 → 0x10001705 */
      if (write_value >= 2) { goto invalid_register_value; }
      *communication_detection_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x33:                    /* 0x1033 word 值<4501 → 0x10001698（同 0x101B 槽） */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_current_calibration_a_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x34:                    /* 0x1034 → 0x100016A0 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_current_calibration_b_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x35:                    /* 0x1035 → 0x100016A8 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_current_calibration_c_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x36:                    /* 0x1036 → 0x100016B0 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_field_calibration_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x37:                    /* 0x1037 → 0x100016B8 */
      if (write_value >= 4501) { goto invalid_register_value; }
      *parameter_voltage_calibration_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x38:                    /* 0x1038 uint8_t 值<3 → 0x10001657 */
      if (write_value >= 3) { goto invalid_register_value; }
      *parameter_emergency_stop_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x39:                    /* 0x1039 uint8_t 值<3 → 0x10001658 */
      if (write_value >= 3) { goto invalid_register_value; }
      *parameter_auxiliary_mode_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x3A:                    /* 0x103A uint8_t 值<2 → 0x10001659 */
      if (write_value >= 2) { goto invalid_register_value; }
      *parameter_feedback_mode_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x3B:                    /* 0x103B uint8_t 值<2 → 0x1000165A */
      if (write_value >= 2) { goto invalid_register_value; }
      *parameter_input_mode_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x3C:                    /* 0x103C uint8_t 值<2 → 0x1000165B */
      if (write_value >= 2) { goto invalid_register_value; }
      *parameter_output_phase_ptr = (uint8_t)write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x3D:                    /* 0x103D 远程使能：值<101；非 0 → out_relay_p020(1)，0 → (0) */
      if (write_value >= 101) { goto invalid_register_value; }
      if (write_value != 0) { out_relay_p020(1); }
      else { out_relay_p020(0); }
      *parameter_remote_enable_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    case 0x3E:                    /* 0x103E word 值<181 → 0x10001660 */
      if (write_value >= 181) { goto invalid_register_value; }
      *parameter_start_phase_ptr = write_value;
      param_sync_live_to_eeprom();
      break;
    default:
      goto invalid_register_address;           /* 原 BIN 0xE16A：未匹配寄存器 → [地址,0x86,0x02] */
    }
    /* 正常写响应：[地址,0x06,0x10,register_address,val_hi,val_lo,CRC16] */
    tx_frame[0] = *slave_address; tx_frame[1] = 0x06; tx_frame[2] = 0x10; tx_frame[3] = rx_frame[3];
    tx_frame[4] = (uint8_t)(write_value >> 8); tx_frame[5] = (uint8_t)(write_value & 0xff);
    frame_crc = crc16((uint8_t *)tx_frame, 6); tx_frame[6] = frame_crc & 0xff; tx_frame[7] = frame_crc >> 8;
    uart3_tx_byte(8);
    return;

  invalid_register_address:                    /* 非法寄存器地址 → 异常响应 [地址,0x86,0x02] */
    tx_frame[0] = *slave_address; tx_frame[1] = 0x86; tx_frame[2] = 0x02;
    frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
    uart3_tx_byte(5);
    return;

  invalid_register_value:                      /* 值越界 → 异常响应 [地址,0x86,0x03] */
    tx_frame[0] = *slave_address; tx_frame[1] = 0x86; tx_frame[2] = 0x03;
    frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
    uart3_tx_byte(5);
    return;
  }

  /* ================= 6) 0x10 写多寄存器 ================= */
  if (rx_frame[1] == 0x10 && rx_frame[2] == 0x10) {
    register_address = (uint32_t)rx_frame[3];
    register_count = ((uint32_t)rx_frame[4] << 8) | (uint32_t)rx_frame[5];   /* 原 BIN 0xE202: rx_frame[4]<<8|rx_frame[5] 大端 */
    if (register_address == 0 || register_address > 0x3E) {
      tx_frame[0] = *slave_address; tx_frame[1] = 0x90; tx_frame[2] = 0x02;      /* 非法地址 */
      frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    if (register_count == 0 || register_count > 0x3E || rx_frame[6] != (register_count << 1)) {
      tx_frame[0] = *slave_address; tx_frame[1] = 0x90; tx_frame[2] = 0x03;      /* 非法数量/字节数 */
      frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    for (index = 0; index < register_count; index++) {
      write_value = ((uint32_t)rx_frame[7 + index * 2] << 8) | (uint32_t)rx_frame[8 + index * 2];  /* 原 BIN 0xE2C2 大端 */
      *modbus_register_value_ptr = write_value;
      modbus_write_multi((uint32_t *)modbus_register_value_ptr, register_address - 1 + index);  /* 内部表 0 基（reg1→case0） */
    }
    /* 响应：[地址,0x10,0x10,起始reg,数量_hi,数量_lo,CRC16] */
    tx_frame[0] = *slave_address; tx_frame[1] = 0x10; tx_frame[2] = 0x10; tx_frame[3] = (uint8_t)register_address;
    tx_frame[4] = (uint8_t)(register_count >> 8); tx_frame[5] = (uint8_t)(register_count & 0xff);
    frame_crc = crc16((uint8_t *)tx_frame, 6); tx_frame[6] = frame_crc & 0xff; tx_frame[7] = frame_crc >> 8;
    uart3_tx_byte(8);
    return;
  }

  /* ================= 7) 0x03 读保持寄存器 ================= */
  if (rx_frame[1] == 0x03 && rx_frame[2] == 0x10) {
    register_address = (uint32_t)rx_frame[3];
    register_count = ((uint32_t)rx_frame[4] << 8) | (uint32_t)rx_frame[5];   /* 原 BIN 0xE408: rx_frame[4]<<8|rx_frame[5] 大端 */
    if (register_address == 0 || register_address > 0x3F) {
      tx_frame[0] = *slave_address; tx_frame[1] = 0x83; tx_frame[2] = 0x02;
      frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    if (register_count == 0 || (register_address - 1 + register_count) > 0x3F) {   /* asm: register_count+(register_address-1)>0x3F → 越界 */
      tx_frame[0] = *slave_address; tx_frame[1] = 0x83; tx_frame[2] = 0x03;
      frame_crc = crc16((uint8_t *)tx_frame, 3); tx_frame[3] = frame_crc & 0xff; tx_frame[4] = frame_crc >> 8;
      uart3_tx_byte(5);
      return;
    }
    tx_frame[0] = *slave_address; tx_frame[1] = 0x03; tx_frame[2] = (uint8_t)(register_count << 1);
    for (index = 0; index < register_count; index++) {
      /* modbus_read_reg 把数据写入 *out_val(0x100017A4)，返回值恒为 0。
       * 原机码 bl 后用 ldrh r0,[rx] 回读 *out_val 取数据，不用返回值。 */
      modbus_read_reg((uint32_t *)modbus_register_value_ptr, register_address - 1 + index);  /* 内部表 0 基 */
      write_value = (uint32_t)*(uint16_t *)modbus_register_value_ptr;
      tx_frame[3 + index * 2] = (uint8_t)(write_value >> 8);
      tx_frame[4 + index * 2] = (uint8_t)(write_value & 0xff);
    }
    frame_crc = crc16((uint8_t *)tx_frame, 3 + register_count * 2);
    tx_frame[3 + register_count * 2] = frame_crc & 0xff;
    tx_frame[4 + register_count * 2] = frame_crc >> 8;
    uart3_tx_byte(5 + register_count * 2);
    return;
  }

                                  /* 8) 兜底帧尾：结构不符（帧[2]!=0x10 等） */
  fio_clear(FIO1, 0x20000000);
  *frame_state = 0;
  UART3->IER = UART3->IER | 1;
  return;
}
