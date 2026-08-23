# 固件离线测试

统一入口：

```bash
python test/run_tests.py
python test/run_tests.py crc16
```

`run_tests.py` 递归发现测试，并保持按名称过滤能力。

- `static/`：CRC 表与语义、Modbus 寄存器映射、参数同步结构检查。
- `emulation/`：使用 Unicorn 直接执行原始 `LPC1765.bin` 与 `firmware.elf`，比较返回值和副作用。
- `support/unicorn_harness.py`：ELF/BIN 加载、符号查询、内存种子和 A/B 差分公共支持。

当前共 11 个测试模块，必须全部通过。独立的更大矩阵不在本目录重复实现，入口为
`tools/verification/verify_firmware_equivalence.py`。
