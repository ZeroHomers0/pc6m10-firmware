/* =============================================================================
 * 08_uart3_receive.c — UART3 接收帧组装
 *
 * 该函数属于 UART3 驱动，而不是通用占位模块。它只负责把 UART3 RBR
 * 收到的字节按当前接收状态写入 Modbus 帧缓冲区。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"

void uart3_receive_frame(void)
{
  volatile uint8_t *receive_state = uart3_rx_state_ptr;
  volatile uint8_t *inter_byte_gap = uart3_rx_gap_ptr;
  volatile uint8_t *receive_index = uart3_rx_index_ptr;
  volatile uint8_t *receive_buffer = uart3_rx_buffer;

  if (*receive_state == 0) {
    *receive_index = 0;
    *receive_state = 1;
  }
  if (*receive_state == 1) {
    *inter_byte_gap = 0;
    receive_buffer[*receive_index] = UART3->RBR;
    *receive_index = (uint8_t)(*receive_index + 1);
  }
}
