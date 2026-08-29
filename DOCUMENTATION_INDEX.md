# 文档索引

> 最后更新：2026-08-30。当前进度优先级：`AGENTS.md` → 本文 → `docs/project/PROJECT_STATUS.md` → 历史文档。

## 当前文档

| 主题 | 文档 |
|---|---|
| 构建与烧写操作 | `操作文档.md`（构建命令 + ISP 烧写 + SWD 烧写） |
| 项目状态 | `docs/project/PROJECT_STATUS.md` |
| 应用指南 | `docs/project/APPLICATION_GUIDE_2026-08-21.md` |
| 数据段 | `docs/project/DATA_SEGMENT_2026-08-21.md` |
| 硬件印证 | `docs/analysis/HARDWARE_VERIFICATION_2026-08-20.md` |
| I2C/参数 | `docs/analysis/I2C_PARAM_SYNC.md` |
| UART3/Modbus | `docs/analysis/UART3_PROTOCOL.md` |
| 状态机 | `docs/analysis/STATE_MACHINE_ANALYSIS.md` |
| 菜单参数 | `docs/analysis/MENU_PARAMETER_MAPPING.md` |

## W8 实机验证

按顺序阅读（进度与完成状态一律看第 1 项 `W8_TEST_MASTER.md`；文档职责分工见 `AGENTS.md`「W8 文档职责分工」）：

1. `docs/w8/W8_TEST_MASTER.md`（**唯一流程 + 进度权威**：阶段 0-4 必检项状态、通过标准、下一步、记录索引）
2. `docs/w8/W8_ONBOARDING_2026-08-22.md`
3. `docs/w8/W8_PRE_HARDWARE_VALIDATION_2026-08-23.md`
4. `docs/w8/W8_HARDWARE_TEST_2026-08-22.md`
5. `docs/w8/W8_SOFTWARE_OPERATION.md`
6. `docs/w8/W8_JLINK_DEBUG_2026-08-24.md`
7. `docs/w8/W8_ISP_FLASH_2026-08-26.md`（ISP 擦除、SWD 恢复、自编译固件 CRP 风险与实机经验）
8. `docs/w8/W8_POST_FLASH_2026-08-26.md`（自编译固件 CRP 修复后 SWD 烧写入板、MEMMAP 排查与板上状态）
9. `docs/w8/W8_DISP_SEL_FIX_2026-08-27.md`（case1 首页上下键无效：三分支 FAULT 门控修复 + DISP_SEL=控制方式 破译）
10. `docs/w8/W8_ISSUE_LOG_2026-08-27.md`（烧录测试全流程问题回顾时间线，**暂未完结**，随调试持续追加）
11. `docs/w8/W8_STAGE1_CLOSE_STAGE2_2026-08-27.md`（阶段1 收尾：SRAM 哨兵栈水位 + 继电器误吸合实机步骤；阶段2 三相同步启动清单）
12. `docs/w8/W8_ISSUE_FIX_2026-08-28.md`（问题修复：case3 编辑态不闪烁 + 恒流 LED/主屏开环 LED 错译）
13. `docs/w8/W8_ISSUE_FIX_2026-08-30.md`（问题修复汇总：6 项实机问题——不闪烁/恢复出厂"M"/Modbus 大端/PID BY DESIGN/控制方式互斥/复位"重启"字样）

## 证据与历史

- 原始硬件证据：`evidence/hardware/README.md`。
- 原始反编译和反汇编：`evidence/README.md`。
- 历史计划、项目全史和时间点审计：`docs/history/`；仅供追溯。
- 目录用途总览：`docs/README.md`、`tools/README.md`、`test/README.md`。
