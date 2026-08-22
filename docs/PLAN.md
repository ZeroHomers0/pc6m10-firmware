# LPC1765 变频电源逆向工程 — 进度与计划

> 恢复时间：2026-08-20（会话因死机丢失后重建）
> 固件：`LPC1765.bin`（NXP LPC1765 / Cortex-M3，256KB flash @0x00000000）
> 设备：ST33C 变频电源（恒压/恒流/开环 三模式，正弦波，SINEP0WER）
> Ghidra 工程：`LPC1765FBD100/LPC1765FBD100.gpr`
> **⚠️ 会话恢复首选：先读 `PROGRESS_2026-08-20.md`（完整进度固化）**

---

## ✅ 已完成（可恢复的成果）

| 成果 | 文件 | 说明 |
|---|---|---|
| **全面进度固化（2026-08-20）** | `PROGRESS_2026-08-20.md` | **防丢失主文档**：完整寄存器表(63项读写对齐)、芯片同步全图(61组)、读写不对称、方法论 |
| **菜单→参数精确映射（任务#16，2026-08-20）** | `MENU_PARAMETER_MAPPING.md` | 面板菜单树全量逐项确证：基本参数**16 屏**（手册仅12）、保护 10 屏、通讯 4 屏、PID 9 屏、相位校准、恢复出厂、版本信息；**旧标修正**（量程/主从偏移/控制方式/保护参数/缺相/三相平衡/起始相位） |
| **硬件印证/修正（2026-08-20）** | `HARDWARE_VERIFICATION_2026-08-20.md` | 用户提供 doc/ 硬件文档（U38接线表/BOM/原理图/分析报告/面板手册）→ 设备定位**三相晶闸管移相触发板**；AT24C02C 证实；显示=12864 LCD；触发 GPIO=G1-G6+P12-G1~G6；认证=ADuM1201 隔离链路 |
| 状态机拆解 | `state_machine_analysis.md` | state_machine(0x458C) 全解：菜单状态表、字符串表、辅助函数、变量簇 |
| UART3 协议逆向 | `uart3_protocol.md` | Modbus-RTU 变体：帧格式、功能码 3/6/0x10、异常码、寄存器表、波特率 |
| I2C 参数系统 | `i2c_param_sync.md` | 芯片@0x53 架构：live/shadow 同步、双银行、参数组、完整芯片寄存器映射 |
| 反汇编转储 | `state_machine_disasm.txt` / `uart3_disasm.txt` | 指令级 |
| Ghidra 脚本 | `AddSramAndVars.java` `AnalyzeStateMachine.java` `CreateIsrFunctions.java` `TypeLiteralPointers.java` `create_isr_functions.py` | SRAM 段、变量标号、ISR 创建、字面量指针类型化、流程导出 |

### 已确证的协议/架构结论

- **Modbus-RTU 变体**：`[addr][func][0x10][reg][data][CRC]`，功能码 3/6/0x10，单字节寄存器编号(1..63)。
- **CRC-16 Modbus**：poly 0xA001 反射，init 0xFFFF，双 256 表 @0x11034/0x11134，`FUN_0000af64`。
- **从站地址**：0x100016FF，菜单范围 1..246，Modbus 写允许 0..247，无广播，精确匹配。
- **I2C 芯片 @0x53 = 24C02 类被动 EEPROM**：纯参数非易失存储（§4g.A，运行期零读回）；LPC1765 通过 FUN_000035f2 同步 61 组 live→芯片。**另有 P2.1-2.3 开机认证芯片（防克隆，失败锁机）**。
- **4 参数组**：cfg_1710(reg23) 选组 1..4，各组 2 增益(0x10001711-18)，活动对 0x1000170E/0F（仅本地用）。
- **读/写不对称**：reg 24/25 读=活动组增益/写=组4增益；reg 27-29 只写(别名 51-53)；reg 37 只写；**reg 40 读=ADC ch5(0x100015A8)/写=src_value(0x10001788)**。
- **reg 61 = 远程输出使能**（P0.20）；**reg 62 = 输出下限系数**（下限=180-值，reg9 上限）；reg 22 = 菜单第9项设定。

---

## 🔜 下一步计划（按优先级）

1. ✅ **5 个"输出设定字"物理含义** — **已确证 = ADC 各通道满度标定除数**（3500..4500）。详见 PROGRESS §4b。regs 40-45=实时测量值，**reg↔ADC 物理通道已确证**（任务#14，PROGRESS §4b）：reg40=Ug(AD0.5)、reg41=IA(AD0.2)、reg42=IB(AD0.1)、reg43=IC(AD0.0)、reg44=IF(AD0.3)、reg45=Uf(AD0.4)。
2. ✅ **reg 22(0..60)、reg 61(0..100)、reg 62(<181)** 语义 — **已确证**（详见 PROGRESS_2026-08-20.md §0 #12）：
   - reg 22 = 菜单第9项设定（编码器 0..60，芯片参数，内部不消费）。
   - reg 61 = **远程输出使能**（P0.20，FIO0SET/CLR bit20，值 0=低/非0=高）。
   - reg 62 = **输出下限系数**（FUN_0000e9ac 中下限=180-值；reg9 为上限）。"特殊值{3,63,63}"为误读已纠正。
3. ✅ **芯片 reg 0x6E-0x72**（0x10001722-26，5 字节）— **已确证 = 闭环误差分区参数**（阈值 0x6E/0x6F + 三区增益 0x70/0x71/0x72），供 FUN_000108b0 分区选增益。详见 PROGRESS §4e。
4. ✅ **输出级链路结构已通** — FUN_0000e9ac：基准duty+软起动phase 0→4→5+闭环 FUN_00010f0a（消费活动增益对 0x1000170E/0F）+报警积分+限幅+停机链（fio0/fio1_pin22）。详见 PROGRESS §4c。**闭环算法已确证**（§4e）：FUN_00010f0a→FUN_000108b0 分区选增益（芯片0x6E-0x72）+积分滤波，非线性非纯PID。**out_setpoint→输出外设映射已确证**（§4f/§4j/§4k，任务#7+#13）：LPC1765 **无本地 PWM/DAC，但用 TIMER2(0x40090000) 编程 SCR 触发角**——EINT3_IRQHandler 每输入过零算 `TIMER2.MR0 = out_freq_adj×10+BASE+out_fine×K−out_scale`（8 组 BASE=out_phase0/1×50/60Hz×mode1/2），**推翻"无输出外设"旧结论**（旧扫描漏 0x40090000）；TIMER2 匹配 → TIMER1 扫描 240 步生成 6 窗口触发脉冲（§4k 全表，60° 双脉冲/9.26kHz 载波/窗口边界 1160μs）。功率转换由外部电路/模块完成（§4g：芯片@0x53 为被动 EEPROM）。✅ **FUN_0000f9aa 预设写入源已确证**（§4h，任务#9）：0x100017F8/0x10001BE0 无写入者（BSS 零初始化），预设恒 0，运行从 0 软起动；0x100017F0=Modbus 写值暂存。**§4c 修正**：软起动门控=cfg_word==1&&reg40读回源(0x100015A8)>=10（非 src>9）；报警1/2 均比 reg45。
5. ✅ **写多寄存器 0x10 路径**(FUN_0000b2e0) — **已确证与写单不一致**：无逐值范围校验、落点映射不同（reg24/25→活动对、reg40→ADC ch5、reg27-32/37/38/44-46→scratch 0x100017AC）、不调 FUN_000035f2。详见 PROGRESS §4d。
6. ✅ **状态机_analysis 更新** — 本会话寄存器语义已并入 state_machine_analysis.md（§9 Modbus↔状态机映射表）。
7. ✅ **芯片 0x53 具体型号** — **AT24C02C（BOM U6，硬件印证）**（详见 PROGRESS §4g.A + HARDWARE_VERIFICATION）。另确证 **P2.1-2.4 经 ADuM1201(U25) 的隔离串行认证链路**（FUN_000106a0 挑战-应答，防克隆，失败锁机，见 §4g.B + 硬件印证）。**剩余**：①认证链路远程端器件 — **BOM 证实远程端为外部模块**（板内无加密 IC，HARDWARE_VERIFICATION §六.B3，隔离侧经排针出）；②触发角→12° 脉冲波形生成机制 **已固件破解**（§4k：TIMER1 扫描 6 窗口触发序列，mode1/mode2 两组，60° 双脉冲确认；12° 脉宽=载波+窗口边界 1160μs 长导通，波形细节仍留示波器实测，HARDWARE_VERIFICATION §四.1 待测项已回应）。

8. ✅ **BOM 芯片清单印证（2026-08-20，用户提供 `doc/PC6M-10-BOM-更新版.xlsx`）** — 全部命中并细化（HARDWARE_VERIFICATION §六）：AT24C02C/LPC1765/HEF40106×2/FR120N×6/ULN2003A/ADuM1201/KMB419-301S×6/继电器×3/12MHz 全确认；**FR120N×6+KMB419-301S×6=仅主桥 6 路，P12 为逻辑输出经 HEF40106 到 P11 排针**；**ADuM1201 远程端不在本板**；电源树 PE5420→KBP310/MB6S→LM2575S-5.0→AZ1117H-3.3(3.3V MCU)；新器件 RS8552XM(高精度运放，Uf 调理)、LM2904×3、LM2901(比较器)、CJ431(基准)；采样 1W 0.1R×3(IA/IB/IC)+运放→ADC0(呼应 reg41-43)。

9. ✅ **全部 FUN_ 函数命名清零（2026-08-20 晚）** — 剩余 16 个零文档 FUN_ 全部反汇编命名（Ghidra 库零 FUN_ 残留，PROGRESS §4l/§5b/§5c）：输入去抖组（scan_run_stop P0.28/27 双模式返回7/8、debounce P0.9/P1.16/P1.17/P0.6、chk_p02_p03 双低联锁）、显示格式化（disp_number3/decimal1/signed_angle=±60°主从偏移/screen_calib）、freq_adjust_sync（菜单频率69..688→芯片 reg C9/CA）、uart3_tx_byte（RS485 DE=P1.29）/rx_timeout_monitor、out_relay_p020/21、IAR 运行时（iar_program_start/init_core/get_initial_sp）。**固件逆向：主控制路径 100% 理解，整体 ~95%**。
10. ✅ **显示面板菜单树→固件参数精确映射（任务#16，2026-08-20）** — 全量确证，详见 `MENU_PARAMETER_MAPPING.md`。**基本参数实为 16 屏**（手册仅 12，多出 12急停/13反馈/14输入/15起始相位=0x10001657/59/5A/60）；保护 10 屏全对齐（过压/欠压/IF过载/CT过载+时间=0x100016C0..DC，缺相=DD、三相平衡=DE）；通讯 4 屏；PID 9 屏（档+P/I仅自定+D自动+隐藏闭环分区 0x10001722-26）；相位校准=0x10001694；恢复出厂=密码门控 0x10001746。**旧标修正**：gain_a/b=电压/电流量程、out_fine=主从偏移、sub_state=控制方式、reg12=启动方式、reg13-20=保护参数、缺相/三相平衡地址、reg62=起始相位（=输出下限）。

---

## 📌 关键符号速查

### 核心 RAM 变量
| 地址 | 名 | 说明 |
|---|---|---|
| 0x10001624 | out_param | 输出参数字（位标志） |
| 0x10001628 | cfg_word | 配置/子状态标志 |
| 0x10001634 | gain_sel | 增益档（0/1/2=恒压/恒流/开环） |
| 0x10001638/0x3C | gain_b / gain_a | **电流量程 / 电压量程**（基本菜单屏2/屏1） |
| 0x100016FF | slave_addr | 从站地址 |
| 0x1000170E/0x0F | 活动增益对 | 本地工作拷贝（不同步芯片），输出计算用 |
| 0x10001710 | cfg_1710 | 参数组选择 1..4 |
| 0x10001711-18 | cfg_1711-1718 | 4 组增益槽（同步芯片 reg 0x5B-0x62） |
| 0x10001698/0xA0/0xA8/0xB0/0xB8 | ADC 标定除数 ch2/1/0/3/4 | 3500..4500，芯片 reg 0xCB-0xD4 |
| 0x1000165C | reg61 远程输出使能 | 值→P0.20（FIO0SET/CLR） |
| 0x10001660 | reg62 起始相位/输出下限 | 基本菜单屏15（≤180）；下限=180-值（0x10002004），reg9 上限 |
| 0x10001788 | src_value | Modbus reg40 写落点（远程显示源值，≤1000） |
| 0x100017AC | scratch | 0x10 写多对 reg27-32/37/38/44-46 的无效果落点 |
| 0x100017A8 | cfg_17a8 | 0x10 写多当前寄存器号（start-1+idx） |
| 0x10001722/0x23 | 闭环误差分区阈值上/下限 | 芯片 reg 0x6E/0x6F |
| 0x10001724/0x25/0x26 | 闭环三区增益（大/中/小误差） | 芯片 reg 0x70/0x71/0x72 |
| 0x1000212C | 闭环缓存输出 | FUN_00010f0a 返回值（→out_setpoint） |
| 0x10002130 | 闭环积分累加器 | 钳位 [0x5CC60, 0x116520] |
| 0x10002000 | 软起动相位 | 0→4→5（FUN_0000f9aa 启停直置 0/5） |
| 0x10001FF8 | freq_hz | 运行频率（50/60Hz） |
| 0x10001FFC | out_setpoint | 输出设定点 |
| 0x100020CC | out_scale | 输出缩放（EINT3 输出公式减项，钳位 0x2730/0x23A0/0x3903/0x31E0） |
| 0x1000205C | out_div | 输出分频（除数 0x22D/0x1FB/0x32B/0x2C5） |
| 0x1000162C | out_freq_adj | 输出频率粗调（菜单 69..688，芯片 reg 0xC9/CA，×10 进输出公式） |
| 0x10001654 | out_fine | **主从偏移**（基本菜单屏10，40..160=100±60°，×K=56/51 进触发角公式 = 相移实现 + 认证挑战源） |
| 0x1000165B | out_phase | 输出相位组合选择（0..3） |
| 0x10002075 | mode_byte | 模式锁存（EINT3 当 input_locked==0 时写 1/2） |
| 0x10002076 | input_state | EINT1/2/3 输入状态锁存 |
| 0x10002000 | input_locked | 输入锁相/相位（0..7，闭环启动后 2..7） |
| 0x10001FF9 | phase_cnt | 输入过零周期计数（频率监测） |
| 0x10001785 | run_flag | 运行/停机 |

### 关键函数
| 地址 | 名 | 说明 |
|---|---|---|
| 0x35F2 | FUN_000035f2 | **参数影子同步器（61 组，live→芯片）** |
| 0x25DC | load_config | 配置加载/初始化（双银行） |
| 0x458C | state_machine | 菜单渲染/事件分发 |
| 0xAC24 | uart3_init | UART3 初始化 |
| 0xAF64 | FUN_0000af64 | CRC-16 计算 |
| 0xAF94 | FUN_0000af94 | **Modbus 读寄存器（switch 63 项）** |
| 0xB2E0 | FUN_0000b2e0 | Modbus 写多寄存器 |
| 0xB642 | FUN_0000b642 | **Modbus 主分派器（53 写 handler）** |
| 0x1E88 / 0x1EBC | i2c_write_reg / i2c_read_reg | I2C 芯片访问 |
| 0x10F0A | FUN_00010f0a | 闭环门控包装（调 FUN_000108b0，缓存 0x1000212C） |
| 0x108B0 | FUN_000108b0 | 闭环积分滤波（分区选增益 + 积分限幅 → 0x10002130） |
| 0xF9AA | FUN_0000f9aa | 启停预设（cfg_word 切换时置 out_setpoint/相位/停关引脚） |
| 0xFA2C | EINT3_IRQHandler | **输出定时编程 ISR**：模式锁存+频率监测+编程 TIMER2.MR0（§4j） |
| 0xFF48 | TIMER2_IRQHandler | 触发角到点：清/复位 TIMER2 + scan=0 + 启动 TIMER1（§4j.D） |
| 0xFF6C | TIMER1_IRQHandler | **SCR 触发脉冲生成**：scan 0..240 步，6 窗口×40 步，偶 SET/奇 CLR 驱动 G1-G6+P12-G1~G6，窗口边界 MR0=1160/609μs（§4j.E/§4k） |
| 0x29A | TIMER0_IRQHandler | 系统节拍：tick_ready/phase_cnt/tick_countdown |
| 0xE79A | FUN_0000e79a | gpio_outputs_set()：显示段全置高安全态 |
| 0xE966/0xE946 | fio0/fio1_pin22_ctrl | P0.22/P1.22 停机线 |
