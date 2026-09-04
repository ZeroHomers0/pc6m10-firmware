/* =============================================================================
 * firmware_api.h — 固件模块公共接口
 *
 * 这里集中声明跨模块调用的函数。硬件和运行时地址统一由
 * firmware_state.h / firmware_parameters.h 提供语义化映射，业务模块不再
 * 各自复制一份前向声明，避免签名漂移和参数名失真。
 * ========================================================================== */
#ifndef FIRMWARE_API_H
#define FIRMWARE_API_H

#include <stdint.h>
#include "types.h"
#include "firmware_types.h"

/* 系统启动、节拍与看门狗 */
void SystemInit(void);
void WDT_IRQHandler(void);
void wdt_init(uint32_t timeout_count);
void wd_feed(void);
void timer0_init(void);
void TIMER0_IRQHandler(void);
void Delay(int loops);
void long_delay(void);
void state_machine(int key);

/* GPIO、输入与采样 */
void pin_config(void);
void gpio1_init(void);
void gpio2_init(void);
void gpio0_input_init(void);
void gpio_inputs_dir_init(void);
uint32_t read_input_p02(void);
uint8_t input_scan_state(void);
uint8_t scan_run_stop(void);
uint32_t debounce_p09(void);
uint32_t debounce_p116(void);
uint32_t debounce_p117(void);
uint32_t debounce_p06(void);
uint32_t chk_p02_p03(void);
void adc_init(void);
void adc0_start(void);
uint32_t adc0_wait_done(void);
void adc0_scan_channels(void);

/* EEPROM 参数 */
void i2c_gpio_init(void);
void i2c_delay_short(void);
void i2c_delay(uint32_t units);
uint32_t i2c_start(void);
void i2c_stop(void);
uint32_t i2c_write_byte(uint32_t byte_value);
uint32_t i2c_read_byte(void);
void i2c_write_reg(uint32_t data, uint32_t register_address);
void i2c_read_reg(uint8_t *out_buffer, uint32_t register_address);
void load_config(void);
void param_sync_live_to_eeprom(void);

/* LCD 显示 */
void lcd_ctrl_line(int enabled);
void lcd_data_byte(uint32_t byte_value);
void disp_data(uint32_t byte_value, int invert);
void disp_cmd(uint32_t command);
void disp_clear(void);
void disp_render_char8(uint32_t character, char row, uint32_t column, uint32_t invert);
void disp_render_char16(uint32_t gb_hi, uint32_t gb_lo, char row, int column, uint32_t invert);
void disp_string(int string_address, uint32_t row, uint32_t column, uint32_t invert);
void disp_digit(uint32_t digit, uint32_t row, uint32_t column, uint32_t invert);
void disp_number3(int value, uint32_t row, int column, uint32_t invert);
void disp_uint2(uint32_t value, uint32_t row, int column, uint32_t invert);
void disp_uint4(uint32_t value, uint32_t row, int column, uint32_t invert);
void disp_uint5(uint32_t value, uint32_t row, int column, uint32_t invert);
void disp_number(uint32_t value, uint32_t row, int column, uint32_t invert);
void disp_signed_angle(int angle, uint32_t row, int column, uint32_t invert);
void disp_offset(uint32_t offset, uint32_t row, int column, uint32_t invert);
void disp_fixed_1dec(uint32_t value, uint32_t row, int column, uint32_t invert);
void disp_decimal1(uint32_t value, uint32_t row, int column, uint32_t invert);
void disp_splash_screen(void);
void disp_screen_static(void);
void disp_screen_calib(void);

/* SCR 输出、中断与闭环 */
void timer1_init(void);
void timer2_init(void);
void eint1_init(void);
void eint2_init(void);
void eint3_init(void);
void EINT1_IRQHandler(void);
void EINT2_IRQHandler(void);
void EINT3_IRQHandler(void);
void TIMER1_IRQHandler(void);
void TIMER2_IRQHandler(void);
void output_stage(void);
void gpio_outputs_set(void);
void fio0_pin22_ctrl(int enabled);
void fio1_pin22_ctrl(int enabled);
void run_stop_preset(void);
void nvic_enable_irq(uint32_t irq_number);
int closed_loop_integral(int setpoint, int feedback, uint32_t coefficient_a,
                         uint32_t coefficient_b);
uint32_t closed_loop_wrapper(uint32_t setpoint, uint32_t feedback,
                             uint32_t coefficient_a, uint32_t coefficient_b);

/* 继电器与状态灯 */
void out_relay_p020(int enabled);
void out_relay_p021(int enabled);
void fio1_pin20_ctrl(int enabled);
void fio1_pin21_ctrl(int enabled);
void fio1_pin23_ctrl(int enabled);

/* 认证 */
void auth_set_timeout(void);
void auth_challenge(void);
void auth_retry(void);

/* UART3 / Modbus */
void uart3_init(uint32_t divisor);
void uart3_tx_byte(uint8_t tx_byte);
void uart3_rx_timeout_monitor(void);
void UART3_IRQHandler(void);
uint16_t crc16(uint8_t *data, uint16_t length);
uint32_t modbus_read_reg(uint32_t *out_value, uint32_t register_address);
uint32_t modbus_write_multi(uint32_t *source_value, uint32_t register_address);
void modbus_dispatch(int argument);
void uart3_receive_frame(void);
void func_0x0000aed0(void); /* 旧符号兼容别名，供历史 A/B 测试与反汇编索引使用 */

/* 联锁页上的频率调节 */
void freq_adjust_sync(int key_code);

#endif /* FIRMWARE_API_H */
