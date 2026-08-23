# AGENTS.md — LPC1765FBD100 固件逆向工程

> 本文件是本项目的**自包含索引**。任何 AI 在任意对话中接手本项目，先读本文件即可快速恢复上下文并继续。
> 配套文档：`PROJECT_SUMMARY_2026-08-21.md`（全史）、`README.md`（模块布局）、`APPLICATION_GUIDE_2026-08-21.md`（应用指南）、`DATA_SEGMENT_2026-08-21.md`（数据段清单）、`WORK_GUIDE_2026-08-21.md`（工作指导）。

## 项目是什么

对 NXP **LPC1765**（Cortex-M3，256KB flash）固件 `LPC1765.bin` 的**静态逆向工程**。
目标设备：**PC6M-10 三相晶闸管（SCR）移相触发功率控制板**（SINE POWER / ST33C 变频电源，恒压/恒流/开环三模式）。
已完成：105 个函数全部反编译、协议/架构全解析、硬件与菜单映射印证。
可编译、可修改的源码工程已经达成；离线等价验证已覆盖关键路径，完整硬件行为等价仍待 W8 分级实测确认。

## 沟通约定（重要）

- **全程中文交流**（用户使用中文，所有回复必须中文）。
- 需要查固件细节时用 Ghidra MCP 工具（`mcp__ghidra__*`：`decompile_function` / `disassemble_function` / `get_function_xrefs` / `list_strings` 等）。
- 用户说"记下来/记录" = 把结论写入本目录文档或记忆。
- 硬件印证依据 = `docs/doc/` 下的 doc 硬件文档（唯一来源，无独立芯片清单）。其中 `PC6M-10-BOM-更新版.xlsx` 的 BOM 列表已用于芯片级印证（HARDWARE_VERIFICATION §六）。
- 反编译保真约定：保留 Ghidra 原样与 `DAT_0000xxxx`/`PTR_xxxx` 符号；`<...>` 为理解注释。

## 环境

| 项 | 值 |
|---|---|
| 本目录 | `D:\code\LPC1765FBD100\decompiled`（自包含项目档案） |
| 原始固件 | `LPC1765.bin`（同目录副本，262144B） |
| Ghidra 工程 | `D:\code\LPC1765FBD100\LPC1765FBD100.gpr` |
| Ghidra 安装 | `D:\code\LPC1765FBD100\ghidra_12.1.3_PUBLIC_20260817` |
| MCP 服务 | `GhidraMCP-release-1-4`（本地，**读取超时 5s、单线程**） |
| 根目录 | `D:\code\LPC1765FBD100\`（含 `doc/` 硬件文档原件、Ghidra 工具） |

## 目录结构

```
decompiled/
├── AGENTS.md                      ← 本文件（入口）
├── PROJECT_SUMMARY_2026-08-21.md  ← 项目全史（开始→过程→结果）
├── README.md                      ← 模块布局 + 内存映射速查 + 功能结论
├── APPLICATION_GUIDE_2026-08-21.md ← 应用指南（逆向结论怎么用：Modbus/菜单/诊断/定制/实测）
├── DATA_SEGMENT_2026-08-21.md     ← 数据段清单（字符串/查表/指针表/SRAM 全局，目标A②+W2 地基）
├── WORK_GUIDE_2026-08-21.md       ← 评估 + W1-W8 未完成清单 + 目标A/B路线
├── LPC1765.bin                    ← 原始固件
├── 01_startup.c … 13_gpio_init.c  ← 反编译源码（13 模块存档版，函数入口地址在注释）
├── firmware/                      ← 可编译工程（src/ 16 模块 + inc/ + build.sh + lpc1765.ld）
│   ├── src/                       ←   16 个 .c（01-13 + 08_modbus_dispatch + crc16_table + strpool）
│   ├── inc/                       ←   types.h / globals.h / reg.h / consts.h
│   └── stub.c                     ←   根残留：func_0x0000aed0 骨架 + freq_adjust_sync 实现
├── 07_state_machine_asm.txt       ← state_machine 全量反汇编（10061 条）
├── 08_modbus_dispatch_asm.txt     ← modbus_dispatch 全量反汇编（5161 条）
├── docs/                          ← 根目录迁移的过程文档（原样）
│   ├── PROGRESS_2026-08-20.md     ←   防丢失主文档（63 寄存器表/61 组同步全图）
│   ├── PLAN.md                    ←   进度计划 + 关键符号速查
│   ├── HARDWARE_VERIFICATION_2026-08-20.md
│   ├── MENU_PARAMETER_MAPPING.md
│   ├── i2c_param_sync.md / uart3_protocol.md / state_machine_analysis.md
│   ├── *disasm.txt / *flow.txt    ←   早期指令级转储（历史产物）
│   ├── 配置.png
│   └── doc/                       ← doc 硬件文档原件（BOM/原理图/U38接线表/面板手册，8 个文件）
└── tools/                         ← Ghidra 辅助脚本（.java/.py）
```

## 硬件事实（已确证）

- **触发 GPIO**：G1-G6 = P0.17/15/18、P2.9/19/16；12 脉波 P2.8/7/6/5、P0.8/7（12° 脉宽 / 60° 双脉冲）。
- **显示**：12864 图形 LCD（P1 口），TIMER1 ISR 动态扫描（6 区域逐行）。
- **认证链路**：P2.1(出)/P2.2(入)/P2.3(时钟)/P2.4(复位) 经 **ADuM1201** 隔离 1-Wire 挑战-应答，失败重试 5 次锁机。
- **EEPROM @0x53** = AT24C02C（256B 被动参数存储）；I2C 为 **GPIO 位带模拟**（SDA=P0.10、SCL=P0.11）。
- **UART3** = RS485（ADM2483 隔离模块），Modbus RTU 从站。
- **继电器/LED**（2026-08-21 复核）：P0.20=RLY3 备用（reg61 远程使能）/ P0.21=RLY2 报警 / P0.22=RLY1 运行；P1.20-23 状态 LED；P1.22=触发/运行指示。
- **过零输入**：EINT1/2/3 = P2.11/12/13（下降沿）。P0.9 = 24V 交流方波输入。
- **电源树**：PE5420 → KBP310/MB6S → LM2575S-5.0 → AZ1117H-3.3。

## 已完成的结论（速查）

- **105 个函数全部处理**：103 个 C 反编译；`state_machine`(0x458C) 与 `modbus_dispatch`(0xB642) 因超 MCP 5s 上限改走全量反汇编 + 精读还原。
- **Modbus**：功能码 0x03/0x06/0x10，CRC16 poly 0xA001（双 256 表 @0x11034/0x11134），reg 1..63 全解析（读/写不对称、别名：reg24/25、reg27-29、reg40、reg61、reg62）。
- **参数系统**：61 组 live↔shadow↔EEPROM 同步（`param_sync` 0x35F2）；双银行 magic 校验（'U'=0x55 / 'f'=0x66）；4 组增益槽。
- **SCR 触发**：EINT3 每输入过零编程 `TIMER2.MR0`（触发角 = 180°-当前角，软起动 phase 0→4→5，PID 三路闭环）→ TIMER1 240 步扫描生成 6 窗口触发脉冲。
- **保护**：0x1000EDF4 位标志（bit4 过压/bit5 过流/bit3 缺相/bit9 缺相严重级；只置不清锁存），停机斜坡。
- **菜单**：14 位状态标志→事件码 1..0xE；运行中每 120s/300s 参数同步。全量映射见 `docs/MENU_PARAMETER_MAPPING.md`。
- **数据段清单**（目标A②）：`DATA_SEGMENT_2026-08-21.md` 已导出——内存布局/向量表/字符串池 GBK 全表/CRC16 双表+反编译代码/波特率系数表/PID 分段除数/触发常数/switch 跳转表 0xDA6/UART3 指针表 0xB00C-0xB094 全映射/SRAM 全局清单。
- **交叉引用**（目标A③）：13 个 .c 模块头注均加"交叉引用"段指向 docs；发现 3 处引脚/编号差异（EINT1/2/3 实为 P2.11/12/13、LCD CS/RS/E、继电器编号），已 ⚠ 标注待复核。
- **两大函数注释补完**（目标A①）：state_machine 事件码→菜单页分发链（0x100048D8→0x10001744/45/46→参数 RAM）、modbus_dispatch 51 写分支结构（reg 0x10xx/范围校验/8 字节响应）已写入 07/08 模块。
- **理解度**：主控制路径 100%，整体 ~95%。

## 未完成 / 下一步

**目标B 可编译已达成**（2026-08-22）：`firmware/` 工程 `bash build.sh` 零警告产出 `firmware.hex/bin/elf`
（text 61936 / data 3000 / bss 2188）。**W1 已完成**——07/08 两大函数已转为真实 C 级还原
（state_machine 0x458C 18 case 写码 / modbus_dispatch 0xB642 51 写分支）；`stub.c` **保留**于 firmware/ 根
（承载不可入 src 联编的 `func_0x0000aed0` 骨架 + `freq_adjust_sync` 0xAB48 完整实现，链接于 0x1A2），
不再提供 07/08 占位。可读性重构 3 组提交已落地（全局变量语义化 g_ / reg.h 位宽 / consts.h 常量表）。
仓库远端为 `origin`；集成分支为 `master`，同时保留 `main`。当前审查分支为 `codex/decompiler-review`。

- 完整清单：`WORK_GUIDE_2026-08-21.md` 的 **W1-W8**。
- W7 行为等价验证（2026-08-23）：数据层三验全 PASS（SRAM 访问宽度 / modbus 51 写分支 / strpool 字符串映射）；
  手工还原函数 07/08 对金标准**地址双向全覆盖无臆造**（07:125/125/125、08:C-only=0）；06 地址与参数银行布局一致。
- **A/B 差分执行级测试全绿（2026-08-23，已合入 master）**：`test/unicorn_harness.py` 用 Unicorn 真实执行
  【原始 LPC1765.bin】vs【firmware.elf】，同一 RAM 种子下比返回值+内存末态，原始二进制即金标准（无需手抄模型）。
  覆盖全部 A/B 安全叶函数（纯 RAM+flash，无外设 RMW）：`modbus_read_reg`(0xAF94)65/65、`crc16`(0xAF64)6/6、
  `modbus_write_multi`(0xB2E0)12/12、`closed_loop_integral`(0x108B0)10/10、`closed_loop_wrapper`(0x10F0A)4/4。
  测试套件 `test/run_tests.py` 11 模块全绿。**抓到并修复 W7 真 bug×2**：
  ① crc16 `len-1` 语义（原固件处理**全部** len 字节；误读 `sub.w r4,#1` 带 S 置位 → 0xAF84 实为 `movs r0,r4` 置 Z、
  sub.w 无 S 不置位、uxtb 后减）；② closed_loop 误差链符号/无符号（应 SDIV 符号除）。
  **教训**：模型测试能"反编译 C == 手抄模型"双错一致，A/B 才能抓；后续纯逻辑函数首选 A/B 而非模型。
  后续独立矩阵已补齐：`output_stage` 144/144、`state_machine` 115/115，测试套件 11/11 通过。
- 当前 `firmware.bin` SHA-256：`F032EFB70BB3942C4999D7C1F2D0DEBB64125F004C4E19405CAB0DD08F5EAA44`。
- 下一步：W8 分级硬件实测——先断开门极与功率负载完成 SWD/控制电冒烟，再做三相过零、12°/60° 波形、reg44/45 标定，最后才允许低压/带载。
  上机风险分层评估见记忆 `firmware-hardware-risk-assessment`（点亮 70-85%、带载 SCR 40-60%）。
- 目标 A（仅文档化）三项收尾已完成（②数据段清单 / ③交叉引用 / ①两大函数注释），≈100%。

## 关键技术限制（踩过的坑）

1. **Ghidra MCP 5s 超时**：超大函数（>几千条指令）`decompile_function` 会超时。
   对策：用 `disassemble_function` 全量落盘 + Python 脚本分析（BL 目标/调用图/协议逻辑）+ 人工精读还原。
2. **Windows 控制台中文乱码**：Bash 中 Python 打印中文标签乱码（GBK/UTF-8）；数据正确。落盘用 Write 工具（UTF-8）。
3. **BL 目标正则**：须用 `\bbl\s+0x[0-9a-f]+`，避免把 `0x` 里的 `0` 当目标捕获。
4. **旧结论纠错史**：①"无输出外设"错误（实为 TIMER2@0x40090000 编程 SCR 触发角，旧扫描漏 0x40090000）；②"reg62 特殊值{3,63,63}"误读（63 来自 reg≤0x3F 校验、3 是异常码）→ reg62=起始相位/输出下限；③ I2C 是位带模拟非硬件 I2C0。

## 常用内存映射

```
0x2009C000  FIO 池（+0x00 FIO0DIR +0x18 FIO0SET +0x1C FIO0CLR
            +0x20 FIO1DIR +0x38 FIO1SET +0x3C FIO1CLR
            +0x40 FIO2DIR +0x54 FIO2PIN +0x58 FIO2SET +0x5C FIO2CLR
            +0x60 FIO3DIR +0x80 FIO4DIR）
0x40004000  TIMER0（系统节拍）  0x40008000 TIMER1（LCD 扫描）
0x40090000  TIMER2（SCR 触发角定时）  0x4009C000 UART3
0x4002C000  PINSEL（+0x10 PINSEL1）  0x400FC000 SCB（+0xC4 PCONP
            +0x140 EXTINT +0x148 EXTMODE +0x14C EXTPOLAR）
0xE000E100  NVIC ISER（= -0x1FFF1F00）
0x1000xxxx  反编译 DAT 全局区（RAM 镜像/内部 SRAM）
```
