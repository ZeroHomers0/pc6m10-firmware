/* =============================================================================
 * firmware_types.h — 固件领域类型
 *
 * 枚举值是原固件输入扫描和状态机之间的既有数值契约。它们只用于局部变量、
 * 函数参数和分支表达式；映射到 SRAM 的状态仍按明确的 uint8_t/uint32_t 访问。
 * ========================================================================== */
#ifndef FIRMWARE_TYPES_H
#define FIRMWARE_TYPES_H

#include <stdint.h>

typedef enum {
  KEY_NONE = 0x00,
  KEY_CONFIRM = 0x01,
  KEY_DOWN = 0x02,
  KEY_UP = 0x03,
  KEY_BACK = 0x04,
  KEY_START = 0x05,
  KEY_STOP = 0x06,
  KEY_SLOW_UP = 0x0B,
  KEY_COMBINED_SLOW_DOWN = 0x0E,
  KEY_FAST_UP = 0x16,
  KEY_SLOW_DOWN = 0x17,
  KEY_FAST_DOWN = 0x21
} KeyCode;

#endif /* FIRMWARE_TYPES_H */
