/* =============================================================================
 * 06_frequency_adjust.c — 联锁页频率微调与 EEPROM 同步
 * ========================================================================== */
#include "inc/types.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include "inc/firmware_parameters.h"
#include "inc/firmware_display_strings.h"

void freq_adjust_sync(KeyCode key_code)
{
  volatile uint32_t *frequency_value = frequency_adjust_value_ptr;
  int debounce_event;

  debounce_event = debounce_p09();
  if ((debounce_event == 1) && (*frequency_adjust_value_ptr != *frequency_adjust_shadow_ptr)) {
    *frequency_adjust_shadow_ptr = *frequency_adjust_value_ptr;
    i2c_write_reg((uint16_t)*frequency_adjust_shadow_ptr >> 8, 0xc9);
    i2c_write_reg((uint8_t)*frequency_adjust_shadow_ptr, 0xca);
  }
  if ((key_code == KEY_DOWN) || (key_code == KEY_FAST_UP)) {
    *frequency_adjust_value_ptr = *frequency_adjust_value_ptr + 1;
    if (0x2b0 < *frequency_value) {
      *frequency_value = 0x2b0;
    }
    disp_offset(*frequency_adjust_value_ptr, 2, 7, 1);
  }
  if ((key_code == KEY_UP) || (key_code == KEY_FAST_DOWN)) {
    if (*frequency_adjust_value_ptr < 0x45) {
      *frequency_adjust_value_ptr = 0x45;
    }
    *frequency_adjust_value_ptr = *frequency_adjust_value_ptr - 1;
    disp_offset(*frequency_adjust_value_ptr, 2, 7, 1);
  }
  if (key_code == KEY_START) {
    *parameter_output_mode_ptr = 1;
    disp_string(DISPLAY_STATUS_OUTPUT_DISABLED, 3, 0xb, 0);
  }
  if (key_code == KEY_STOP) {
    *parameter_output_mode_ptr = 0;
    disp_string(DISPLAY_FREQUENCY_STOPPED, 3, 0xb, 0);
  }
}
