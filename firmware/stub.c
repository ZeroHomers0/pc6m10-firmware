/* =============================================================================
 * stub.c — 反编译伪代码函数的编译占位（firmware/stub.c）
 *
 * 反编译源码中两个超大函数（state_machine 0x458C / modbus_dispatch 0xB642）
 * 无合法 C 函数体（见 08_modbus_dispatch.c 流程还原注释），gcc 直接编译会失败。
 * 二者现已由 src 侧完整还原：
 *   modbus_dispatch(0xB642) → src/08_modbus_dispatch.c（W1a 已完成）
 *   state_machine(0x458C)   → src/07_state_machine.c（W1b 进行中）
 * 故本文件不再提供二者的占位定义，只保留无法纳入 src 联编的辅助子例程。
 *
 * freq_adjust_sync（0xAB48，07 联锁屏频率调节）有完整可编译实现，从根目录
 *   07_state_machine.c 迁入（非 stub），保证联锁分支行为保真。
 * ========================================================================== */
#include <stdint.h>
#include "inc/types.h"

#include "inc/firmware_api.h"
#include "inc/firmware_state.h"

/* 08 UART3 RX 组帧子例程（0xAED0..0xAF06）。 */
void uart3_receive_frame(void)
{
  volatile uint8_t *state = uart3_rx_state_ptr;
  volatile uint8_t *gap = uart3_rx_gap_ptr;
  volatile uint8_t *rx_idx = uart3_rx_index_ptr;
  volatile uint8_t *rx_buf = uart3_rx_buffer;

  if (*state == 0) {
    *rx_idx = 0;
    *state = 1;
  }
  if (*state == 1) {
    *gap = 0;
    rx_buf[*rx_idx] = *uart3_peripheral_base;
    *rx_idx = (uint8_t)(*rx_idx + 1);
  }
}

/* 保留反汇编地址命名作为链接别名；新代码统一使用 uart3_receive_frame。 */
void func_0x0000aed0(void) __attribute__((alias("uart3_receive_frame")));

/* 0x0000AB48 —— 联锁/错误屏频率调节 + EEPROM 写回（07 迁入，完整实现） */
void freq_adjust_sync(int key_code)
{
  volatile uint32_t *frequency_value;
  int debounce_event;

  debounce_event = debounce_p09();
  if ((debounce_event == 1) && (*frequency_adjust_value_ptr != *frequency_adjust_shadow_ptr)) {
    *frequency_adjust_shadow_ptr = *frequency_adjust_value_ptr;
    i2c_write_reg((uint16_t)*frequency_adjust_shadow_ptr >> 8,0xc9);
    i2c_write_reg((char)*frequency_adjust_shadow_ptr,0xca);
  }
  frequency_value = frequency_adjust_value_ptr;
  if ((key_code == 2) || (key_code == 0x16)) {
    *frequency_adjust_value_ptr = *frequency_adjust_value_ptr + 1;
    if (0x2b0 < *frequency_value) {
      *frequency_value = 0x2b0;
    }
    disp_offset(*frequency_adjust_value_ptr,2,7,1);
  }
  if ((key_code == 3) || (key_code == 0x21)) {
    if (*frequency_adjust_value_ptr < 0x45) {
      *frequency_adjust_value_ptr = 0x45;
    }
    *frequency_adjust_value_ptr = *frequency_adjust_value_ptr - 1;
    disp_offset(*frequency_adjust_value_ptr,2,7,1);
  }
  if (key_code == 5) {
    *parameter_output_mode_ptr = 1;
    disp_string((int)0x47F0,3,0xb,0);    /* 0x47F0：高档字符串（&频率高档字符串 内容） */
  }
  if (key_code == 6) {
    *parameter_output_mode_ptr = 0;
    disp_string((int)0xAC1C,3,0xb,0);    /* 0xAC1C：低档字符串（&频率低档字符串 即地址） */
  }
  return;
}
