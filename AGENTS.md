# AGENTS.md — LPC1765FBD100 固件逆向工程

> 本文件是 AI 接手项目的唯一权威上下文。文档导航见 `DOCUMENTATION_INDEX.md`。

## 项目与当前状态

目标设备为 PC6M-10 三相 SCR 移相触发功率控制板，MCU 为 NXP LPC1765。原始固件
`LPC1765.bin`（262144 B）是所有等价验证的金标准。

- GCC 可编译工程：`firmware/`，Arm GNU Toolchain 14.2.Rel1。
- 当前构建：`text 61968 / data 3000 / bss 2188`。
- 当前 `firmware.bin` SHA-256：`25676E62317091EA33D054D404E0D2C4E2C988C8CE4969E04E70395FC0A49C62`。
- 自编译固件已于 2026-08-26 完成 CRP 布局修复（`0x2FC` 显式保留 `0xFFFFFFFF`，见
  `firmware/lpc1765.ld`、`firmware/startup.s`、`firmware/build.sh`）并经 J-Link SWD 烧写入板、
  全镜像校验通过；过程见 `docs/w8/W8_POST_FLASH_2026-08-26.md`。
- 测试：11/11 模块；输出级 144/144、状态机 115/115、TIMER1 126 例、Modbus 65 读/320 写均 PASS。
- 离线结果允许进入断开门极与功率负载的 W8 分级实测，不代表带载 100% 等价。

## 必须遵守

- 全程中文交流。
- 反编译、反汇编和硬件原始资料属于证据，不得因为当前未引用而删除。
- 修改源码后必须以原始 BIN 做 A/B 执行级验证，不能仅依赖手写模型。
- J-Link 一律调用仓库打包版 `tools/jlink/JLink.exe`（免安装），禁止依赖本机其他安装路径；
  工具链由 `build.bat`/`build.sh` 自动探测（约定版本优先）。
- 上机严格执行 `docs/w8/W8_HARDWARE_TEST_2026-08-22.md`，不得跨级带载。
- `docs/history/` 只保存历史结论；当前状态以本文件、`DOCUMENTATION_INDEX.md` 和 W8 预验证记录为准。

## 目录职责

```text
LPC1765.bin                 原始固件金标准
firmware/                   当前可编译、可修改工程
docs/project/               当前应用、数据段和项目状态
docs/analysis/              模块级逆向结论
docs/w8/                    实机验证四份权威文档
docs/history/               历史计划、进度与审计
evidence/hardware/          BOM、接线表、手册等原始硬件证据
evidence/reverse/           原始反编译、反汇编和过程报告
test/                       静态与 Unicorn 执行级测试
tools/                      审计、生成、Ghidra、维护、验证和 W8 工具
```

## 常用入口

```powershell
# 构建（Git Bash 中执行 build.sh）
cd firmware
bash build.sh

# 全部测试
cd ..
python test/run_tests.py

# 独立原 BIN / 新 ELF 验证
python tools/verification/verify_firmware_equivalence.py
```

## 已确证硬件事实

- G1-G6=P0.17/P0.15/P0.18、P2.9/P2.19/P2.16；12 脉波扩展=P2.8/P2.7/P2.6/P2.5、P0.8/P0.7。
- EINT1/2/3=P2.11/P2.12/P2.13；TIMER2 编程触发角，TIMER1 240 步扫描输出触发窗口。
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
当前下一步只有 W8：原固件已备份、新固件已烧写，先做控制电冒烟，再做三相空载波形、
Modbus/ADC 标定、低压限流，最后才评估带载。
