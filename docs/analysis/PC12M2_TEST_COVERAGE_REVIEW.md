# PC12M-2 测试覆盖复查结果（任务 #4）

> 日期：2026-08-31
> 范围：参考 PC6M-10/test/emulation 测试覆盖，为 PC12M-2 查漏补缺并跑通
> 状态：**全部 113/113 PASS**
> 测试脚本：`D:\code\LPC1765FBD100\PC12M-2\test\emulation\test_extra_coverage_12.py`（644 行，已保留在 12p 仓库）
> 运行：`cd PC12M-2 && python test/emulation/test_extra_coverage_12.py`
> 依赖：`tools/verification/verify_firmware_equivalence_12.py`（machine/run/snapshot/SYMS）

## 一、背景

PC6M-10（六相成熟仓）`test/emulation` 下有 22 个 A/B 测试文件；PC12M-2 的 P5 验证
只覆盖了其中一部分（静态 + 状态机 + Modbus 读写矩阵 + 显示 + 去抖 + 认证 + MISC 等，
见 `docs/analysis/P5_VERIFICATION_PROGRESS.md`）。

本次把 12p **尚未覆盖**的 9 个测试组移植为 12p 地址，全部 A/B 差分：
OLD（`pc12m2_orig.bin` 原固件）vs NEW（GCC 重建 `firmware.elf`），
比较 R0 + SRAM 快照 +（需要时）外设写迹 / i2c 写序列。

## 二、结果汇总

```
adc_wait_done            4 PASS  0 FAIL
adc_scan_channels       13 PASS  0 FAIL
input_scan_state         6 PASS  0 FAIL
uart_rx_sequence         5 PASS  0 FAIL
modbus_dispatch         20 PASS  0 FAIL
eeprom_sync_matrix       1 PASS  0 FAIL
interrupt_sequence       5 PASS  0 FAIL
control_multitick        3 PASS  0 FAIL
case3_edit              56 PASS  0 FAIL
─────────────────────────────────────
TOTAL 113 PASS / 0 FAIL
```

各测试组覆盖点：

| 组 | 覆盖点 |
|---|---|
| adc_wait_done | AD0GDR/AD0DR0 播不同结果，抓「错读 +16 偏移」回归（g_adc 是 uint32_t* 陷阱） |
| adc_scan_channels | 连续多拍扫描 + cfg_word/gain_sel/标定除数边界（1/0xA/0xFFFF/0xFFFFFFFF） |
| input_scan_state | 全 6 位引脚组合 × 计数初值差分（512 例）+ 快加/快减/慢加/慢减关键事件 |
| uart_rx_sequence | 经 UART3_IRQHandler(IIR=4) 多字节组帧状态转移 / 索引回绕（255→0） |
| modbus_dispatch | 读/写(0x06/0x10)/异常/CRC 错/站址不匹配帧全流程 + 13 帧异常矩阵 |
| eeprom_sync_matrix | param_sync 逐字节(0x110)与批量(8)扰动，i2c_write_reg 写序列捕获 |
| interrupt_sequence | 多 ISR 顺序执行（EINT1/2/3 + TIMER0/1/2 + UART3），RAM+MMIO 差分 |
| control_multitick | 状态机 + 输出级在持久 RAM 上连续多拍（RUN/STOP/FAULT 刺激） |
| case3_edit | menu=3 case3 编辑键矩阵（menu2=2..15，P5 只覆盖 0/1/3） |

## 三、本次发现的真实移植 bug（A/B 差分暴露，已修复）

### 3.1 adc0_scan_channels 增益全局对调（05_adc.c）

- 症状：非零 ADC 样本下 A/B 不等价，`RAM首差异=0x10001590/94/B8/BC`（ch3/ch4/ch5 标定输出）。
  零值样本 PASS（0×gain=0 掩盖），非零样本暴露。
- 根因（OLD 反汇编 `00001f6c` 铁证）：05_adc.c 中 `gain_a`(0x10001630) / `gain_b`(0x10001634)
  四个使用点**全部对调**：

  | 平均段 | OLD 用地址 | 正确符号 | 源码误用 | 修复 |
  |---|---|---|---|---|
  | ch5 gain_sel==0 | 0x10001634 | gain_b | gain_a | ✅ 已改 |
  | ch5 gain_sel==1 | 0x10001630 | gain_a | gain_b | ✅ 已改 |
  | ch3 (scan_idx==4) | 0x10001630 | gain_a | gain_b | ✅ 已改 |
  | ch4 (scan_idx==5) | 0x10001634 | gain_b | gain_a | ✅ 已改 |

- 验证：修复后 adc_scan_channels 13/13 PASS。

### 3.2 modbus_dispatch 0x10 写多异常路径漏写 menu_param_4（08_modbus_dispatch.c）

- 症状：`0x10 字节数不匹配` 帧（frame[6] != Q*2）A/B 结果不同，
  snapshot 差异 `0x1000177C`（menu_param_4，原=0x02 新=0x00）。
- 根因（OLD 反汇编 `0000b3b2` 0xDFAE-0xDFC0）：先写 `menu_param_4 = frame[6]`
  **再**比较 `frame[6] != Q*2`；源码先比较、异常 return 时**漏写 menu_param_4**。
- 修复：把 `*MENU_P4 = frame[6]` 移到比较之前（忠实复现 OLD 顺序）。
- 验证：修复后 modbus_dispatch 20/20 PASS。

## 四、测试脚本调试记录（供复现）

1. **hook 注册地址 bug**：`_adc_scan_run` 曾对 is_new 机器同时注册 OLD 地址
   （0x1F30/0x1F56）与 NEW 地址 hook。12p OLD/NEW 布局不同，OLD wait_done 地址
   0x1F56 恰好落在 NEW adc0_scan_channels 函数体中段，被 OLD wait_hook 误触发读 LR
   跳回形成死循环（max_insn 截断）。修复：**只注册本侧地址**。同类问题同步修复
   modbus_dispatch（uart3_tx_byte / i2c_write_reg hook）与 eeprom_sync（i2c hook）。
2. `UC_HOOK_MEM_WRITE` 未导入 → 补导入。
3. `_isr_seq_run` 中 `run(uc, OLD[name])` 对虚拟名 `"RX"` KeyError → 先映射为
   `"UART3_IRQHandler"` 再执行。

## 五、结论

- PC12M-2 本次新增 9 个测试组 113 例，**全部 A/B 等价 PASS**。
- 差分测试真实捕获了 2 个源码移植 bug（增益对调、异常路径漏写），均已按 OLD 反汇编
  修复并回归 PASS——证明 A/B 差分对「非零路径」与「异常路径」的覆盖价值。
- 测试脚本已保留：`test/emulation/test_extra_coverage_12.py`。
- 运行完整测试后需 `cd firmware && bash build.sh` 重建固件（本次修改了 05_adc.c 与
  08_modbus_dispatch.c）。
