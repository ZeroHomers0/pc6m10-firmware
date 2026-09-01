# AGENTS.md — LPC1765FBD100 固件逆向工程

> 本文件是 AI 接手项目的唯一权威上下文。文档导航见 `DOCUMENTATION_INDEX.md`。

## 项目与当前状态

目标设备为 PC6M-10 三相 SCR 移相触发功率控制板，MCU 为 NXP LPC1765。原始固件
`LPC1765.bin`（262144 B）是所有等价验证的金标准。

- GCC 可编译工程：`firmware/`，Arm GNU Toolchain 14.2.Rel1。
- 当前构建：`text 62840 / data 3000 / bss 2188`。
- 当前 `firmware.bin` SHA-256：`C6D3F35DD6C5A451947C27BA1825D8CE35C43D031F4EA02E32A9438BB32AE74E`。
- 自编译固件已于 2026-08-26 完成 CRP 布局修复（`0x2FC` 显式保留 `0xFFFFFFFF`，见
  `firmware/lpc1765.ld`、`firmware/startup.s`、`firmware/build.sh`）并经 J-Link SWD 烧写入板、
  全镜像校验通过；过程见 `docs/w8/W8_POST_FLASH_2026-08-26.md`。
- 测试：静态测试 `run_tests.py` 25/25；A/B 矩阵全 PASS（TIMER1 126 / 输出级 144 /
  状态机 130 / 显示 106 + 全执行 4 / Modbus 65 读 + 320 写）。
- **2026-09-02 实机测试通过（用户直接实机验证）**：自编译固件已烧写入板并经用户实机测试
  通过（含产品信息定制、X/O 字形修复与电话行宽修正后的最终固件）。**W8 分级实测流程
  未按计划执行**——用户未按 W8 分级（L1/L2/L3、示波器波形/标定/带载）进行测试，该分级
  流程已废弃；`docs/w8/` 下 W8 分级文档仅保留为历史流程档案，进度一律以本文档与
  `DOCUMENTATION_INDEX.md` 为准。实机通过不代表分级波形/带载 100% 量化等价。
- **2026-09-01 产品信息定制**（用户要求）：case9「产品版本信息」屏 4 行文本覆写为
  型号:PC6M-10 / 版本:V2.0 / 厂商:XIANPOWER / 电话:02984205750（原厂
  ST33C / V2.0.2016 / SINEP0WER / 18938061832）。实现：`tools/generation/generate_string_pool.py`
  新增 `PRODUCT_INFO_OVERRIDES`（0x6b78/0x6b84/0x6b94/0x6ba4），重新生成 `firmware/src/strpool.c`
  得 `strpool_override` 段，`strpool_map` 前置查表（flash 地址不变，仅替换显示内容；
  原厂串仍保留于 blob）。重建固件后六相 A/B 验证全 PASS（含 DISPLAY_MATRIX 106 /
  DISPLAY_FULL_EXEC 4）。注：本仓对 PC12M-2 仅作只读参考，本次改动为用户对六相固件的明确授权。
- **2026-09-01 厂商 X/O 显示修复**（用户要求，分支 `w8-debug-2026-09-01`，与 PC12M-2 方案C 同款）：
  原 BIN 8×8 字库仅 36 字符（map @0x10000BB8 无 X/O），"厂商:XIANPOWER" 的 X/O 查表未命中
  渲染空白。修复：`02_lcd_display.c` 新增 `ext_char8_x[0x10]` 扩展 X 字形，O 复用原字库数字 0，
  其余查原字库（字库外字符空白保留）。`verify_firmware_equivalence.py` 新增
  `XO_FONT`：OLD 'X' 0 写 / NEW 'X'·'O' 各 387 写 / NEW 'Z' 0 写；A/B 全套 PASS。
  详见 `docs/w8/W8_ISSUE_FIX_2026-09-01.md`。注意：此为**有意定制偏离**（原 BIN 渲染 X/O 空白），
  仅"渲染 X/O"的 GPIO 写迹与 OLD 不同。本次改动为用户对六相固件的明确授权。
- **2026-09-02 产品信息电话行宽修正**（用户要求）：菜单电话值仅容 11 个 ASCII 字符，
  `029-84205750`（12 字符）末尾 `0` 被截断，去连字符改为 `02984205750`。重新生成
  `strpool.c` 并重建固件（text 62840 / data 3000 / bss 2188，SHA-256 `C6D3F35D…AE74`），
  六相 A/B 全 PASS、`run_tests.py` 25/25。详见 `docs/w8/W8_ISSUE_FIX_2026-09-01.md`。

## 必须遵守

- 全程中文交流。
- 反编译、反汇编和硬件原始资料属于证据，不得因为当前未引用而删除。
- 修改源码后必须以原始 BIN 做 A/B 执行级验证，不能仅依赖手写模型。
- J-Link 一律调用仓库打包版 `tools/jlink/JLink.exe`（免安装），禁止依赖本机其他安装路径；
  工具链由 `firmware/build.sh` 自动探测（约定版本优先）。
- 上机操作按 `操作文档.md` 执行；W8 分级实测流程已废弃（用户未按分级测试，实机已直接通过）。
  硬件安全仍须遵守 `docs/w8/W8_HARDWARE_TEST_2026-08-22.md`：断开市电/门极/功率负载、
  仅限流控制电、不跨级带载。
- `docs/history/` 只保存历史结论；当前状态以本文件、`DOCUMENTATION_INDEX.md` 为准。
- 更新 `docs/w8/` 文档一律按「W8 文档职责分工」落位：进度/状态只维护在 `W8_TEST_MASTER.md`；
  硬件事实改 `W8_HARDWARE_TEST_2026-08-22.md`；软件/仪器操作改 `W8_SOFTWARE_OPERATION.md`；
  带日期文档只记录当时事实、不当作现状来源。

## 目录职责

```text
LPC1765.bin                 原始固件金标准
firmware/                   当前可编译、可修改工程
docs/project/               当前应用、数据段和项目状态
docs/analysis/              模块级逆向结论
docs/w8/                    实机历史档案：W8 分级实测已废弃（实机已直接通过），W8_TEST_MASTER 仅作历史流程档案
docs/history/               历史计划、进度与审计
evidence/hardware/          硬件证据索引；board/ 整板、display/ 面板、reports/ 分析报告
evidence/reverse/           原始反编译、反汇编和过程报告
test/                       静态与 Unicorn 执行级测试
tools/                      审计、生成、Ghidra、维护、验证和 W8 工具；jlink/ 为打包 J-Link
```

## W8 文档职责分工

| 文档 | 职责（更新文档时的落位） |
|---|---|
| `W8_TEST_MASTER.md` | **历史流程档案**：顶部已加 2026-09-02 废弃说明，不再维护进度；阶段 0-4 表保留为历史 |
| `W8_HARDWARE_TEST_2026-08-22.md` | 硬件安全规范：安全前提、P12 引脚/触发引脚、时序表（无进度，仍为安全参考） |
| `W8_SOFTWARE_OPERATION.md` | 软件操作手册：软件清单/安装/示波器·信号源操作（活文档）；构建与烧写命令见 `操作文档.md` |
| `W8_ONBOARDING_2026-08-22.md` | 历史导航入口 + 停止线 |
| 其余带日期文档 | 记录：问题时间线/专项排查/阶段执行步骤，只记录当时事实 |

更新规则：
- 当前进度与结论 → 只改 `AGENTS.md` 与 `DOCUMENTATION_INDEX.md`；W8 分级文档不再维护进度。
- 硬件安全事实变化 → 改 `W8_HARDWARE_TEST_2026-08-22.md` 与 AGENTS.md「已确证硬件事实」。
- 软件清单/安装/仪器操作变化 → 改 `W8_SOFTWARE_OPERATION.md`；构建与烧写命令变化 → 改根目录 `操作文档.md`（命令唯一源，不双源维护）。
- 带日期文档：不删不改写历史，可追加新事实；不得作为「当前状态」来源。

## 常用入口

后续逆向或同系列项目迁移，先读 `docs/analysis/REVERSE_ENGINEERING_AI_GUIDE.md`；该文档汇总
PC6M/PC12M 的最优证据链、A/B 差分方法、历史 bug、验证门禁、项目结构和构建烧写规范。

操作入口（详见 `操作文档.md`；根目录三个 `.bat` 已删除，跨机不通用）：
- 构建 —— `cd firmware && bash build.sh`（产物 `firmware.bin/hex/elf/map`，SHA/尺寸见 操作文档.md §2）
- SWD 烧写（日常主通道，首选）—— 打包版 `tools/jlink/JLink.exe` + `-CommanderScript`，命令见 操作文档.md §3
- ISP 烧写（SWD 连不上时的解困通道）—— Flash Magic + USB-TTL，接线与用法见 操作文档.md §4
- 重启固件一律**物理断电再上电**（J-Link 驱动 nRESET 复位会悬挂 SWD，干扰复用调试脚的固件运行）。

```powershell
# 手动构建（Git Bash 中执行 build.sh）
cd firmware
bash build.sh

# 全部测试
cd ..
python test/run_tests.py

# 独立原 BIN / 新 ELF 验证
python tools/verification/verify_firmware_equivalence.py
```

## 已确证硬件事实

- G1-G6=P0.17/P0.15/P0.18、P2.9/P0.19/P0.16；12 脉波扩展=P2.8/P2.7/P2.6/P2.5、P0.8/P0.7。
  （2026-08-27 修正：G5=P0.19、G6=P0.16，非 P2.19/P2.16——据 pin_config/TIMER1 ISR 位写 +
   HARDWARE_VERIFICATION_2026-08-20 §二.2 + PROGRESS_2026-08-20 §4k 三处一致。）
- EINT1/2/3=P2.11/P2.12/P2.13；TIMER2 编程触发角（单次延时），TIMER1 240 步扫描生成 6 窗口
  触发脉冲序列（非显示扫描）；封锁安全态=gpio_outputs_set() 全部触发脚置高。
- EEPROM 为 AT24C02C @0x53，GPIO 模拟 I2C：SDA=P0.10、SCL=P0.11。
- UART3 经 ADM2483 实现 Modbus RTU，从站支持 0x03/0x06/0x10，寄存器 1..63。
- P0.20=RLY3 备用、P0.21=RLY2 报警、P0.22=RLY1 运行；P1.20..23 为状态 LED。
- P12 SWD：1=VTref、2=GND、3=P2.10(ISP 引导，低进 bootloader)、6=SWDIO、7=nRESET、8=SWCLK。
  P12-7 是复位、P12-3 是 ISP 引导脚，均已在排针引出（2026-08-26 据实机排针确认）。
- SWD 调试脚被固件复用（2026-08-26 定论，证据见 `docs/w8/W8_JLINK_DEBUG_2026-08-24.md` §3.1）：
  P1.30(SWDIO)→AD0.4 电压反馈（`adc_init`，PINSEL3[29:28]=3）、P1.29(SWCLK)→RS485 DE/RE（`uart3_init`）。
  两者在 `main()` 初始化即执行 → 出厂固件 SWD 连不上属设计冲突，非 J-Link 问题。
  进调试需 connect-under-reset（另接 nRESET）；烧录/备份可走 ISP（P2.10 拉低 + UART0 P0.0/P0.1）。

## 关键限制与下一步

Ghidra MCP 对超大函数有 5 秒超时；完整指令证据已保存于 `evidence/reverse/disassembly/`。
**实机测试已通过**（用户直接实机验证，2026-09-02 定稿）；W8 分级实测流程未按计划执行、已废弃，
`docs/w8/W8_TEST_MASTER.md` 仅作历史流程档案。后续无强制 W8 步骤：若需发布/量产/换板复验，
按 `操作文档.md` 重建并烧写，实机冒烟核对 LCD/按键/通信即可；分级波形量化与强电带载如需补充，
可按 `docs/w8/W8_HARDWARE_TEST_2026-08-22.md` 的规范另行安排。
