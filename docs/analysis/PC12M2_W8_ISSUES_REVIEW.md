# PC12M-2 复查：PC6M 实机测试（W8）暴露问题是否会重现（任务 #5）

> 日期：2026-08-31
> 范围：严格复查 `PC6M-10/docs/w8/` 14 份文档中 PC6M 实机测试暴露的全部问题，
> 逐一对照 PC12M-2 的 12p 原始 BIN 反汇编 / 可编译 C 源码 / A/B 等价验证结果，
> 判定每项**是否会在 PC12M-2 重现**、是否已修复、是否仍需 12p 实机核对。
> 依据：12p 结论一律以 `backup/pc12m2_orig.bin`、12p 反汇编（evidence/reverse/disassembly）、
> A/B 差分（`verify_firmware_equivalence_12.py`）、额外覆盖测试（`test_extra_coverage_12.py` 113/113）
> 与本板 HARDWARE_VERIFICATION 为准；6p 仓库仅只读参考。
> 关联：`PC12M2_TEST_COVERAGE_REVIEW.md`（任务 #4 测试覆盖复查）。

## 一、结论速览

| 类别 | 项数 | 12p 结论 |
|---|---|---|
| A. 流程/工具约束（非固件 bug，实机同样适用） | 5 | 12p 同样适用（SWD 复用、ISP/MEMMAP/排针、哨兵/继电器监测） |
| B. 6p 固件 bug（6p 已修，12p 核对是否已修） | 9 | **全部已修复或未重现**（6 项源码已修+测试覆盖、3 项原厂设计非 bug） |
| C. 阶段 2 触发/引脚修正 | 3 | 12p 触发脚全集与 6p 修正后一致；2-3 判据需 12p 实机波形核对 |
| D. 6p 遗留未决项 | 2 | 12p case1E 三分支已确认正确；case3 item10 已钳位闭环 |
| 合计 | 19 项问题 + 6 项流程/清单 | **无一项需要在 12p 做代码级修复**（认证放行、ADC、FIO、钳位、大端、恢复出厂、坐标、G5/G6 等 12p 全部已就位） |

---

## 二、逐项复查明细

### A. 阶段 0/流程类（实机操作约束，非固件 bug）

#### A1. SWD 连不上（P1.30→AD0.4、P1.29→RS485 复用调试脚）【6p 根因已定】

- **6p 现象**：J-Link 无法 connect，根因 = 固件初始化复用 SWD 脚：`adc_init()` 把 P1.30(SWDIO) 复用为 AD0.4、`uart3_init()` 把 P1.29(SWCLK) 用作 RS485 DE/RE。
- **12p 核对**：**同样复用，属设计行为（非 12p bug）**。
  - `05_adc.c` `adc_init()`（0x1F04）：`DAT_000022b0=0x4002C000` 实为 **PINSEL 基址**（反汇编语义化注释误标"时钟/PCLK"，地址本身即 PINSEL0）。`+4`=PINSEL1（&0xffc03fff | 0x154000 → P0.23/24/25 = AD0.0/1/2），`+0xc`=PINSEL3（&0xcfffffff |0x30000000 → **P1.30 = AD0.4**；&0x3fffffff |0xc0000000 → **P1.31 = AD0.5**）。
  - `08_uart3_modbus.c` `uart3_init()`（0xA994）：`FIO1CLR |= 0x20000000`（bit29）→ **P1.29 置为 RS485 DE/RE 空闲**（line 79-80）。
- **12p 结论**：与 6p 完全一致——复位后 P1.30/P1.29 被复用，SWD 实机连不上是**设计冲突非故障**。实机介入路径相同：加 nRESET 走 connect-under-reset，或走 ISP（UART0 + P2.10 拉低）。**无代码改动**。

#### A2. P12 排针定义实机确认【硬件事项】

- 12p 板排针定义以 **12p 板硬件文档 / 丝印实测** 为准，不沿用 6p 的 P12 定义。本次复查不涉及代码。

#### A3. ISP 备份与烧录（CRP 读保护、9600 卡死、先备份后擦除）【流程约束】

- **12p 结论**：12p 实机同样必须先备份 `pc12m2_orig.bin`（板内当前 CRP 状态只能实读，`0x2FC` 文件值≠板内值）；9600 bps 写 256KiB HEX 卡死坑同样存在；**未找到并核对原固件备份和第二物理副本前绝不擦写 Flash；若 CRP ≠ 0xFFFFFFFF 绝不执行 unlock/recover/mass erase**。流程约束，12p 实机同样适用。

#### A4. 自编译固件 CRP 地址布局冲突（0x2FC 被指令占用）【6p 已修】

- **6p**：firmware.bin 偏移 0x2FC=0x721A2255（落在 wd_feed 指令），三处修复（startup.s .crp 段 + ld 固定 0x2FC + build.sh --gap-fill 0xFF）。
- **12p 核对**：**12p 在反向工程阶段已预先修复**（同 6p 方案）：
  - `firmware/lpc1765.ld`：`.crp 0x2FC : { KEEP(*(.crp)) } > FLASH`，`.text` 从 0x300 起；
  - `firmware/src/startup.s`：`_crp_word = 0xFFFFFFFF` 发射；
  - `firmware/build.sh`：objcopy `--gap-fill 0xFF`。
  - 12p `wdt_init/wd_feed` 地址（0x200/0x238）均在 0x300 之后，未落 0x2FC。
- **12p 结论**：**不会重现**。重建固件后 `pc12m2` 布局的 0x2FC = 0xFFFFFFFF。

#### A5. SWD 烧写后 MEMMAP 向量重映射坑【工具流程】

- **12p 结论**：J-Link reset & halt 停 Boot ROM 时 `MEMMAP=0`，读真实 Flash 前须 `w4 0x400FC040, 1`（MEMMAP=1）；`verifybin` 走 flash loader 不受影响。工具流程，12p 实机同样适用。

#### A6. 烧写后核心 halt + SWD 失联【预期行为】

- **12p 结论**：同 A1——烧写后需复位才运行；运行后 P1.30/P1.29 复用 → SWD 失联（设计行为）。12p 相同。

---

### B. 6p 固件 bug（6p 已修复，12p 逐项判定）

#### B1. 认证锁机【6p 强制放行；12p 任务 #6 已完成】

- **6p**：`auth_challenge/auth_retry` 失败 → 锁机屏死循环；决定抄板：`main()` 在认证后强制 `*DAT_00000750 = 1` 永久放行（调用保留保 A/B 等价）。
- **12p 核对**：**12p 反逻辑**：`auth_pass_flag(0x100020C0) == 0` 为通过。任务 #6（2026-08-31）已在 `01_startup.c` `main()` 认证段后强制 `*lock = 0`（`auth_pass_flag` 同址），锁机分支（"报警忙碌/CPU 忙碌"）正常不可达；`auth_verify_loop()` 调用保留（保持原厂语义与 A/B 等价）。
- **12p 结论**：**已修复（任务 #6）**。重建固件后 `verify_firmware_equivalence_12.py` 全 PASS（含 `AUTH: PASS funcs=3`）、`test_extra_coverage_12.py` 113/113 PASS。注意 6p 与 12p **认证结果语义相反**（6p 1=放行、12p 0=通过）。

#### B2. ADC wait_done 指针算术坑【6p 已修；12p 已修+测试覆盖】

- **6p**：`g_adc + 4`（uint32_t*）编译成 +16 字节 → 读 AD0DR0 而非 AD0GDR → 非 ch0 通道 DONE 永不置位 → 死循环 → WDT 复位循环（闪屏+按键死）。
- **12p 核对**：`05_adc.c` `adc0_wait_done()` 用 `(uint)g_adc + 4` 先转整数做字节偏移读 AD0GDR(0x40034004)。`g_adc` 是 `uint32_t*` 的同类坑已避。
- **12p 结论**：**不会重现**。测试覆盖：`test_extra_coverage_12.py` `adc_wait_done 4 PASS`（同时给 AD0GDR/AD0DR0 播不同结果，错读立即 FAIL）。

#### B3. input_scan FIO 读址错位【6p 已修；12p 已修+测试覆盖】

- **6p**：`DAT_00001974(uint32_t*) +0x34` 元素偏移 → +0xd0 → 读 FIO 无效地址 → 按键永不触发。
- **12p 核对**：`03_input_debounce.c` `input_scan_state()` 用 `(uint32_t)DAT_00001924 + 0x34/0x14/0x74` **字节偏移**读 FIO1PIN/FIO0PIN/FIO2PIN。
- **12p 结论**：**不会重现**。测试覆盖：`input_scan_state 6 PASS`（全 6 位引脚组合 × 计数初值差分 512 例 + 关键事件）。

#### B4. case1 三分支 FAULT 门控【6p 已修；12p 结构正确】

- **6p**：DISP_SEL 三分支被 C 复刻嵌套进 `FAULT==0` else → 缺相置位 → 上下键永不执行。
- **12p 核对**：`07_state_machine.c` case1（0x48a0-0x514c）DISP_SEL 三分支位于 **FAULT 门控之外**（0x501a-0x5148，line 582-611）：`DISP_SEL==0`→TARGET=HSRC+换算、`==1`→仅 V_AMP=V_AMP2、`==2`→key2/0x16 增 MANUAL（clamp 0x3e8/0xa）+key3/0x21 减 MANUAL+TARGET=MANUAL。启动/停机/块A/块B 逻辑（line 550-580）有 FAULT 门控，三分支独立。
- **12p 结论**：**不会重现**。A/B `STATE_MACHINE_MATRIX 130 场景` PASS。

#### B5. DISP_SEL=控制方式（原厂设计，非 bug）【6p 定论；12p 同设计】

- **6p**：`DISP_SEL(0x10001655)`=控制方式 0通讯/1本地/2定值，只有"定值"分支处理上下键调 MANUAL（原厂 0x52FA-0x542C + 手册双重确认）。
- **12p 核对**：`DISP_SEL(0x1000164d)`（≠6p 0x10001655）同样三值，case1 仅 `==2` 分支处理 UP/DOWN 调 MANUAL，与 OLD 一致。
- **12p 结论**：**非 bug（原厂设计）**。12p 实机同样需在"基本参数设置→控制方式"切到"定值(2)"后，首页上下键才可设定输入信号。

#### B6. case3/case4 编辑态选中值不闪烁【6p 已修；12p 已实现+测试覆盖】

- **6p**：反编译缺 `TIMEOUT3>0x1F4` 回绕擦除块 + 软起自动置 1 块 → 只有反显、值不消失。
- **12p 核对**：`07_state_machine.c` case3/case4 均含闪烁机制：
  - 进编辑 `key==1`：`*TIMEOUT3 = (MENU3==0)?0xfa:0x1f4`（公共尾部 ++ 后成 0xfb 整页重绘 / 0x1f5 擦除）；
  - 尾部 `TIMEOUT3++` → `==0xFB` 整页重绘（当前项反显 attr=1）→ `>0x1F4` 回绕 + 按 MENU2 用空格串擦除值列；
  - 软起自动置 1：`if (*b2c < 2 && *b44 == 0) *b44 = 1;`。
- **12p 结论**：**不会重现**。测试覆盖：`case3_edit 56 PASS`（menu=3 case3 编辑键矩阵 menu2=2..15）。

#### B7. 恒流 LED 不亮（原厂固有）+ 主屏开环 LED 错译【6p 已修；12p 未重现】

- **6p**：恒流时 P1.21 为低 = 原程序固有行为（非回归）；顺带修复主屏刷新开环分支误译 `fio1_pin20_ctrl(1)`→`(0)`。
- **12p 核对**：`07_state_machine.c` case1 主屏刷新 CTRL_MODE 分支已正确：`CTRL_MODE==0` → `fio1_pin20_ctrl(1); fio1_pin21_ctrl(0)`（开环 LED20A 亮）、`==1` → `(0);(1)`、`==2` → 双 0。
- **12p 结论**：**未重现错译**（12p 主屏开环分支原始即正确）；恒流 LED 亮灭规则同 6p 原厂固有，实机万用表核对。

#### B8. Modbus 大端字节序（4 处拼接反向）【6p 已修；12p 已正确】

- **6p**：`frame[4]|(frame[5]<<8)` 小端拼接 ×4 处 → 改 `(frame[4]<<8)|frame[5]`。
- **12p 核对**：`08_modbus_dispatch.c` **4 处全部为大端**：
  - 0x06 写单数据：`v = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];`（line 183 等）；
  - 0x10 写多数量：`cnt = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];`（line 691）；
  - 0x10 写多数据逐条：`v = ((uint32_t)frame[7 + i*2] << 8) | (uint32_t)frame[8 + i*2];`（line 705）；
  - 0x03 读保持数量：`cnt = ((uint32_t)frame[4] << 8) | (uint32_t)frame[5];`（line 737）。
- **12p 结论**：**不会重现**。测试覆盖：`modbus_dispatch 20 PASS`（读/写 0x06/0x10/异常/CRC/站址不匹配 + 13 帧异常矩阵）。

#### B9. 恢复出厂设置只显示"M"（字面量池误取）【6p 已修；12p 已修正】

- **6p**：`disp_string(0x6aa0,…)` 误取字面量池值 0x4D9C → 首字节 0x4D='M'。
- **12p 核对**：`02_lcd_display.c` `disp_screen_static()`（0x41B4）4 行主菜单 = "1.通信参数设置/2.保存参数设置/3.诊断参数设置/**4.恢复出厂参数**"，串地址用 **ADR 实际 flash 地址 0x436C/0x437C/0x438C/0x439C**，注释明确记录「2026-08-31 修正：原实现误用 literal pool 指针 0x4814/0x4824/0x4834/0x4844 作串地址」——同类坑已在 12p 移植中修正。
- **12p 结论**：**不会重现**。恢复出厂项串地址正确；A/B 显示验证覆盖。

#### B10. PID 三级菜单按键无法切换（原厂设计，非 bug）【6p 定论；12p 同设计】

- **6p**：A/B 16 场景 MENU2 终值 BIN=ELF 全一致，三级菜单切换限制为原程序固有逻辑。
- **12p 核对**：12p case7（MENU==7 PID 参数设置，0x8E18-0x97A8）与 OLD 逐指令一致，A/B 状态机矩阵覆盖。
- **12p 结论**：**非 bug（原厂设计）**。

#### B11. 控制方式互斥（钳位 vs 环绕）【6p 已修；12p 已正确】

- **6p**：case3 编辑 items 10-14 曾被写成"超上限环绕到 0"，BIN 实为钳位（item10 钳2、item11 钳1、item12 钳2、item13 钳1、item14 钳1）；item0 CTRL_MODE 例外保持双向环绕。
- **12p 核对**：`07_state_machine.c` case3 编辑增量分支：item0 环绕 `(*b2c)++; if (*b2c>2) *b2c=0;`；items 10-14 钳位 `case 10: (*b4d)++; if (*b4d>2) *b4d=2; break;` 等——与 6p 修复后语义一致。
- **12p 结论**：**不会重现**。测试覆盖：`case3_edit 56 PASS`。

#### B12. 本地模式无法设定值 + 复位按钮"重启"坐标错位【6p 已修；12p 已正确】

- **6p 6a**：DISP_SEL==1 本地模式无按键处理 = 原厂设计（非 bug）。
- **6p 6b**：复位流程两处 `disp_string` 坐标被反编译成 `(0,0,0)`，应为 `(3,0xa,0)`。
- **12p 核对**：
  - case1 复位流程（line 501/504）：`disp_string(0x4d6c, 3, 0xa, 0)`（"复位"）→ `disp_string(0x4d7c, 3, 0xa, 0)`（"重启"），**坐标已正确 (3,0xa,0)**；
  - case1E 复位流程（line 1926/1929）：同样 (3,0xa,0)。
- **12p 结论**：**不会重现**。本地模式无按键处理为原厂设计；复位/重启坐标 12p 原始即正确。

---

### C. 阶段 1 收尾 / 阶段 2 触发引脚

#### C1. SRAM 哨兵栈水位【实机流程】

- 12p `_estack = 0x100029A0`（lpc1765.ld），栈区实机用 connect-under-reset 停核 → ClrRESET 放行 → 运行 N 秒制造负载 → SetRESET 停核 → 读栈区残留找最后一个非 0 字。12p 实机同样需执行该流程（SWD 复用脚限制同 6p）。

#### C2. 继电器误吸合监测【12p 初态安全】

- **6p**：ULN2003 高有效，`pin_config` FIO0DIR P0.20/21/22 配输出后 **FIO0CLR 清 0** → 初始低电平=释放；上电预期表 A/B 差分定论。
- **12p 核对**：`09_output_stage.c` `pin_config()`（0xE308）：FIO0DIR P0.20/21/22（0x100000/0x200000/0x400000）配输出后立即 **FIO0CLR 清 0**（line 95-97）→ 12p 继电器初始同样释放。P0.22=RLY1 运行（`fio0_pin22_ctrl` 0xE6C6）、P0.21=RLY2 报警（`out_relay_p021`，10_relay_led.c）、P0.20=备用。
- **12p 结论**：**不会重现**（静态低风险）。上电瞬间三继电器全断开，RLY1 仅 RUN=1 后吸合（设计行为）。12p 实机复现 6p §2.2 步骤确认。

#### C3. G5/G6 引脚映射修正（P0.19/P0.16，非 P2.19/P2.16）+ 2-3 判据修正【12p 原始即正确】

- **6p**：AGENTS.md 曾误记 G5=P2.19、G6=P2.16；代码级证据三处一致为 **G5=P0.19、G6=P0.16**（FIO0DIR 配 P0.15..19、FIO2DIR 只配 P2.5..9 + P2.0）。
- **12p 核对**：`09_output_stage.c` `pin_config()` 触发脚全集（FIO0DIR + FIO0SET）：
  - **G1=P0.17、G2=P0.15、G3=P0.18、G4=P2.9、G5=P0.19(0x80000)、G6=P0.16(0x10000)**，12 脉波扩展 = P2.8/P2.7/P2.6/P2.5、P0.8/P0.7——**与 6p 修正后完全一致**，且 FIO2DIR 只配 P2.5..9（无 P2.19/P2.16）。
- **12p 结论**：**12p 原始即正确**（G5=P0.19、G6=P0.16），无需 6p 式修正；6p 的 AGENTS.md 勘误教训可直接复用于 12p 文档（勿误记为 P2.19/P2.16）。
- **2-3 判据**：6p 定论"无显式 12° 单脉，为 54μs 载波 @≈9.26kHz + 60° 窗口"。12p 为 12 相板，触发架构需以 **12p 实机波形** 核对（本仓库 HARDWARE_VERIFICATION_2026-08-31.md 记录 12p 触发/同步硬件），本次复查不做代码级预判。

#### C4. EINT 三相同步引脚【12p 相同】

- 12p `09_output_stage.c` `eint1/2/3_init`：PINSEL4 写 P2.11=EINT1（+0x400000）、P2.12=EINT2（+0x1000000）、P2.13=EINT3（+0x4000000），与 6p 相同（P2.11/12/13 = TV/TU/TW）。

---

### D. 6p 遗留未决项（12p 侧已闭环）

#### D1. `MENU==0x1e` 路径另一份三分支调用时机【6p 未决；12p 已确认】

- 12p case1E（MENU==0x1e，0x9fae-0xa854）DISP_SEL 三分支（line 2007-2015，0xa6fa-0xa854）为**独立 if 块**，位于块A/块B/key5/key6（均有 FAULT 门控）之后，**不受 FAULT 门控**——结构正确，调用时机与 case1 并行。**12p 已闭环**（6p 的同类疑问在 12p 无重现风险）。

#### D2. case3 item10 DISP_SEL 编辑键循环语义【6p 已闭环；12p 已钳位】

- 见 B11：12p item10 已钳位在 2（`if (*b4d>2) *b4d=2`），与 BIN 一致，无"定值按 UP 跳回通讯"。

---

## 三、12p 需在实机阶段补做的核对项（非代码缺陷）

| # | 核对项 | 依据 |
|---|---|---|
| 1 | SWD 介入路径实测（connect-under-reset / ISP） | A1/A6：P1.30/P1.29 复用 |
| 2 | 板内 `0x2FC` CRP 实读（备份前） | A3：文件值≠板内值 |
| 3 | P12/排针定义按 12p 板实机核对 | A2 |
| 4 | 恢复出厂项实机走一遍（"4.恢复出厂参数"屏完整显示） | B9：地址已修正，实机确认 |
| 5 | 恒流模式 P1.20/P1.21 电平 + LED18B 两端 | B7：原厂固有行为确认 |
| 6 | SRAM 哨兵栈水位（12p `_estack=0x100029A0`） | C1 |
| 7 | 继电器上电/复位无吸合脉冲（P0.20/21/22） | C2 |
| 8 | 12p 触发引脚 G1-G6 波形 + 2-3 判据（54μs 载波/60° 窗口） | C3：12p 板触发架构实机核对 |
| 9 | 门极有效电平极性、TIMER2.MR1 用途 | 12p 实机待测项 |

## 四、结论

- 对 W8 全部 19 项问题 + 6 项流程/清单逐项复查：**PC12M-2 无任何一项需要在代码级修复**。
- 6p 踩过的 **9 项固件 bug** 中，12p 已有 6 项在反向工程/移植阶段修复并被 A/B + 额外覆盖测试锁定（ADC 指针、FIO 读址、case3 钳位/闪烁、Modbus 大端、恢复出厂串地址、复位坐标），3 项为原厂设计（DISP_SEL 控制方式、PID 三级菜单、本地模式无按键）。
- 12p 独有的修正项（认证放行、case1/case1E 三分支门控、主屏 LED 错译）已在任务 #6 及 P5/P4 阶段完成并通过 A/B。
- 实机阶段只需按「三、12p 需在实机阶段补做的核对项」执行，重点为 **SWD 介入路径、CRP 实读、12p 触发波形 2-3 判据**。
