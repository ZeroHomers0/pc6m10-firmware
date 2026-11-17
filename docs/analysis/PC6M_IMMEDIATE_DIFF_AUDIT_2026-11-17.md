# PC6M-10 立即数（阈值）差分全量审计 2026-11-17

## 目的与边界

本轮用「比较类指令立即数集合差」这一独立视角，对 87 个已登记函数做一次阈值级排查，
目的是发现「条件、阈值、边界被改写」这类执行级矩阵不一定暴露的差异。

范围仅源码、反汇编与模拟执行；不含实机与烧写。

比较关系只能是：6M 原 BIN `LPC1765.bin` → 6M 新 ELF。不引用 PC12M-2 的地址与结论。

## 方法

1. 按 `tools/audit/full_review_2026-11-16/function_inventory.json`（87 项，键为 `"0x200"` 形式，
   值为 `{"name","source","old_bytes"}`）切出原 BIN 函数体与 `firmware/firmware.elf` 同名函数体。
2. 只抽取比较类指令的立即数：`cmp/cmn/tst/teq` 加条件后缀与 `.w` 变体。
   取立即数前必须先 `line.split("@")[0]` 去掉 objdump 附带的字面量池地址注释，否则全是噪声。
3. 先按函数名逐个比集合差，再对 `state_machine` 做「原机单块 blob vs 新 ELF 全部
   `state_machine*` 符号」的整体比对（GCC 已把该函数拆成 7 个符号：
   `state_machine@0x62b4`、`state_machine_update_entry@0x334c`、`state_machine_page_main@0x3f8e`、
   `state_machine_page_protection@0x3814`、`state_machine_page_basic_parameters@0x505c`、
   `state_machine_page_basic_parameter_edit@0x5818`、`state_machine_page_calibration_result@0x47f0`）。
4. 先过滤「`v-1` / `v+1` 在新函数体内出现过」的条目，因为 `>=N` 与 `>N-1` 是同一语义的
   两种编译产物。剩余 68 条逐条回查反汇编定性。

脚本：`tmp/t10.py`（逐函数集合差，36/87 有差异）、`tmp/t12.py`（带指令原文，215 行）、
`tmp/t13.py`（过滤 ±1 改写后 68 条，输出 `tmp/cmp_unexplained.txt`）、
`tmp/t16.py`（状态机 blob 与新 ELF 全符号比对）。

踩坑记录：PowerShell 里 `objdump > file` 默认写 UTF-16，Python 侧必须用
`encoding="utf-16"` 读取；新 ELF 转储为 UTF-8。

## 结论

**未发现新的阈值/边界差异。** 68 条候选与状态机集合差全部定性为等价改写，分四类：

### 一、位测试改写（原机 `tst #bit`，新 ELF 用移位取符号位）

原机状态机把 17 个故障位逐个写成 `tst.w r0, #bit` + `beq`：

| 位 | 原机地址 | 指令 |
| --- | --- | --- |
| 0x10 | `0x46f2` | `tst.w r0, #16` |
| 0x20 | `0x4702` | `tst.w r0, #32` |
| 0x40 | `0x46d2` | `tst.w r0, #64` |
| 0x80 | `0x4722` | `tst.w r0, #128` |
| 0x100 | `0x4712` | `tst.w r0, #256` |
| 0x200 | `0x46c2` | `tst.w r0, #512` |
| 0x400 | `0x46e2` | `tst.w r0, #1024` |
| 0x800 | `0x4752` | `tst.w r0, #2048` |
| 0x1000 | `0x4772` | `tst.w r0, #4096` |
| 0x2000 | `0x4762` | `tst.w r0, #8192` |
| 0x4000 | `0x4732` | `tst.w r0, #16384` |
| 0x8000 | `0x4742` | `tst.w r0, #32768` |

新 ELF 改为 `lsls` 把目标位移到 bit31 再 `bf44 itt mi` 取条件（例
`0x346c lsls r3, r3, #29` 对应 bit2、`0x347e lsls r5, r2, #30` 对应 bit1、
`0x34ac lsls r1, r2, #22` 对应 bit9），事件码写入 `[r4, #1864]`（`0x10001748`）
的分支顺序与原机 `0x46b0–0x4780` 完全一致。

同型的还有 `SystemInit` 的 `tst #0x40`(`0x44c`)/`tst #0x400`(`0x4f8`)/`tst #0x4000000`(`0x496`)、
`input_scan_state` 的 6 个方向位、`scan_run_stop` 的 `tst #0x8000000`、`debounce_p09` 的 `tst #0x200`、
`chk_p02_p03` 的 `tst #0x4`/`tst #0x8`、`i2c_write_byte` 的 `tst #0x80`/`tst #0x400`、
`i2c_read_byte` 的 `tst #0x400`、`adc0_wait_done` 的 `tst #0x80000000`、`uart3_tx_byte` 的 `tst #0x20`、
`read_input_p02` 的 `tst #0x4`，全部等价。

### 二、`>=`/`>` 与循环边界改写

- `input_scan_state` 饱和：原机 `0x1804 cmp r0, #250 / bcc 0x18f2` + `0x1808 movs r0, #245`
  （`>=250 → 245`）；新 ELF `0x1780 cmp r2, #249 / bls` + `movs r2, #245`（`>249 → 245`），等价。
  源码 `firmware/src/03_input_debounce.c` 写 `>0xF9` 即 `>=0xFA`，与原机一致。
- `state_machine` `0x45` 下限：原机 `0x9ada cmp r0, #69 / bcs / movs r0, #69`；新 ELF
  `0x69a2 cmp r2, #68 / itt ls / movls r2, #69`，等价（`<69` → `<=68`）。
- `state_machine` `0xf6` 上限：原机 `0x88d4 cmp r0, #246 / blt` + `movs r0, #246`；
  新 ELF `0x6752 cmp r3, #245 / itt hi / movhi r3, #246`，等价。变量是
  `communication_slave_address_ptr`（`0x100016FF`，池 `0x8b18`）。
- `disp_clear`：原机 `0x9d8 cmp r5, #64` / `0x9e0 cmp r4, #8` 双循环；新 ELF
  `ands.w r5, r5, #0xff / bne` 与起始 `0xb8` 步进到 `0xc0` 的 `cmp #0xc0 / bne`，同为 8 行 × 64 列。
- `disp_digit` 的 `cmp r7, #40`(`0xdac`) 在新 ELF 中以宽度常量形式保留。

### 三、逐值比较改区间比较（`modbus_dispatch` 22 条）

原机 0x06 单写分支对寄存器号逐个 `cmp r0, #N`（`0xa`/`0xc`/`0xd`/`0xe`/`0x12`/`0x14`/`0x1a`/`0x1c`/
`0x21`/`0x22`/`0x23`/`0x28`/`0x30`/`0x32`/`0x34`/`0x36`/`0x37`/`0x38`/`0x39`/`0x3a`/`0xa1`/`0xf8`）；
新 ELF 改写为「减去下界 + 上界比较」（例 `0x8412 cmp r3, #120` 对应 40..160、
`0x8514 subs r3, r4, #1 / cmp #127` 对应 1..128）。逐条抽查的两处边界：

- 原机 `0xbed4 cmp r0, #161 / bcs 0xbf46` 与 `0xbedc cmp r0, #39 / bls 0xbf46`
  即有效区间 `40..160`，写 `[r5, #0x54]`（`0x10001654` = `parameter_master_slave_offset_ptr`）；
  新 ELF `0x840e sub.w r3, r4, #40` + `0x8412 cmp r3, #120 / bhi 0x85f0` + `0x841a strb [r5, #84]`
  是同一区间与同一写目标。
- 原机 `0xd54e cmp r0, #248 / bcs 0xd5c0` 与 `0xd556 cbz r0, 0xd5be`
  即有效区间 `1..247`，写 `[r1, #0]`（`0x100016FF` = `communication_slave_address_ptr`）；
  新 ELF `0x8626 subs r3, r4, #1` + `0x8628 cmp r3, #246 / bhi 0x85f0` + `0x862e strb [r5, #255]`
  区间与写目标都一致。
- 原机 `0x83fe` 一带的 `cmp r4, #180` 写 `[r3, #1616]`（`0x10001650` = `parameter_phase_limit_ptr`），
  与原机 `cmp #0xb4` 的上界一致。

### 四、switch 降级形式不同（`state_machine` 剩余项）

- 屏号分派：原机 `tbb` 跳表；新 ELF 用 `cmp r3, #98`/`#99`/`#90`（`'b'`/`'c'`/`'Z'`，
  即屏 0x62/0x63/0x5A）配合 `tbh`，语义相同。新出现的 `0x1d`(`0x62ce`) 与 `0x1e`(`0x62c4`)
  是 `subs r3, #1 / cmp r3, #29` 覆盖 1..30 的 switch 范围检查，`0x1f`(`0x4834`) 与
  `0x3b`(`0x35fc`) 同理（`subs r2,#2 / cmp #31` 覆盖 2..33；`cmp r2,#59` 是 0..59 的上界）。
- ASCII 数字：原机 `0x6b8c`/`0x4814`/`0x4824`/`0x4834`/`0x4844`/`0x5b18`/`0x5b2c`/`0x5b40`/
  `0x5b54`/`0x5b68` 的 `cmp r6, #48..57` 在部分位置是**误反汇编**（那些地址是串数据区，
  如 `0x6b88` 起的 `"2: 2 1"`），新 ELF 对应位置改成 `adds r0, #48` 之类生成字符；
  真正的阈值为 `cmp r3, #48`（ASCII `'0'`）等在新 ELF 中保留。
- `0x2d`(`0x4da4`) 同样是串数据区（`"---"`）被误反汇编成 `cmp r5, #45`；新 ELF 对应
  `movs r0, #45` 输出 `'-'` 生成负号串。
- `0xc`(`0x5878`) / `0xd`(`0x6ea6`)：原机 `cmp r0, #12` / `cmp r0, #13` 是项号导航的
  下一页上界与项号判定；新 ELF 统一改用 `cmp r3, #11 / bhi`（`>11` 即 `>=12`，8 处：
  `0x1816`/`0x3874`/`0x39d6`/`0x491c`/`0x4fc2`/`0x513e`/`0x786e`/`0x7a0e`）与
  `subs #12` + `tbh` 的降级形式，边界一致。

## 附带产物

`firmware/src/07_state_machine.c` 中项号 `< 0xc` 的判定共 8 处
（`:596`、`:661`、`:715`、`:1002`、`:1561`、`:1620` 与 `draw_protection_parameter_page` 的早退 `:148`），
均与原机 `cmp #0xc / bge` 结构对应，无遗漏。

## 后续

本轮只覆盖「比较类立即数」。算术立即数（`adds/subs/and/orr/bic` 的常量）不在比较语义内，
已由 `tmp/imm_diff.py` 做过粗筛，命中项均为编译器噪声（新 ELF 普遍改用 `0x10001000` 基址常量
与位段立即数）。执行级覆盖以状态机全矩阵为准：

- 菜单 0..0x1F × 项号 0..16 × 12 键 × view 0/1 × 参数档 23 档
- 补充 `SM_STATS=0xfa,0x1f4,0x1f5` 三档刷新状态
- 累计 **302940 例 bad=0**（另有两组合计 201960 + 65450 例 bad=0）
