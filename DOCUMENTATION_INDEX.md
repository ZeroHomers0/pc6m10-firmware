# 文档索引与状态基线

> 最后更新：2026-08-23。本文是项目文档的统一入口；涉及进度时，以本文和 `AGENTS.md` 为准。

## 当前结论

- 可编译、可修改的 GCC 工程已经建立；当前构建为 `text 61936 / data 3000 / bss 2188`。
- 当前 `firmware.bin` SHA-256：`F032EFB70BB3942C4999D7C1F2D0DEBB64125F004C4E19405CAB0DD08F5EAA44`。
- 独立差分验证已通过：`output_stage` 144/144、`state_machine` 115/115；既有测试套件 11/11 模块通过。
- 离线验证支持进入“断开门极与功率负载”的分级上机验证；尚不能据此宣称整机或带载 100% 等价。
- 当前开发分支为 `codex/decompiler-review`；目标集成分支为 `master`，远端为 `origin`。

## 推荐阅读顺序

1. `AGENTS.md`：自包含项目上下文和最新进度。
2. `W8_ONBOARDING_2026-08-22.md`：拿到硬件后的总入口。
3. `W8_PRE_HARDWARE_VALIDATION_2026-08-23.md`：上机前离线验证基线。
4. `W8_HARDWARE_TEST_2026-08-22.md`：分级硬件实测清单。
5. `W8_SOFTWARE_OPERATION.md`：免费软件、官方下载地址及操作方法。
6. `SETUP_WINDOWS.md`：固定版本的 Windows 环境配置。

## 文档分类

| 类别 | 文档 | 状态 |
|---|---|---|
| 总览 | `README.md`、`AGENTS.md`、`CLAUDE.md` | 当前 |
| 当前进度 | `WORK_GUIDE_2026-08-21.md`、`W8_PRE_HARDWARE_VALIDATION_2026-08-23.md` | 顶部状态为准 |
| W8 实测 | `W8_ONBOARDING_2026-08-22.md`、`W8_HARDWARE_TEST_2026-08-22.md`、`W8_SOFTWARE_OPERATION.md`、`SETUP_WINDOWS.md` | 当前 |
| 逆向成果 | `APPLICATION_GUIDE_2026-08-21.md`、`DATA_SEGMENT_2026-08-21.md` | 当前参考 |
| 协议/模块 | `docs/I2C_PARAM_SYNC.md`、`docs/UART3_PROTOCOL.md`、`docs/STATE_MACHINE_ANALYSIS.md`、`docs/MENU_PARAMETER_MAPPING.md`、`docs/HARDWARE_VERIFICATION_2026-08-20.md` | 技术参考 |
| 历史过程 | `PROJECT_SUMMARY_2026-08-21.md`、`docs/PLAN.md`、`docs/PROGRESS_2026-08-20.md` | 历史快照 |
| 审计记录 | `CODEX_FULL_AUDIT_2026-08-23.md`、`CODEX_REVALIDATION_2026-08-23.md` | 时间点记录；前者缺陷已由后续提交修复 |
| 子工程 | `firmware/README.md`、`test/README.md` | 当前 |
| 工具内部 | `tools/RENAMING_GUIDE.md`、`tools/_SM_W1B_PROGRESS.md` | 维护/历史记录 |

## 命名规则

- 固定入口使用生态惯例：`README.md`、`AGENTS.md`、`CLAUDE.md`。
- 专题文档使用 `UPPER_SNAKE_CASE.md`；时间点文档使用 `TOPIC_YYYY-MM-DD.md`。
- W8 文档统一使用 `W8_` 前缀；工具内部临时记录可保留前导下划线。
- 日期表示文档起始或审计时间，不表示内容一定仍是最新；是否当前以本索引“状态”列为准。

## 历史与当前结论的使用原则

历史文档用于保存推理、纠错和证据链，不应删除。若历史数字或风险判断与当前状态冲突，依次采用：
`W8_PRE_HARDWARE_VALIDATION_2026-08-23.md` → `AGENTS.md` → 本文 → 历史记录。
