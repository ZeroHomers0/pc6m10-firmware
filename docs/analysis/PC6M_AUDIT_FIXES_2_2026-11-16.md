# PC6M 第二轮审计问题修复及原固件验证

2026-11-16（第二轮）：按用户要求逐模块精读源码并回查原固件反汇编，第二轮又确认 5 类功能/状态差异与 6 类结构差异（显示调用契约、控制量重算范围、枚举越界、擦除项号、兜底超时）。本轮全部按原 BIN 修复并新增显示调用契约回归。保留原有菜单文字、字体、产品信息、20 秒背光与认证放行等授权定制。

## 本轮确认并修复的差异

### 功能级（会改变可见行为）

1. **参数页电压/电流限值绘制**（`07_state_machine.c` 中 `draw_basic_parameter_item` case 4/5）。
   原 BIN 0x7606–0x76BE：`cmp range,limit` 后 `blo 0x7636`，未达限值时只画
   `DISPLAY_OPTION_DISABLED(0x6038)`；达到限值才 `disp_uint4(limit,row,0xb,attr)` 并在
   列 0xf 补单位 `'V'(0x7974)` / `'A'(0x7980)`。源码原先无条件画数值且无单位。
2. **软起/软停时间宽度**（case 6/7）。原 BIN 0x76FE/0x7724 用 `disp_uint4`（4 列），源码用
   `disp_number3`。
3. **PID 页 gain_a/gain_b 宽度**（参数菜单进入 PID 页）。原 BIN 0x677A/0x6788/0x67AA/0x67B8/
   0x67DA/0x67E8/0x680A/0x6816 全为 `disp_uint4`，源码用 `disp_uint2`。
4. **control_mode==2 不重算目标幅值**（`state_machine_page_main`）。原 BIN 0x5300 起：
   mode0 → 频率*电压量程/1000，mode1 → 电流量程，**mode2 直接把目标幅值写入平均/二次给定**；
   源码的 `else` 把 mode2 也按电流量程重算。
5. **control_mode==2 不重算手动输出**。原 BIN 0x53E2/0x53FE/0x5418 同结构；源码 mode2 仍按
   量程重算 `manual_scaled_output`。

### 状态级

6. **枚举越界不绘制**。control_mode=3 / control_method=3 / emergency_stop=3 时，原 BIN 用
   独立 `cmp #2; bne` 跳过该行（0x4C88/0x4C9C/0x4CB2 与 0x85xx/0x87xx 同构），源码 `else`
   会多画 OPTION_3/AUTO。
7. **control_method>=3 跳过手动 ±1**。原 BIN 0x5362 `cmp #2; bne 0x542C` 直接返回，源码
   `else` 仍执行手动加减。
8. **擦除按当前项号切换**。原 BIN 每个 case 重新 `ldr [ui_item_index]`（0x7A4E…0x7BC2），源码
   用帧首缓存值，KEY_BACK 当帧触发擦除时会按旧项擦错行（1 帧）。实测该帧在仿真中不可达
   （KEY_BACK 在入口阶段已切屏），仍按原 BIN 改回读取指针。
9. **状态监视页无故障帧提前返回**。原 BIN 0xA08C–0xA09E 画完空格串直接返回，跳过
   0xA29A 的 `idle++`；源码该帧仍计数（超时阈值约 +0.4% 漂移）。已补 `return`。
10. **非法屏号兜底超时**。原 BIN 分派链末端 0xA058 `b 0xAA9A`，非法屏号仍执行 `idle++` 与
    0x1388 超时（到期回主屏 + splash）；源码 `default: return` 会永久停留。已补兜底分支。

### 结构/注释

11. `07_state_machine.c` 参数菜单项号注释由「0-8」改为「0-7」：原 BIN UP 钳位同为 `>7 归 7`，
    item8 分支两侧同为死代码（连续 17 次 UP 后两侧 item 均为 7）。
12. 删除 `state_machine_page_main` 中冗余的 `*run_stop_state_ptr = 1`（条件已要求其值为 1）。

## 验证结果

- 构建：text 59808 / data 0 / bss 2188。
- 新增 `tmp/probe_disp_trace.py`：hook 12 个显示函数记录 `(函数, r0..r3)` 调用序列，覆盖
  菜单 3（16 项 × 6 参数变体）与菜单 7（9 项 × 4 变体）→ **640/640 一致**；参数菜单入口
  9 项 × 6 变体 → **54/54 一致**。
- 新增 `tmp/probe_sm_state.py`：状态机终态差分（SRAM0 0x10000000–0x10002200 + 6 个已映射
  外设区）→ **42/42 一致**；`tmp/probe_nav.py` 连续按键导航 → **18/18 一致**。
- 新增回归 `test/emulation/test_display_call_equivalence.py`：显示调用契约 152/152、
  控制量终态 12/12。
- 全量回归 `python test/run_tests.py` → **30/30**。
- 原 BIN A/B `python tools/verification/verify_firmware_equivalence.py` → VECTOR/UART_RX/
  PIN_CONFIG 48/GPIO2_INIT 7/AUTH_CHALLENGE 85/TIMER1_ISR 4/EINT1-3 1·1·13/TIMER2 6/
  TIMER1_MATRIX 126/CRC 5/MODBUS_REGS read65 write320/CLOSED_LOOP 6/OUTPUT_STAGE 144/
  STATE_MACHINE 130/DISPLAY 106/DISPLAY_FULL_EXEC 4/RELAY_DIRECT/XO_FONT **全部 PASS**。
- 第一轮修复的专项矩阵仍然通过：Modbus 0x03 读矩阵 62/62、0x06 写矩阵 305 例、
  0x10 多写 13 例、随机帧 400/400、output_stage 随机差分 300/300。
- 三个只读子代理分别复核：`02_lcd_display.c`（22 函数，无行为差异）、`04_i2c.c`+`05_adc.c`+
  启动链（无行为差异，2 处 P3 注释/启动时序）、`03_input_debounce.c`+`06_frequency_adjust.c`+
  `10_relay_led.c`+`13_gpio_init.c`+`11_auth.c`+`12_closed_loop.c`（无行为差异，
  `auth_challenge` 的 32 位读已改回 `ldrb`）。本轮 07_state_machine 的差异由两个子代理报告、
  本代理逐条回查反汇编确认后修复。

## 本轮未证明的部分

- 显示差异的等价性只覆盖「调用序列」层面（函数名、参数、行列、反显），未覆盖字库点阵像素；
  定制字形仍需目视实机。
- 未验证全部中断交错、看门狗真实复位、电气时序；`0x03` 读循环的 `count+2` 迭代属原厂
  「多读 2 个寄存器」怪癖，已复现且响应字节一致，暂存槽残留差异不视为缺陷。
- PC12M-2 的第二轮排查仍在进行，本记录不含其结论。

## 产物

- 探测与差分脚本：`tmp/probe_disp_trace.py`、`tmp/probe_sm_state.py`、`tmp/probe_nav.py`、
  `tmp/probe_read_matrix.py`、`tmp/probe_write_matrix.py`、`tmp/probe_modbus_fuzz.py`、
  `tmp/probe_output_stage_random.py`。
- 新增回归：`test/emulation/test_audit_fixes_2.py`、`test/emulation/test_display_call_equivalence.py`。
- 未烧写硬件。
