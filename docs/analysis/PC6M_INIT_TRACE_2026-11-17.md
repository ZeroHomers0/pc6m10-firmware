# PC6M-10 初始化/底层 I/O 执行级覆盖补齐（2026-11-17）

## 1. 起因

2026-11-17 的一致性估算得出：六相官方套件里 **25 个函数共 2502 B** 从未做过执行级
A/B —— 全部集中在启动、GPIO 初始化、I2C 位操作、ADC 初始化、以及显示后端入口。
这些函数此前只做过**静态**反汇编核对（8 个子代理逐文件精读），没有可重复的执行门禁。

`tools/verification/verify_firmware_equivalence.py` 的 `PAIRS` 表覆盖 42 项，缺口正是
它的补集。本次不改业务代码，只补测试。

## 2. 判定方式（为什么不重造业务状态）

这批函数的共同特征是**只写外设与 GPIO、不读参数、不依赖 SRAM 内容**，因此不需要
构造业务状态机，用两个可观察量即可判定等价：

- **外设写迹**：`UC_HOOK_MEM_WRITE` 记录 `(address, size, value)` 序列，按地址索引比对，
  要求两侧**逐项相同**（含写宽度与写次序）。
- **末态快照**：10 个外设页 + `FIO 0x2009C000` 的末态字节比对。

对会读写 SRAM 的少数函数（`i2c_read_reg`、`freq_adjust_sync`、`auth_retry`、显示后端）
改用「SRAM 末态 + 显示调用序列」判定。

## 3. 函数身份不是猜的

用 `tools/audit/full_review_2026-11-16/function_inventory.json`（87 项，
键为原机入口十六进制串，值为 `{name, source, old_bytes}`）逐字节比对确认归属：
25 个未覆盖入口中有 13 项与六相同名函数**函数体逐字节相同**，其中 12 项连紧随的
96 B 字面量池也相同（唯一例外 `chk_p02_p03` 的池 +10 字节，指向不同的 SRAM 全局，
属预期差异）。其余入口按 `bl` 目标与写迹特征定名。

## 4. 必须绕开的五个 Unicorn 陷阱

1. **FIO 页 `0x2009C000` 不能挂 `UC_HOOK_MEM_WRITE`**：挂上后 TCG 缓存会让新 ELF 的
   `fio1_pin23_ctrl` 等函数体**一条都不执行**，表现为「新固件少写一次 GPIO」。FIO 页
   只比末态。
2. **`i2c_delay` 入口是 `0x1C82`/`0x1C6C`，不是体内循环地址**：把 PC 设到体内地址会
   跳过压栈、破坏栈平衡，随后 `UC_ERR_FETCH_UNMAPPED`。
3. **`SystemInit` 死等晶振/PLL 锁定位**：`0x448 tst #0x40; beq`（OSCSTAT）、`0x496`、
   `0x4B6`、`0x4F8`、`0x518`。且 **Unicorn 的 MEM_WRITE 钩子在写入生效之前触发** ——
   在钩子里置位会被紧随其后的 store 覆盖（实测 SCS 只剩 `0x20`）。改为记账到
   `pending`，在下一指令的 CODE 钩子里补位。
4. **`long_delay` 是 6000 × 1000 双层空转**（原机 `0x5BE movw r1,#6000`、
   `0x5B2 cmp.w #1000`），实跑超 3000 万指令。原机内层体首指令 `0x5AA`、内层计数器在
   `[sp,#0]`、外层在 `[sp,#4]`；**新 ELF 把顺序交换了**（内层体 `0x5A6`、内层 `[sp,#4]`、
   外层 `[sp,#0]`）。做法：命中内层体即把内层计数器写 999，内层只再跑一轮退出，外层
   仍真实跑满 6000 次。（早期「直接跳 PC 到内层出口」会 `UC_ERR_INSN_INVALID`。）
5. **显示叶函数体必须跳过但调用必须记录**：最初只跳过不记录，`disp_decimal1` /
   `disp_screen_static` / `disp_screen_calib` 三者「什么都不画」也能 PASS，毫无区分力。
   改为记录 `(name, r0..r3)` 调用序列后，三者分别产生 **3 / 4 / 20 项**调用。
   原机侧地址表 `DISP_TRACE_OLD = {0xD3C: disp_string, 0xB44: disp_render_char8,
   0xC3A: disp_render_char16}`。

## 5. 两个「用例本身不该产生调用」的区分

- 显示后端函数：必须产生调用，否则视为用例没进绘制分支 → FAIL。
- `auth_retry`：`retry_state != 1` 时合法地一次也不进循环。故只要求两侧调用序列一致，
  不要求非空。

`auth_retry` 的 `auth_challenge` 无形参，原机调用前 r0/r1 残留 `auth_set_timeout` 的
写入值（实测 r0=50000），新 ELF 该写入内联成 `movw/str`、寄存器为 0。**实参不是契约**，
只比调用次数与顺序。

### long_delay 的迭代次数也必须纳入比较

long_delay 是 6000 × 1000 双层空转，**不写任何外设、也不写 SRAM**，唯一可观察量
就是外层跑了几轮。第一次实现时只在命中内层体时改写内层计数器，两侧写迹都为空、末态
完全相同，于是把 6000 改成 5999 重建后**仍然 PASS**。

修法是把迭代次数本身追加成一条伪写迹 (0xFFFFFFF0, 4, 轮数) 参与既有的写序列
相等比较；改完后注入立刻报
写迹 1/1 项不同 old_only=[(4294967280, 4, 6000)] new_only=[(4294967280, 4, 5999)]。

**教训：纯空转函数没有副作用可观察，必须显式把「循环轮数」导出成被比较的量，否则
测试对该函数的任何改动都无感。**

## 6. 检出能力双向验证（关键）

**没有反向验证的门禁等于没有门禁**。三次注入全部被检出：

| 注入点 | 改动 | 检出结果 |
|---|---|---|
| `firmware/src/01_startup.c` | `TIMER0->MR0 = 1999` → `1998` | `写迹 8/8 项不同 old_only=[(1073758232,4,1999)] new_only=[(1073758232,4,1998)]`；`页0x40004000: +0x018 OLD=0xCF NEW=0xCE` |
| `firmware/src/11_auth.c` | `while (*auth_retry_counter_ptr < 5)` → `< 4` | `SRAM 2 字节不同，首个 +0x1730 OLD=0x02 NEW=0x01`；`调用序列 5/4 项不一致` |

第三次（`freq_adjust_sync` 的上限钳位）在十二相侧做过，见
`PC12M-2/docs/analysis/PC12M2_INIT_TRACE_2026-11-17.md`。

**第一次跑 `auth_retry` 时，注入 `< 5 → < 4` 竟然仍 PASS** —— 因为当时只覆盖了
`auth_response_status == 0`（认证成功）路径，第一轮就 `counter = 10` 退出，循环次数恒为 1，
`< 5` 与 `< 4` 无从区分。补 `auth_response_status = 1`（认证失败）用例后才真正跑满 5 轮。
**教训：只跑「顺利路径」的函数级 A/B 等于没测，必须覆盖每个行为分支的边界。**

## 7. 覆盖结果

- 新建 `test/emulation/test_init_trace.py`：**24 个唯一原机入口 / 40 组用例，全部通过**。
  - 外设类 26 组（含 `nvic_enable_irq` 0/20/37、`lcd_data_byte` 0x55/0x00/0xFF、
    `i2c_write_byte` 0x00/0xA5/0xFF、`i2c_read_byte` ack 0/1 多档）。
  - SRAM/显示类 14 组（`disp_decimal1`、`disp_screen_static`、`disp_screen_calib`、
    `run_stop_preset` cfg=0/1、`auth_set_timeout`、`auth_retry` 三个分支）。
- 官方套件覆盖缺口：**25 项 / 2502 B → 1 项 / 2 B**（`0x5CA startup_idle_hook` 是空函数，
  新 ELF 已优化掉）。函数覆盖 **86/87 = 98.9%**，字节覆盖 **57480/57482 = 100.0%**。

## 8. 验证结果

- `test/emulation/test_init_trace.py` → 40/40 PASS。
- `python test/run_tests.py` → **36/36 模块**。
- `python tools/verification/verify_firmware_equivalence.py` → 全 PASS
  （VECTOR / PIN_CONFIG 48 / AUTH_CHALLENGE 85 / TIMER1_MATRIX 126 / CRC 5 /
  MODBUS_REGS read=65 write=320 / CLOSED_LOOP 6 / OUTPUT_STAGE 144 /
  STATE_MACHINE 130 / DISPLAY 106 / DISPLAY_FULL_EXEC 4 / RELAY_DIRECT / XO_FONT）。
- 构建未变：`text 59900 / data 0 / bss 2188`，`firmware.bin` 60460 B，
  SHA-256 `B20C67A1994AED331C59CE096DF4903B9B3391676D10CCB051ADA94095FBA92B`。
- 本轮**未发现新的行为差异**（与十二相不同，六相这批函数此前已被逐条静态核对过）。

## 9. 范围说明

- 只做代码与模拟验证，**不含实机验证、不烧写**（用户 2026-11-17 明确限定）。
- 时序与中断上下文仍未证明：Unicorn 里 `Delay` 与渲染体被跳过、`long_delay` 用快进。
- `startup_idle_hook`（2 B 空函数）故意不纳入：新 ELF 已把它优化掉，不存在可对照的符号。
