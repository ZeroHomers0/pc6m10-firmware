# 固件离线测试

统一入口：

```bash
python test/run_tests.py
python test/run_tests.py crc16
```

`run_tests.py` 递归发现测试，并保持按名称过滤能力。

## 测试模块清单（13 个，必须全部通过）

### static/ — host 静态/结构检查（不执行机器码）

| 文件 | 覆盖 |
|---|---|
| `test_crc16_tables_and_semantics.py` | CRC 表逐字节 vs 原 BIN flash 表；`crc16` 长度语义（处理全部 len 字节） |
| `test_modbus_register_map.py` | Modbus 读写映射对称：read/write 同 SRAM 地址、同位宽、保留区落 g_scratch |
| `test_parameter_sync_structure.py` | 参数 live→EEPROM 同步结构（shadow 复制 + i2c_write 模式） |

### emulation/ — Unicorn 执行级验证（A/B 差分 或 真执行编译产物）

| 文件 | 类型 | 覆盖 |
|---|---|---|
| `test_adc_wait_done_equivalence.py` | A/B 差分 | `adc0_wait_done` 读 AD0GDR 回归护栏（W8 指针坑 bug） |
| `test_closed_loop_equivalence.py` | A/B 差分 | `closed_loop_integral` PID（除数表/死区/钳位） |
| `test_closed_loop_wrapper_equivalence.py` | A/B 差分 | `closed_loop_wrapper` 重算/回绕不重算两分支 |
| `test_crc16_against_model.py` | 真执行 | 编译 `crc16` vs Python 模型 |
| `test_crc16_firmware_equivalence.py` | A/B 差分 | `crc16` 原始 vs 编译 |
| `test_input_scan_equivalence.py` | A/B 差分 | `input_scan_state` FIO 扫描回归护栏（W8 指针坑 bug） |
| `test_modbus_dispatch_execution.py` | 真执行 | `modbus_dispatch` 帧处理（合法帧/CRC 错帧，hook uart3_tx_byte） |
| `test_modbus_read_register_equivalence.py` | A/B 差分 | `modbus_read_reg` reg 0x00-0x3F |
| `test_modbus_write_multi_equivalence.py` | A/B 差分 | `modbus_write_multi` reg 0x00-0x3F |
| `test_parameter_sync_execution.py` | 真执行 | `param_sync_live_to_eeprom`（hook i2c_write_byte） |

### support/

- `unicorn_harness.py`：ELF/BIN 加载、符号查询、内存种子和 A/B 差分公共支持。

## 规则

- 修改源码后必须运行对应回归测试；A/B 差分测试是「原始 vs 编译」执行级等价护栏。
- 独立的更大验证矩阵不在本目录重复实现，主入口为 `tools/verification/verify_firmware_equivalence.py`。
