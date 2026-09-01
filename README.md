# LPC1765FBD100 固件逆向与等价源码工程

本仓库保存 PC6M-10 三相 SCR 控制板 LPC1765 固件的原始 BIN、逆向证据、可编译 GCC 工程、
执行级差分测试和实机烧写/验证记录（**实机测试已通过**）。

## 快速开始

- AI/维护人员先读 `AGENTS.md`。
- 全部文档入口见 `DOCUMENTATION_INDEX.md`。
- 当前实现见 `firmware/README.md`。
- 原始证据见 `evidence/README.md`。
- 测试说明见 `test/README.md`。

```bash
cd firmware && bash build.sh && cd ..
python test/run_tests.py
python tools/verification/verify_firmware_equivalence.py
```

当前基线：构建尺寸 `text 62840 / data 3000 / bss 2188`，`firmware.bin` SHA-256
`C6D3F35DD6C5A451947C27BA1825D8CE35C43D031F4EA02E32A9438BB32AE74E`，静态测试 25/25，
A/B 矩阵全 PASS（输出级 144、状态机 130、TIMER1 126、Modbus 65 读 + 320 写）。
**实机测试已通过**（用户直接实机验证；W8 分级实测流程未按计划执行、已废弃，见 `AGENTS.md`）。
