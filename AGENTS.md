# AGENTS.md — LPC1765FBD100 固件逆向工程

> 本文件是 AI 接手项目的唯一权威上下文。文档导航见 `DOCUMENTATION_INDEX.md`。

## 项目与当前状态

目标设备为 PC6M-10 三相 SCR 移相触发功率控制板，MCU 为 NXP LPC1765。原始固件
`LPC1765.bin`（262144 B）是所有等价验证的金标准。

- GCC 可编译工程：`firmware/`，Arm GNU Toolchain 14.2.Rel1。
- 当前构建：`text 59900 / data 0 / bss 2188`（2026-11-17 寄存器命名与 Modbus 索引订正后）。
- 当前 `firmware.bin` SHA-256：`B20C67A1994AED331C59CE096DF4903B9B3391676D10CCB051ADA94095FBA92B`（60460 B）。
- 板上已烧写镜像为上一版 `E843035C88D1B5AE52A516072F09FC551BC90720CFF23EA4EEF6623FF19022B2`（60368 B，text 59808，含闭环反馈复制修复；本次功能码修复未烧写）。
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
  原厂型号（略） / V2.0.2016 / SINEP0WER / 18938061832）。实现：`tools/generation/generate_string_pool.py`
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

- **2026-11-13 菜单文字定制**（用户要求，2026-11-13 已烧写并读回校验）：相位限制→αmin限制、起始相位→αmax限制（含标定结果页同名项）、主从偏移→主从移相、相位校准→相位补偿；控制方式选项通讯→远程、定值→面板，本地不变。min/max 为 5×5 紧凑下标（3 字母共16像素）；α 按 Times New Roman 原字形等比例采样为16×16点阵，占满首末行，α+下标总宽仍32像素；新增字形集中在 `firmware/src/02_lcd_display.c`，不新增字库头文件。文字覆写入口为 `tools/generation/generate_string_pool.py` 的 `MENU_TEXT_OVERRIDES`。构建 `text 59296 / data 0 / bss 2188`，SHA-256 `A8C75FE705D87CA2A664E5C94F4494DF1747AE5FF3767EB6F9C2F1C530A97A1C`。完整回归 26/26、原 BIN A/B 全套及菜单专项验证通过；菜单文字/新增字形为有意定制偏离，不改变控制参数逻辑。状态矩阵移除未使用且会触发 Unicorn 异常的 GPIO 写 hook、跳过无副作用 Delay，并新增完整返回检查；菜单编辑测试同样跳过 Delay 并检查返回。日志见 `evidence/reverse/reports/menu_*_2026-11-13.log`。

- **2026-11-13 背光时长定制**（用户要求，2026-11-13 已烧写并读回校验）：`state_machine_update_entry()` 背光无操作阈值由5000拍改为10000拍（每拍约2ms，约20秒）；按键点亮并清零计时。此为原 BIN 的有意定制偏离；其它菜单超时阈值保持原值。专项入口 `tools/verification/verify_backlight_timeout.py`，原 BIN/新 ELF 边界与按键续亮28例、完整 A/B 矩阵及回归26/26通过；日志见 `evidence/reverse/reports/backlight_*_2026-11-13.log`。当前固件 SHA 见本文顶部。

- **2026-11-13 新版烧写完成**：使用仓库打包 J-Link（S/N 609799999，SWD 100kHz），用户已确认断开市电/门极/负载、仅接控制电。先备份板上完整256KiB并二次 verifybin 通过，再擦写当前固件（SHA见顶部）。verifybin通过；整片读回前59856字节与包内固件逐字节一致，其余Flash全FF，CRP=FFFFFFFF。备份/日志/manifest位于 `backup/flash_2026-11-13_menu20s/`（不提交备份）。固件包位于 `release/PC6M10_2026-11-13_menu_alpha_backlight20s.zip`。尚待用户物理断电上电及本次菜单/背光冒烟确认；本次不以J-Link驱动nRESET作为运行重启。

- **2026-11-13 项目命名清理**（用户要求）：活动源码注释、工具脚本、项目/分析/历史说明及软件设计 Word 文档统一使用本项目名称；历史原厂型号引用改为省略标记，不修改原始证据内容。原始 BIN、备份及 `evidence/` 下提取/反编译资料保留原样。Word 仅替换一处适用固件字段；XML复查通过，因本机缺少LibreOffice未完成渲染。重新构建后BIN哈希与已烧写版完全一致，菜单与背光专项、原BIN A/B全套通过（`evidence/reverse/reports/naming_cleanup_ab_2026-11-13.log`），无需重复烧写。

- **2026-11-14 菜单标题调整**：按用户要求在 `firmware-menu-display-2026-11-13` 分支将相位补偿改为同步校准，补齐同/步点阵并集中在显示模块。菜单专项及原BIN完整A/B验证通过（`evidence/reverse/reports/menu_sync_calibration_ab_2026-11-14.log`）；2026-11-14已通过仓库J-Link烧写，verifybin及整片读回核对通过，CRP=FFFFFFFF；备份和日志见 `backup/flash_2026-11-14_sync_calibration/`。尚待用户物理断电上电及显示确认。

- **2026-11-16 端子启停事件码修复**：点动扫描曾错误返回面板键码5/6，修正为原BIN要求的端子事件7/8；新增 `firmware/inc/input_events.h` 明确事件契约，扫描与状态机共用。新增 `test/emulation/test_terminal_event_contract.py` 覆盖1176例扫描和88例去抖边界，修复后全部A/B一致，完整原BIN A/B及全量回归27/27通过，日志见 `evidence/reverse/reports/terminal_event_*_2026-11-16.log`。保留原版本地＋自锁门控；此次未烧写。本次修复前的全套测试未覆盖点动触发边界，历史PASS不构成本问题不存在的证明。

- **2026-11-16 菜单文案调整**：共享选项外控→模选、限相→解限；启动方式点动→瞬动、自锁→保持；菜单6相位参数校准→同步参数校准。编号/值列/参数逻辑不变；短标题补空格清除旧字形。新增瞬/解/均/选/持集中点阵；同/步沿用已扩展字形。菜单专项13项正反显通过。显示全执行A/B以原厂文字验证渲染行为，定制文字及字形另由菜单专项实际执行检查。本仓完整原BIN A/B及回归27/27通过，日志见 `evidence/reverse/reports/menu_wording_*_2026-11-16.log`。此次未提交、推送或烧写。

- **2026-11-16 审计及修复**：全仓审计确认的8类行为差异已按原BIN修复，保留授权菜单、字体和20秒背光定制。历史发现见 `docs/analysis/PC6M_FULL_CODE_AUDIT_2026-11-16.md`，修复与验证见 `docs/analysis/PC6M_AUDIT_FIXES_2026-11-16.md`。新增 `test/emulation/test_audit_fixes.py` 强制断言通信/菜单扩展矩阵、密码错误外层及喂狗次数、复位命令应答/EEPROM顺序与非返回自跳。未烧写硬件；旧菜单烧写包EBC44BD6…5F46保留为历史，不作为本次修复产物。

- **2026-11-16 第二轮逐模块精读修复**：逐模块精读源码并回查原BIN反汇编，又确认5类功能/状态差异与6类结构差异并全部修复：参数页电压/电流限值「未达限值画DISABLED、达限值补V/A单位」、软起/软停与PID增益改 disp_uint4、control_mode==2 不重算目标幅值/手动输出、枚举越界与 control_method>=3 不再多画/多算、擦除按当前项号、状态监视页无故障帧提前返回、非法屏号兜底超时。构建 text 59808/data 0/bss 2188；显示调用序列 640/640、参数菜单入口 54/54、状态终态 42/42、导航 18/18、回归 30/30、原BIN A/B 全 PASS。新增 test/emulation/test_display_call_equivalence.py（显示调用契约 152/152、控制量终态 12/12）与 test/emulation/test_audit_fixes_2.py。记录见 docs/analysis/PC6M_AUDIT_FIXES_2_2026-11-16.md。未烧写硬件。

- **2026-11-17 最新两提交独立复核**：`7af6119`/`1a55f84`大部分修正有原BIN依据，但第二路闭环三处反馈复制仍误写为自赋值（`09_output_stage.c` 576/589/601行），可使PID反馈原版3000变为当前1。原第二路回归未满足ADC给定>=10的分支前提，无法覆盖该缺陷。现有30/30与完整A/B仍通过，新增实际入分支样本4例失败；临时副本补齐三处复制后消除。该复核时未修改业务源码；随后已按用户要求修复并补齐真实分支回归，见下条。详见 `docs/analysis/PC6M_LATEST_TWO_REVIEW_2026-11-17.md`。

- **2026-11-17 闭环反馈复制修复**：三处恢复 `adc_field_output_scaled = adc_voltage_output_scaled`，对应原BIN 0xF450/0xF4B6/0xF50A。回归显式设置ADC给定100，三个复制路径各4例，断言原复制指令及PID调用确实发生，反馈3000、PID目标526及完整SRAM一致；通信/输出级测试增加正常返回检查。修复记录与最终验证日志见 `docs/analysis/PC6M_FEEDBACK_FIX_2026-11-17.md`。未烧写硬件。

- **2026-11-17 已烧写闭环反馈修复版**：用户确认断开市电/门极/负载、仅限流控制电。使用仓库J-Link（609799999，SWD100kHz），完整256KiB备份及verifybin通过后，烧写隔离构建的提交 `24cbbad`，固件60368字节，SHA256 `E843035C88D1B5AE52A516072F09FC551BC90720CFF23EA4EEF6623FF19022B2`。烧后verifybin、整片读回逐字节核对通过，其余Flash全FF，CRP=FFFFFFFF。备份、脚本、日志与manifest在 `backup/flash_2026-11-17_feedback_fix/`，不上传。工作区并行通信改动未纳入；当前构建文件可能已被并行任务替换，以本次备份目录中固定固件与manifest为本次烧写证据。未驱动nRESET运行重启，尚待用户物理断电上电及实机冒烟确认。

- **2026-11-17 异常应答功能码修复**：原 BIN 异常应答的功能码是 `adds r0,#0x80`（0xB42C / 0xB4C2），按 8 位回绕；源码用按位或，功能码 >=0x80（0x83/0x86/0xFF）时原机应答 0x03/0x06/0x7F、新固件仍回原值。已把两处（功能码非 0x03/0x06/0x10 与 CRC 错误）改为 `(uint8_t)(rx_frame[1] + 0x80)`，新增 `test/emulation/test_exception_func_code.py`（非法功能码 10 帧 + CRC 错误 3 帧，共 13/13 与原 BIN 一致）。构建 text 59804、回归 31/31、原 BIN A/B 全 PASS。未烧写。

- **2026-11-17 保护页第 3 页上界修复**：全屏显示调用矩阵（6936 例：屏号 0/1..9/0xB/0xC/0x14/0x1E/0x5A/0x62/0x63 × 项 0..16 × 12 键 × view 0/1）发现保护参数页（屏号 4）项号 12..16 仍画第 3 页；原 BIN 0x84FC `cmp #8; blt` + 0x8504 `cmp #0xc; bge`（跳 0x85C0）证明只有 8 <= 项号 < 12 才绘制、>=12 直接跳过（PC12M-2 同源缺陷已在 12p 侧修成 `else if (< 0xc)`）。修法：`draw_protection_parameter_page` 增加 `if (item_index >= 0xc) return;`，导航标题块末支 `else` 改 `else if (*ui_item_index_ptr < 0xc)`。另用全屏状态终态矩阵（同样 6936 例，比较 SRAM0 0..0x2200 + 六段外设终态）复核：除密码错误延时循环的 0x1000161C/0x10001620 残值（指令预算截断，400M 指令预算下两版完全一致）外全部相等。构建 text 59824 / data 0 / bss 2188，SHA-256 `E98F6B07…F6B727B`（60384 B）；新增 `test/emulation/test_menu_page_bounds.py`（72/72）、回归 32/32、原 BIN A/B 全 PASS。未烧写。

- **2026-11-17 枚举参数越界绘制修复**：用户实测指出通信校验取 2、3 等越界值时原版不绘制选项，旧源码 `if (...) 开启 else 关闭` 会把任意非 1 值落成「关闭」。原 BIN 逐条确认：0x8a5c `cmp #3; bne` 只在 ==3 画 '1 ST0P'；0x8ad2 `cbnz` 只在 ==0 画 DISABLED、0x8ae4 `cmp #1; bne` 只在 ==1 画 ENABLED；基本参数页选项 3 入口块 0x8b6a 同理。三处 `else` 改为相等判断。新增 `test/emulation/test_enum_range_equivalence.py`（27 例，控制方式/控制手法/急停/校验位/通信校验越界逐值比较绘制序列）。验证：枚举越界扫描 730/730、显示调用契约 6936/6936、回归 33/33、原 BIN A/B 全 PASS。提交 `a6c1045`。未烧写。

- **2026-11-17 双项目联合复核与验证门禁修复**：6M当前提交64b3e1f及已包含的周期同步retry门控、保护页项号与枚举越界、高位功能码回绕已按本项目原BIN核对。`verify_firmware_equivalence.py`现要求每个有界调用正常返回，防止预算耗尽时比较中途状态误判通过；新增`test_execution_completion.py`证明死循环/未就绪UART被拒绝、正常返回/就绪UART通过。回归35/35、完整原BIN A/B通过，日志与哈希在 `evidence/reverse/reports/joint_validation_2026-11-17/`。本轮仅修改验证工具与测试；最新固件未重新烧写。详见 `docs/analysis/JOINT_VALIDATION_2026-11-17.md`。

- **2026-11-17 校准结果页枚举越界绘制修复**：把 `tmp/sm_matrix.py` 的参数档从 11 扩到 18（补入急停/辅助模式/反馈/输入模式/输出相位/控制方式的越界取值）后，校准结果页（屏 0x63）暴露出同类缺陷：急停或辅助模式取 ≥3 时，原 BIN `0x5D94 cmp #2; bne 0x5DE0` 与 `0x5E12 cmp #2; bne 0x5E60` 都不绘制任何选项，旧源码末支 `else` 会落成「限相」。两处改为 `else if (... == 2)`。同页反馈/输入模式/输出相位三项是原机真二分（`cbnz` 分支），不动。验证：扩展档位后状态机终态矩阵 `3,4,5,7` 13464、`8,9,a,63` 13464、`0,1,2,6,b,c,d,e` 26928、`f,10..1f,5a,62` 63954 全 bad=0；枚举越界专项 47/47、回归 35/35、原 BIN A/B 全 PASS。构建 text 59892 / data 0 / bss 2188，SHA-256 `7AD41A4B…664C744D`（60452 B）。提交 `f3e78df`。未烧写。

- **2026-11-17 寄存器命名与 Modbus 索引订正**：`rev-init3`/`rev-modbus3` 审完 01_startup/13_gpio_init/10_relay_led/11_auth 与 Modbus 全模块，均无高/中差异。据此订正注释与命名：`0x400FC1A0` 实为 SCS（bit5=OSCEN、bit6=OSCSTAT）而非 PLL1 锁相，`0x400FC1C8` 实为 CLKOUTCFG（原机写 0 关 CLKOUT）而非 PCLKSEL1 别名；`crc16` 两表字节角色、`uart3` 多处把字面量池槽当数据地址、`gpio0_input_init` 位号（实为 P0.2/P0.28/P0.27）、`timer0_init` 中断号（ISER0=2 即 IRQ1）一并订正；`09_output_stage.c` bit5 欠压误标「过流」改回。另 `modbus_read_reg`/`modbus_write_multi` 按原机 `ldrb` 低 8 位截断后索引（`0xB004`/`0xB2F0`）。构建 text 59900 / data 0 / bss 2188，SHA-256 `B20C67A1…5FBA92B`（60460 B），提交 `b5f6868`；回归 35/35、原 BIN A/B 全 PASS、状态机双视图矩阵 6732 bad=0。未烧写。

- **2026-11-17 立即数（阈值）差分全量审计**：改用「只抽比较类指令（`cmp/cmn/tst/teq` 带条件后缀与 `.w`）立即数集合差」这一独立视角排查 87 个函数，逐函数比对先得 36/87 有差、215 条明细，过滤「`v-1`/`v+1` 在新函数体内出现」的 `>=N` 与 `>N-1` 等价改写后剩 68 条，再对 `state_machine` 做「原机单块 blob vs 新 ELF 全部 7 个 `state_machine*` 符号」整体比对。**68 条与状态机集合差全部定性为等价改写**：①原机 12 个故障位 `tst.w #16…#32768`（`0x46c2`–`0x4772`）在新 ELF 改为 `lsls` 移入 bit31 后 `bf44 itt mi` 取条件，事件码写入顺序与原机 `0x46b0–0x4780` 一致；②`>=`/`>` 改写（`input_scan_state` `0x1804 cmp #250/bcc` ≡ 新 `0x1780 cmp #249/bls`；`state_machine` `0x9ada cmp #69/bcs` ≡ 新 `0x69a2 cmp #68/itt ls`；`0x88d4 cmp #246/blt` ≡ 新 `0x6752 cmp #245/itt hi`）；③`modbus_dispatch` 22 条逐值比较改为「减下界 + 上界」（`0x840e sub #40`+`0x8412 cmp #120` 对应原机 `0xbed4 cmp #161/bcs`+`0xbedc cmp #39/bls` 的 40..160，写 `0x10001654`；`0x8626 subs #1`+`0x8628 cmp #246` 对应原机 `0xd54e cmp #248/bcs`+`0xd556 cbz` 的 1..247，写 `0x100016FF`）；④屏号 switch 由 `tbb` 降级为 `cmp #98/#99/#90` 加 `tbh`，`0x1d`/`0x1e`/`0x1f`/`0x3b` 是 `subs #1`/`#2` 后的范围检查。原机 `0x6b8c`/`0x4814`/`0x4da4` 等处的 ASCII `cmp #48..57`/`cmp #45` 是串数据区被误反汇编，不是真实阈值。结论：**未发现新的阈值或边界差异**。状态机全矩阵本轮累计 **302940 例 bad=0**（另有 201960 + 65450 例同批 bad=0）。详见 `docs/analysis/PC6M_IMMEDIATE_DIFF_AUDIT_2026-11-17.md`。仅新增文档与 `DOCUMENTATION_INDEX.md` 条目，未改业务源码、未烧写。

## 必须遵守

- **2026-11-17 用户明确的差异消除范围**：按 `docs/project/FIRMWARE_EQUIVALENCE_REMEDIATION_PLAN_2026-11-17.md` 实施；目标是消除源码与原BIN的非授权逻辑差异及测试假通过，不是删除代码。认证永久放行继续保留；菜单、字形、背光等已授权定制保留，原始证据不得删减。用户进一步限定本方案仅做代码/模拟验证，不包含实机验证或烧写，也不以实机测试作为方案验收条件。当前仅形成实施文档，阶段任务需另按证据验收。

- 全程中文交流。
- Git 提交说明必须使用中文（提交标题及正文均使用中文描述）。
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
