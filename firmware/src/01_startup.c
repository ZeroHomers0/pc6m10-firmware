/* =============================================================================
 * LPC1765FBD100 (ST33C 变频电源 / PC6M-10 三相晶闸管移相触发板)
 * 反编译源码导出 — 模块 01：IAR 运行时 / 启动 / 系统初始化
 *
 * 工具：Ghidra 反编译（MCP）
 * 固件：LPC1765.bin（NXP LPC1765 / Cortex-M3）
 * 导出：2026-08-20
 * 说明：反编译原样保留；<...> 内为理解注释。
 *
 * 交叉引用：
 *   · 中断/定时器架构（TIMER0 系统节拍/EINT/向量表）→ docs/PROGRESS_2026-08-20.md §4j
 *   · 启动序列与硬件（LPC1765 / 12MHz 晶振）→ docs/HARDWARE_VERIFICATION_2026-08-20.md §一.1
 *   · 认证门控调用 → 11_auth.c（ADuM1201 隔离链路，HARDWARE_VERIFICATION §二.5）
 * ========================================================================== */

/* ---------- 固定地址工作区说明 ---------- */
/* 主循环节拍、输入码和认证状态均通过 firmware_state.h 中的语义化映射访问。 */


/* =============================================================================
 * src/01_startup.c — 反编译模块 01（启动/初始化/main/系统时钟）可编译副本
 * 目标B 阶段4：补 include + 移除 IAR runtime 函数（startup.s/vectors.c/
 *   lpc1765.ld 等价替代）+ MMIO 符号换 reg.h 宏 + 跨模块函数前向声明。
 * ========================================================================== */
#include "inc/types.h"
#include "inc/reg.h"
#include "inc/firmware_api.h"
#include "inc/firmware_state.h"
#include <stdbool.h>

/* ==================== IAR EWARM 运行时（已移除，见文件头） ==================== */



/* ==================== 看门狗 ==================== */

/* 0x000001E4 —— 看门狗超时 ISR：清超时标志 + 计数+1（超时监控用） */
void WDT_IRQHandler(void)
{
  WDMOD = WDMOD & 0xfb;
  watchdog_timeout_count = watchdog_timeout_count + 1;
  return;
}


/* 0x00000200 —— 看门狗初始化（timeout_cnt=超时计数值，main 里传 200） */
void wdt_init(uint32_t timeout_cnt)
{
  watchdog_timeout_count = 0;
  NVIC_ISER0 = 1;                       /* 使能 WDT IRQ（IRQ0） */
  WDTC = (timeout_cnt & 0x1ffff) << 0xd;    /* WDTC：看门狗定时值 */
  WDMOD = 3;                            /* WDEN+WDRESET */
  WDFEED = 0xAA;                        /* 喂狗序列第 1 字节 */
  WDFEED = 0x55;                        /* 喂狗序列第 2 字节 */
  return;
}


/* 0x00000238 —— 喂狗（WDFEED=0x55） */
void wd_feed(void)
{
  WDFEED = 0xAA;
  WDFEED = 0x55;
  return;
}


/* ==================== 系统节拍定时器 TIMER0 ==================== */

/* 0x00000248 —— TIMER0 初始化：MR0=1999（节拍周期），匹配中断 IRQ0 */
void timer0_init(void)
{
  SYSTEM_CONTROL->power_control = *system_pconp | 2;  /* PCONP |= 2：TIMER0 上电 */
  TIMER0->TCR = 2;            /* 复位 TC/PC（先复位再配置） */
  TIMER0->PR = 0x18;          /* 预分频 24 */
  TIMER0->MR0 = 1999;         /* MR0 = 1999（节拍周期） */
  TIMER0->IR = 0xff;          /* 清全部中断标志 */
  TIMER0->MCR = 3;            /* MR0 匹配→中断 + 复位 */
  TIMER0->TCR = 1;            /* 计数使能 */
  NVIC_ISER0 = 2;             /* 使能 TIMER0 IRQ（IRQ1） */
  return;
}


/* 0x0000029A —— TIMER0 节拍 ISR（主循环节拍源）：
 *   tick_ready=1（主循环等待它）；phase_cnt++（钳位 200）；tick_countdown--（若非0） */
void TIMER0_IRQHandler(void)
{
  volatile uint8_t *phase_counter_ptr;

  TIMER0->IR = 0xff;
  *system_tick_ready_ptr = 1;
  phase_counter_ptr = system_phase_counter_ptr;
  *system_phase_counter_ptr = *system_phase_counter_ptr + '\x01';
  if (200 < (uint8_t)*phase_counter_ptr) {
    *phase_counter_ptr = 200;
  }
  if (system_tick_countdown != '\0') {
    system_tick_countdown = system_tick_countdown + -1;
  }
  return;
}


/* ==================== 芯片时钟/电源初始化 ==================== */

/* 0x00000440 —— 系统初始化：内部 RC → PLL0/PLL1 → 外设时钟树
 * 时钟/电源寄存器块 0x400FC000（SCB 区，对照语义地址映射）：
 *   所有 PLL、PCLK 和 PCONP 寄存器均通过 firmware_state.h 的语义化地址访问。
 * PLL 配置要点：改 PLLxCON 后须向 PLLxFEED 写 0xAA→0x55 序列锁存；随后
 *   do{}while 轮询 PLLxSTAT 位直到锁相完成。pll_feed 即 PLL0FEED 寄存器指针。 */
void SystemInit(void)
{
  CLOCK_CONTROL->PLL1_LOCK_CONTROL = 0x20;
  do {
  } while ((CLOCK_CONTROL->PLL1_LOCK_CONTROL & 0x40) == 0);
  CLOCK_CONTROL->CLOCK_CONFIGURATION = 3;
  CLOCK_CONTROL->CLOCK_SOURCE_SELECT = 1;
  CLOCK_CONTROL->PLL0_CONFIG = system_pll0_config_value;
  CLOCK_CONTROL->PLL0_FEED = 0xaa;
  CLOCK_CONTROL->PLL0_FEED = 0x55;
  CLOCK_CONTROL->PLL0_CONTROL = 1;
  CLOCK_CONTROL->PLL0_FEED = 0xaa;
  CLOCK_CONTROL->PLL0_FEED = 0x55;
  do {
  } while ((CLOCK_CONTROL->PLL0_STATUS & 0x4000000) == 0);
  CLOCK_CONTROL->PLL0_CONTROL = 3;
  CLOCK_CONTROL->PLL0_FEED = 0xaa;
  CLOCK_CONTROL->PLL0_FEED = 0x55;
  do {
  } while ((CLOCK_CONTROL->PLL0_STATUS & 0x3000000) == 0);
  CLOCK_CONTROL->PLL1_CONFIG = 0x23;
  CLOCK_CONTROL->PLL1_FEED = 0xaa;
  CLOCK_CONTROL->PLL1_FEED = 0x55;
  CLOCK_CONTROL->PLL1_CONTROL = 1;
  CLOCK_CONTROL->PLL1_FEED = 0xaa;
  CLOCK_CONTROL->PLL1_FEED = 0x55;
  do {
  } while ((CLOCK_CONTROL->PLL1_STATUS & 0x400) == 0);
  CLOCK_CONTROL->PLL1_CONTROL = 3;
  CLOCK_CONTROL->PLL1_FEED = 0xaa;
  CLOCK_CONTROL->PLL1_FEED = 0x55;
  do {
  } while ((CLOCK_CONTROL->PLL1_STATUS & 0x300) == 0);
  CLOCK_CONTROL->PERIPHERAL_CLOCK_SELECT0 = 0;
  CLOCK_CONTROL->PERIPHERAL_CLOCK_SELECT1 = 0;
  SYSTEM_CONTROL->power_control = system_pconp_default;
  CLOCK_CONTROL->PERIPHERAL_CLOCK_SELECT1_ALIAS = 0;
  CLOCK_CONTROL->SYSTEM_CONTROL_REGISTER = 0x303a;
  return;
}


/* 0x00000598 —— 长延时（6000×1000 空循环，用于等待电源稳定） */
void long_delay(void)
{
  volatile uint32_t i;
  volatile uint32_t j;

  for (i = 0; i < 6000; i++) {
    for (j = 0; j < 1000; j++) {
      /* 原固件空转延时；volatile 防止 -Os 删除。 */
    }
  }
}


/* 0x000005CA —— 空函数（bx lr） */
void stub_ret(void)
{
  return;
}


/* ==================== 主程序 ==================== */

/* 0x000005CC —— 主程序
 * 启动序列：
 *   SystemInit → pin_config → gpio1_init → long_delay → gpio0_input_init →
 *   read_input_p02 → timer0_init → gpio_inputs_dir_init → i2c_gpio_init →
 *   adc_init → load_config → pin_config → gpio2_init → timer1_init →
 *   timer2_init → eint1_init → eint2_init → eint3_init → uart3_init →
 *   long_delay → 开机认证(auth_challenge ×3) → disp_splash_screen →
 *   auth_retry → wdt_init(200)
 *
 * 认证逻辑：
 *   (0x10000744==0) && (0x10000748<3)：auth_challenge()
 *     0x1000074C==0（成功）→ 0x10000750=1（放行）
 *     0x1000074C==1（失败）→ 0x10000750=0（锁机）；0x10000748++（重试计数）
 *     成功后 param_sync_live_to_eeprom()（参数同步）
 *
 *   2026-08-27 决定（抄板）：永久强制认证放行，防抄板认证不再启用，
 *   不因 EEPROM 认证状态锁机。0x1000072C 与状态机 SYNC_2C（07_state_machine.c）
 *   同址：=1 时跳过 RLY3/2/1 安全强制驱动，继电器走正常输出逻辑。
 *   安全强制驱动，继电器走正常输出逻辑。
 *
 * 分支：
 *   · 0x10000750==1（认证通过/强制放行）：chk_p02_p03()<1（P0.2/P0.3 安全联锁未触发）
 *     → 主循环：adc0_scan_channels→input_scan_state→adc0_scan_channels→
 *       state_machine→output_stage→wd_feed→uart3_rx_timeout_monitor→modbus_dispatch(0)
 *   · 0x10000750==1 但联锁触发 → 显示"急停/联锁错误"屏（disp_string 0x76C 区）死循环
 *     + freq_adjust_sync + run_stop_preset
 *   · 0x10000750==0（认证失败，正常不可达）→ 显示错误屏（0x754 区）死循环锁机 */
void main(void)
{
  volatile uint8_t *input_code_ptr;
  uint8_t input_key;
  int interlock_state;

  SystemInit();
  pin_config();
  gpio1_init();
  long_delay();
  gpio0_input_init();
  read_input_p02();
  timer0_init();
  gpio_inputs_dir_init();
  i2c_gpio_init();
  adc_init();
  load_config();
  pin_config();
  gpio2_init();
  timer1_init();
  timer2_init();
  eint1_init();
  eint2_init();
  eint3_init();
  uart3_init(0);
  long_delay();
  if ((system_auth_enabled == '\0') && (system_auth_retry_count < 3)) {
    auth_challenge();
    if (system_auth_result == '\x01') {
      system_access_granted = 0;
    }
    if (system_auth_result == '\0') {
      system_access_granted = 1;
    }
    system_auth_retry_count = system_auth_retry_count + 1;
    param_sync_live_to_eeprom();
  }
  disp_splash_screen();
  auth_retry();
  wdt_init(200);
  /* —— 2026-08-27 决定（抄板）：防抄板认证永久放行，不再启用。
   *    忽略 auth_challenge/auth_retry 结果；1=放行、0=锁机（0x1000172C 与状态机 SYNC_2C 同址）。 */
  system_access_granted = 1;
  if (system_access_granted == 0) {
    /* —— 认证失败：锁机屏（认证已永久放行，正常不可达） —— */
    disp_clear();
    disp_string(0x754,0,4,0);
    disp_string(0x760,2,4,0);
    do {
      wd_feed();
    } while( true );
  }
  /* —— 认证通过 —— */
  interlock_state = chk_p02_p03();
  if (interlock_state < 1) {
    /* —— 主循环（正常运行态）—— */
    do {
      do {
      } while (system_tick_ready != '\x01');   /* 等系统节拍就绪 */
      system_tick_ready = '\0';
      stub_ret();
      adc0_scan_channels();
      input_key = input_scan_state();
      system_input_code = input_key;
      adc0_scan_channels();
      state_machine(system_input_code);
      output_stage();
      wd_feed();
      uart3_rx_timeout_monitor();
      modbus_dispatch(0);
    } while( true );
  }
  /* —— 安全联锁触发（P0.2/P0.3）：显示联锁错误屏 + 降频 + 停机预设 —— */
  disp_clear();
  disp_string(0x76c,0,4,0);
  disp_string(0x778,1,2,0);
  disp_string(0x784,2,2,0);
  disp_offset(system_display_offset,2,7,1);
  disp_string(0x790,3,0,0);
  do {
    do {
    } while (system_tick_ready != '\x01');
    system_tick_ready = '\0';
    input_key = input_scan_state();
    input_code_ptr = system_input_code_ptr;
    system_input_code = input_key;
    freq_adjust_sync(*input_code_ptr);
    run_stop_preset();
    wd_feed();
  } while( true );
}


/* 0x000007A8 —— 简单延时（loops×50 空循环） */
void Delay(int loops)
{
  int remaining_loops = loops * 50;

  while (remaining_loops != 0) {
    /* 空编译屏障：保留原固件的软件延时循环，同时避免volatile栈读写改变时序。 */
    __asm volatile ("" ::: "memory");
    remaining_loops--;
  }
}
