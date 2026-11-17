# 文档索引

> **2026-11-17 软件复核第二批**：[执行范围、真实差异与未关闭缺口](docs/analysis/SOFTWARE_REVALIDATION_2026-11-17.md)。六相回归54/54与原BIN A/B通过；新增连续流程、持久介质、复位链及有限ISR抢占验证，分支/测试干预台账仍有待关闭条目，不代表实施方案全部验收。未烧写/实机。

> 最后更新：2026-11-16。当前进度优先级：`AGENTS.md` → 本文 → `docs/project/PROJECT_STATUS.md` → 历史文档。
>
> **2026-09-02 结论**：自编译固件已烧写入板并经用户直接实机测试通过（含产品信息定制、X/O
> 字形修复与电话行宽修正后的最终固件）。**W8 分级实测流程未按计划执行、已废弃**——用户未按
> W8 分级流程测试；`docs/w8/` 下 W8 分级文档保留为历史流程档案，进度以 `AGENTS.md` 与本文为准。

> **2026-11-13 菜单文字定制**：α 的 min/max 下标、主从移相、相位补偿及远程/面板选项已构建并完成离线验证，已于2026-11-13通过J-Link烧写并完成整片读回核对，待用户物理断电上电与功能冒烟确认。当前固件尺寸与 SHA 以 `AGENTS.md` 为准。完整回归 26/26，原 BIN A/B 全套与菜单专项验证通过。新增字形统一保存在 `firmware/src/02_lcd_display.c`；专项入口 `tools/verification/verify_menu_text.py`。

> **2026-11-13 背光定制**：无按键操作约20秒熄灭背光，按键重新计时；已于2026-11-13通过J-Link烧写并完成整片读回核对，待用户物理断电上电与功能冒烟确认。执行专项 `tools/verification/verify_backlight_timeout.py`。

> **2026-11-14**：菜单标题改为“同步校准”，新增同/步字形；当前构建见 `AGENTS.md`，已烧写并完成整片读回核对，待用户物理断电上电与显示确认。

> **2026-11-16**：修复六相点动端子启停事件码5/6与状态机7/8不匹配，新增原BIN边界矩阵1176＋88例，未烧写。当前哈希见 `AGENTS.md`。

> **2026-11-16 菜单文案**：模选/解限、瞬动/保持、同步参数校准；字形与专项已更新，尚未烧写。当前构建见 `AGENTS.md`。

> **2026-11-16 第二轮精读修复**：逐模块精读并回查反汇编，又修复 5 类功能/状态差异与 6 类结构差异（显示列宽/单位/DISABLED、control_mode==2 重算范围、枚举越界、擦除项号、兜底超时），见 [第二轮修复验证](docs/analysis/PC6M_AUDIT_FIXES_2_2026-11-16.md)。

> **2026-11-16 审计修复**：原BIN对照发现的8类差异已修复，见 [修复验证](docs/analysis/PC6M_AUDIT_FIXES_2026-11-16.md)。[原始审计](docs/analysis/PC6M_FULL_CODE_AUDIT_2026-11-16.md)保留当时事实；不把有限矩阵通过解释为全量形式化等价。

- **2026-11-17 保护页第 3 页上界 + 枚举越界绘制**：全屏显示调用矩阵发现屏号 4 项号 12..16 多画第 3 页，按原 BIN 0x84FC/0x8504 限定 8..11（`test/emulation/test_menu_page_bounds.py` 72/72）；又按用户实测修掉通信校验/校验位/控制方式等越界值仍绘制「关闭」的问题（`test/emulation/test_enum_range_equivalence.py` 27/27）。构建 text 59852、SHA `0C7B6539…AFBD8EA`；未烧写。

- **2026-11-17 独立复核与新差异修复**：见 [双项目复核报告](docs/analysis/INDEPENDENT_REPAIR_REVIEW_2026-11-17.md)；PID符号边界、六相显示低字节读取与独立执行完成门禁已修复。当前构建以AGENTS.md为准；未烧写。

## 当前文档

| 主题 | 文档 |
|---|---|
| 软件设计及开发文档 | `docs/project/PC6M10_软件设计及开发文档_V1.1.docx`（产品背景、软件架构、模块设计、接口、开发、测试与烧写） |
| 软件设计文档生成脚本 | `tools/docs/generate_pc6m_software_design_doc.py`（可重建 Word，支持 `--output` 指定输出路径并执行结构、禁用表述和标题字体检查） |
| AI 逆向与重建总指南 | `docs/analysis/REVERSE_ENGINEERING_AI_GUIDE.md`（项目结构、证据链、A/B 差分方法、构建烧写、历史陷阱） |
| 构建与烧写操作 | `操作文档.md`（构建命令 + ISP 烧写 + SWD 烧写） |
| 项目状态 | `docs/project/PROJECT_STATUS.md` |
| 可读性重构记录（2026-09-04） | `docs/project/READABILITY_REFACTOR_2026-09-04.md` |
| 官方开发板 VS Code 调试环境（**仅官方板**，不适用 PC6M-10 目标板） | `docs/project/OFFICIAL_DEVBOARD_DEBUG_2026-09-05.md` |
| 人工可读化二阶段计划与实施状态 | `docs/project/HUMAN_READABLE_REFACTOR_PLAN_2026-09-04.md` |
| 固件编译配置与警告策略 | `firmware/COMPILER_PROFILE.md` |
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

- [2026-11-17 第二路闭环反馈复制修复及原BIN验证](docs/analysis/PC6M_FEEDBACK_FIX_2026-11-17.md)

- **2026-11-17 烧写记录**：PC6M提交24cbbad（E843035C…9022B2）已通过J-Link烧写、整片备份及读回校验；CRP全FF。记录在 `backup/flash_2026-11-17_feedback_fix/manifest.json`（本地备份不上传）。待用户物理断电上电确认。

- **2026-11-17 异常应答功能码 func+0x80**：异常应答功能码改为 8 位加法回绕，新增 `test/emulation/test_exception_func_code.py`；构建 text 59804（SHA `38B3EB17…C405F7`），未烧写。

- [2026-11-17 最新提交联合复核与执行完成门禁](docs/analysis/JOINT_VALIDATION_2026-11-17.md)

- **2026-11-17 校准结果页枚举越界绘制修复**：屏 0x63 急停/辅助模式取 ≥3 时原 BIN 不绘制选项（`0x5D94`/`0x5E12` 的 `cmp #2; bne`），源码末支 `else` 改为 `else if (... == 2)`；状态机参数档扩到 18 档后全屏矩阵 117810 例 bad=0，枚举越界专项 47/47、回归 35/35、原 BIN A/B 全 PASS。构建 text 59892（SHA `7AD41A4B…664C744D`，60452 B），提交 `f3e78df`，未烧写。

- **2026-11-17 寄存器命名与 Modbus 索引订正**：SCS(0x400FC1A0, bit5=OSCEN) 与 CLKOUTCFG(0x400FC1C8) 原名误标为 PLL1 锁相/PCLKSEL1 别名，crc16 两表字节角色、uart3 池槽地址当数据地址、gpio0_input_init 位号、timer0 中断号一并订正；modbus_read_reg/modbus_write_multi 按原机 ldrb 截断后索引。构建 text 59900（SHA `B20C67A1…5FBA92B`，60460 B），提交 `b5f6868`，回归 35/35、原 BIN A/B 全 PASS，未烧写。

- **2026-11-17 立即数（阈值）差分全量审计**：以「比较类指令立即数集合差」独立排查 87 个函数，68 条候选与状态机（GCC 已拆成 7 个符号）整体集合差全部定性为等价改写（位测试移位化、`>=N` 与 `>N-1`、逐值比较改区间比较、switch 降级形式），未发现新的阈值/边界差异。状态机全矩阵累计 302940 例 bad=0。详见 [PC6M_IMMEDIATE_DIFF_AUDIT_2026-11-17.md](docs/analysis/PC6M_IMMEDIATE_DIFF_AUDIT_2026-11-17.md)。

- [PC6M-10 初始化/底层 I/O 执行级覆盖补齐（2026-11-17）](docs/analysis/PC6M_INIT_TRACE_2026-11-17.md)

## 差异消除实施方案（2026-11-17）

[PC6M／PC12M 固件逻辑差异消除与缺陷修复实施方案](docs/project/FIRMWARE_EQUIVALENCE_REMEDIATION_PLAN_2026-11-17.md)：以证据修正行为和验证失真，保留认证放行与已授权定制，不以删除代码或证据完成任务。该文件为实施设计，仅做代码/模拟验证，不包含实机验证或烧写，不代表已完成全量验收。
