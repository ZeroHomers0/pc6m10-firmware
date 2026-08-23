# LPC1765FBD100 固件逆向与等价源码工程

本仓库保存 PC6M-10 三相 SCR 控制板 LPC1765 固件的原始 BIN、逆向证据、可编译 GCC 工程、
执行级差分测试和 W8 实机验证流程。

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

当前离线基线：构建尺寸 `text 61936 / data 3000 / bss 2188`，测试 11/11，输出级 144/144，
状态机 115/115。尚未完成整机带载验证。
