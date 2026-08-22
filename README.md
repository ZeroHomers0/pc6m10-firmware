# LPC1765FBD100 固件反编译导出说明

**目标**：`D:\code\LPC1765FBD100\LPC1765.bin`（NXP LPC1765 / Cortex-M3）
**硬件**：PC6M-10 三相晶闸管（SCR）移相触发功率控制板（SINE POWER / ST33C 变频电源）
**工具**：Ghidra + MCP（`list_functions` 全量 105 个函数；逐一 `decompile_function`）

---

## 一、导出方法（反编译保真约定）

- **反编译结果原样保留**，不改写逻辑；`<...>` 为理解注释。
- Ghidra 符号（`DAT_0000xxxx` / `PTR_xxxx`）原样保留，含义见各文件头注释。
- 每个函数以 `/* 0x0000XXXX —— 名称 */` 标注真实入口地址，便于在 Ghidra 中交叉核对。
- **MCP 超时说明**：Ghidra MCP 读取超时 5s、单线程。两个超大函数（
  `modbus_dispatch` 0xB642、`state_machine` 0x458C）的 C 反编译结果超过传输上限，
  连续反编译均超时；已改走**反汇编精读**：`disassemble_function` 全量指令落盘为
  `.txt` 附录，并在对应 `.c` 文件中给出流程还原伪代码 + 关键代码段注释。

## 二、模块布局

| 文件 | 函数数 | 内容 |
|---|---|---|
| `01_startup.c` | 20 | IAR 运行时 + `main` 完整启动序列（SystemInit→pin_config→gpio1_init→load_config→auth_challenge×3→disp_splash_screen→auth_retry→wdt_init(200)→主循环） |
| `02_lcd_display.c` | 22 | 12864 LCD 动态扫描、字符/数字/字符串渲染、菜单屏 |
| `03_input_debounce.c` | 10 | 按键/输入消抖（debounce_p06/p09/p116/p117、scan_run_stop 等） |
| `04_i2c.c` | 9 | I2C 位带模拟（SDA=P0.10、SCL=P0.11、EEPROM 芯片@0x53，byte 寻址 AT24C02C） |
| `05_adc.c` | 4 | ADC0 多通道采集 |
| `06_param_system.c` | 2 | `load_config`（银行 A/B 魔数校验、默认值、shadow→live 拷贝、增益对选择）、`param_sync_live_to_eeprom`（逐参数比对写回） |
| `07_state_machine.c` | 2+3 | `freq_adjust_sync` + `state_machine`（主菜单状态机，**C 级还原**，18 case 写码；入口 0x458C 事件码→菜单页分发链）+ static 辅助 `sm3_draw_item/sm4_draw_value/sm6_delay_loop` |
| `08_uart3_modbus.c` | 8 | UART3 初始化/收发/CRC16/读写寄存器（`uart3_init/uart3_tx_byte/uart3_rx_timeout_monitor/crc16/modbus_read_reg/modbus_write_multi`） |
| `08_modbus_dispatch.c` | 1 | `modbus_dispatch`（帧解析分发，**C 级还原**，51 写分支结构；入口 0xB642） |
| `crc16_table.c` | — | CRC16 双表内嵌 `const` 数组（`crc16_hi_tbl/crc16_lo_tbl`，原 flash 0x11034/0x11134，bug#S9 修复） |
| `strpool.c` | 1 | 字符串池 GBK blob + `strpool_map`（flash 地址→映射，W7a） |
| `09_output_stage.c` | 16 | 引脚配置、定时器、外部中断、`output_stage`（SCR 移相触发核心：软起停/闭环/保护） |
| `10_relay_led.c` | 5 | 继电器/LED 输出（P0.20/P0.21/P1.20/P1.21/P1.23） |
| `11_auth.c` | 3 | 1-Wire 认证（24 位挑战/16 位应答，P2.1/2/3/4） |
| `12_closed_loop.c` | 2 | PID 闭环（误差死区 + 分段除数表 + 位置式累加钳位） |
| `13_gpio_init.c` | 2 | NVIC IRQ 使能、GPIO2（1-Wire 口）初始化 |
| `07_state_machine_asm.txt` | — | `state_machine` 全量反汇编（10061 条指令）附录 |
| `08_modbus_dispatch_asm.txt` | — | `modbus_dispatch` 全量反汇编（5161 条指令）附录 |

## 三、关键内存映射速查

```
0x2009C000  FIO 池基址（+0x18=FIO0SET +0x1C=FIO0CLR +0x20=FIO0DIR
            +0x38=FIO1SET +0x3C=FIO1CLR +0x40=FIO1DIR
            +0x54=FIO2PIN +0x58=FIO2SET +0x5C=FIO2CLR +0x80=FIO3DIR...）
0x40008000  TIMER1；0x40090000 TIMER2；0x4009C000 UART3
0x4002C000  PINSEL（+0x10=PINSEL4：P2.11/12/13=EINT1/2/3，2026-08-21 复核）
0x400FC000  SCB（+0x140=EXTINT +0x148=EXTMODE +0x14C=EXTPOLAR；+0xC4=PCONP）
0xE000E100  NVIC ISER（-0x1FFF1F00 同址）
0x1000xxxx  反编译 DAT 全局区（RAM 镜像/内部 SRAM）
```

## 四、功能结论（与 HARDWARE_VERIFICATION_2026-08-20.md 对照）

- **触发 GPIO**：P0.15-19、P1.5-9、P2.5-9 为 SCR 触发组；P0.22=运行继电器 RLY1、P1.22=触发/运行指示（2026-08-21 复核）。
- **显示**：12864 LCD 由 TIMER1 ISR 动态扫描（COM/SEG 分 6 区域逐行点亮）。
- **认证链路**：ADuM1201 隔离 1-Wire（P2.1 数据出 / P2.2 数据入 / P2.3 时钟 / P2.4 复位），
  24 位挑战字节由 RAM 值异或/或/与组合生成，应答 16 位比对，失败最多重试 5 次。
- **Modbus RTU**：UART3，从站地址 0x1000B988，功能码 0x03/0x06/0x10；
  读/写寄存器映射见 `08_uart3_modbus.c` 中 `modbus_read_reg` / `modbus_write_multi`；
  写参数后 `param_sync_live_to_eeprom` 写回 EEPROM。
- **PID 闭环**：三路输出（第一路 0x1000F268 状态机、第二路 0x1000F2D4 使能、
  恒压源 0x1000F770），触发角 180°-当前角，系数 0x18BD，软起步进表 0x2C88。
- **运行计时**：运行中每 120s/300s 触发一次参数 EEPROM 同步。
- **菜单**：主菜单状态机 405 处 `disp_string` 渲染，事件码 1-0xE 由状态标志位映射，
  全量菜单→参数映射见 `MENU_PARAMETER_MAPPING.md`。
- **交叉引用差异（13 模块头注 ⚠ 标注，2026-08-21）**：
  ① EINT1/2/3 经 PINSEL 复用为 **P2.11/12/13**（头注默认 P0.22/23/24 为误，P0.22 实为运行继电器 RLY1）；
  ② LCD **P1.25/26=CS1/CS2、P1.27=RS、P1.28=E、P1.24=背光**（头注 P1.24=CS/P1.25=RES 为误）；
  ③ 继电器 **RLY1=P0.22（运行）、RLY3=P0.20（备用）**（软件"继电器1"对应 P0.22）。

## 五、目录结构（2026-08-21 整理后）

```
decompiled/
├── CLAUDE.md                      ← 项目自包含索引（入口，任何 AI 接手先读）
├── PROJECT_SUMMARY_2026-08-21.md  ← 项目全史（开始→过程→结果）
├── APPLICATION_GUIDE_2026-08-21.md ← 应用指南（逆向结论怎么用）
├── DATA_SEGMENT_2026-08-21.md     ← 数据段清单（目标A② / W2 地基）
├── WORK_GUIDE_2026-08-21.md       ← 工作指导（W1-W8 / 目标 A/B）
├── LPC1765.bin                    ← 原始固件
├── 01_startup.c … 13_gpio_init.c  ← 反编译源码（13 模块存档版）
├── firmware/                      ← 可编译工程（build.sh → firmware.elf/hex/bin；src/ 16 模块 + inc/ + lpc1765.ld）
│   ├── src/                       ←   16 个 .c（01-13 + 08_modbus_dispatch + crc16_table + strpool）
│   ├── inc/                       ←   types.h / globals.h / reg.h / consts.h
│   └── stub.c                     ←   根残留：func_0x0000aed0 骨架 + freq_adjust_sync 实现
├── 07_state_machine_asm.txt       ← state_machine 全量反汇编（10061 条）
├── 08_modbus_dispatch_asm.txt     ← modbus_dispatch 全量反汇编（5161 条）
├── docs/                          ← 根目录迁移的过程文档
│   ├── PROGRESS_2026-08-20.md     ←   防丢失主文档（63 寄存器表/61 组同步全图）
│   ├── PLAN.md                    ←   进度计划 + 关键符号速查
│   ├── HARDWARE_VERIFICATION_2026-08-20.md（硬件印证）
│   ├── MENU_PARAMETER_MAPPING.md（菜单→参数全量映射）
│   ├── i2c_param_sync.md / uart3_protocol.md / state_machine_analysis.md
│   ├── *disasm.txt / *flow.txt    ←   早期指令级转储（历史产物）
│   └── 配置.png
└── tools/                         ← Ghidra 辅助脚本（.java/.py）
```

**接手顺序**：`CLAUDE.md` → `PROJECT_SUMMARY_2026-08-21.md` → 本文档 → `WORK_GUIDE_2026-08-21.md` → `docs/` 细节。

## 六、导出日期

2026-08-21。函数清单以 Ghidra `list_functions` 实际结果为准（105 个，非早期估计的 122）。
