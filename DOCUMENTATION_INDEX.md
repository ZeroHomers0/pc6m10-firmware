# 文档索引

> 最后更新：2026-09-02。当前进度优先级：`AGENTS.md` → 本文 → `docs/project/PROJECT_STATUS.md` → 历史文档。
>
> **2026-09-02 结论**：自编译固件已烧写入板并经用户直接实机测试通过（含产品信息定制、X/O
> 字形修复与电话行宽修正后的最终固件）。**W8 分级实测流程未按计划执行、已废弃**——用户未按
> W8 分级流程测试；`docs/w8/` 下 W8 分级文档保留为历史流程档案，进度以 `AGENTS.md` 与本文为准。

## 当前文档

| 主题 | 文档 |
|---|---|
| 软件设计及开发文档 | `docs/project/PC6M10_软件设计及开发文档_V1.1.docx`（产品背景、软件架构、模块设计、接口、开发、测试与烧写） |
| 软件设计文档生成脚本 | `tools/docs/generate_pc6m_software_design_doc.py`（可重建 Word，支持 `--output` 指定输出路径并执行结构、禁用表述和标题字体检查） |
| AI 逆向与重建总指南 | `docs/analysis/REVERSE_ENGINEERING_AI_GUIDE.md`（项目结构、证据链、A/B 差分方法、构建烧写、历史陷阱） |
| 构建与烧写操作 | `操作文档.md`（构建命令 + ISP 烧写 + SWD 烧写） |
| 项目状态 | `docs/project/PROJECT_STATUS.md` |
| 应用指南 | `docs/project/APPLICATION_GUIDE_2026-08-21.md` |
| 数据段 | `docs/project/DATA_SEGMENT_2026-08-21.md` |
| 硬件印证 | `docs/analysis/HARDWARE_VERIFICATION_2026-08-20.md` |
| I2C/参数 | `docs/analysis/I2C_PARAM_SYNC.md` |
| UART3/Modbus | `docs/analysis/UART3_PROTOCOL.md` |
| 状态机 | `docs/analysis/STATE_MACHINE_ANALYSIS.md` |
| 菜单参数 | `docs/analysis/MENU_PARAMETER_MAPPING.md` |
| PC12M-2 测试覆盖复查（任务 #4，113/113） | `docs/analysis/PC12M2_TEST_COVERAGE_REVIEW.md` |
| PC12M-2 W8 问题复查（任务 #5） | `docs/analysis/PC12M2_W8_ISSUES_REVIEW.md` |

> 说明：`docs/analysis/PC12M2_*` 两份为 PC12M-2（十二相）项目的复查产出，存于本仓
> `docs/analysis/`（用户授权），仅作十二相侧结论记录，不影响本仓六相源码与 W8 结论。

## W8 实机验证

按顺序阅读（**W8 分级实测已废弃**——用户未按分级流程测试，实机测试已直接通过；下列 W8 文档
仅为历史流程/记录档案，不再作为进度来源；职责分工见 `AGENTS.md`「W8 文档职责分工」）：

1. `docs/w8/W8_TEST_MASTER.md`（历史流程 + 阶段档案：阶段 0-4 必检项、通过标准、记录索引；顶部已加废弃说明）
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
14. `docs/w8/W8_STAGE2_CODE_PREPLAN_2026-08-27.md`（阶段2 代码级预案：触发架构/引脚修正/模式表/2-1..2-6 上机预期，离线产物）
15. `docs/w8/W8_ISSUE_FIX_2026-09-01.md`（产品信息屏厂商 X/O 显示修复 + 字形风格修正 + 电话行宽修正）

## 证据与历史

- 原始硬件证据：`evidence/hardware/README.md`。
- 原始反编译和反汇编：`evidence/README.md`。
- 历史计划、项目全史和时间点审计：`docs/history/`；仅供追溯。
- 目录用途总览：`docs/README.md`、`tools/README.md`、`test/README.md`。
