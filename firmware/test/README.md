# firmware/test — host 虚拟数据测试

> 用**构造的虚拟数据**对反编译固件的**纯逻辑函数**做行为等价验证，
> 不依赖真实硬件、不烧录。以**原始二进制 + 反汇编金标准**为基准，
> 不是教科书规范（避免把原固件行为怪癖误判为 bug）。

## 运行

```bash
cd firmware/test
PYTHONUTF8=1 python run_tests.py           # 跑全部 test_*.py
PYTHONUTF8=1 python run_tests.py crc16     # 只跑名字含 crc16 的
```

每个测试模块暴露 `main()` 返回 0=全过 / 非0=有失败。输出 UTF-8。

## 测试清单（2026-08-23 首轮）

| 测试 | 被测函数 | 结果 | 验证内容 |
|---|---|---|---|
| `test_crc16_semantics.py` | `crc16` (0xAF64) | ✅ 9/9 | ① CRC 表==原始 bin 0x11034/0x11134（S9 修复）；② `len-1` 先减后终检语义；③ 真实 Modbus 帧字节序 |
| `test_modbus_regmap.py` | `modbus_read_reg`(0xAF94)/`modbus_write_multi`(0xB2E0) | ✅ 5/5 | ① 读写同 reg 映射同地址；② 位宽一致；③ 保留区(0x1A-1F/24-25)读返回0/写落 g_scratch |
| `test_param_sync.py` | `param_sync_live_to_eeprom` (0x35F2) | ✅ 6/6 | ① 符号真存在于 globals.c；② EEPROM reg 无冲突；③ 仅不等才写；④ 16 位分高低两次写 |
| `test_unicorn_crc16.py` | 编译 crc16@0x91ac | ✅ 3/3 | **真实执行**（Unicorn 加载 firmware.elf）：编译产物 crc16 == Python 模型 |
| `test_unicorn_param_sync.py` | 编译 param_sync@0x3478 | ✅ 2/2 | **真实执行**：live≠shadow 触发 i2c_write_reg + 写后 shadow=live（hook i2c_write_reg 拦截 GPIO） |

## Unicorn 真实执行（已可用，2026-08-23）

- **`unicorn_harness.py`**：加载 `firmware.elf`，把 FLASH(0x0)/SRAM0(0x10000000)/SRAM1(0x2007C000)
  映射为真实可读写内存（天然解决 g_/DAT_ 指针 SRAM 重定向），SP=0x10006768，按 AAPCS 传参、
  回读副作用。`pip install unicorn`（需代理，15.9MB wheel）。
- 可执行**编译产物**的最硬验证。外设 GPIO RMW（FIO/定时器）不可直接仿真 → 用
  `UC_HOOK_CODE` 拦 i2c_write_reg 等入口点跳过硬时序、记录调用参数。

## 关键发现：crc16 的 `len-1` 语义（重要）

`crc16()` 循环是 `while((len=(len-1)&0xff)!=0)`：**先减后终检**，只处理 `len-1` 字节、
丢弃第 len 个字节。这是 0xAF64 原始机器码的忠实还原（0xaf86 `sub r4,#1` → `uxtb` → `bne`
循环体）。调用方 `modbus_dispatch` 传 `crc16(FRAME, rx_len-2)`（0xB642 `subs r0,#2` → `uxtb r1`
→ `bl`）。因此校验一个数据体 N 字节的帧时，固件实际只覆盖 **N-1** 个字节。

**测试基准用原始二进制真值**，不用教科书 Modbus CRC——这正是为了把"原固件行为"从
"反编译 bug"里区分出来。

## 方法与局限

- **纯逻辑函数**（无外设 RMW、无硬实时时序）既可模型测试（从 .c 提取），也可 unicorn 真实执行。
- crc16 / modbus_read_reg / modbus_write_multi / param_sync 都是纯 SW 逻辑 → 两路皆可测。
- **外设硬时序**（TIMER 触发角、FIO 位带写、GPIO 协议时序）无法在 unicorn 仿真——这些是
  上机行为风险的来源，靠 W7 静态对照 + W8 硬件实测覆盖，**静态/仿真证明不了**。
- 现有两类测试互补：
  - **模型测试**从 .c 源码提取行为表 + 构造虚拟数据 + 断言不变量（快、覆盖全部参数）。
  - **unicorn 执行测试**真正跑编译的 Thumb 产物，验证"反编译 C→编译→机器码→执行"闭环。
