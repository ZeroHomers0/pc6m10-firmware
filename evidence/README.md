# 原始证据目录

本目录保存 AI 和人工重新核验结论所需的原始证据，不能因为“当前未引用”而删除。

- `hardware/`：BOM、接线表、面板手册、原理/分析文档及其文本提取。
- `reverse/decompiled/`：Ghidra 原始 C 级反编译存档。
- `reverse/disassembly/`：完整函数反汇编和历史反汇编转储。
- `reverse/state_machine/`：状态机精读过程产物；下划线文件是历史人工分析记录，不是当前权威结论。
- `reverse/reports/`：数据段、全局变量、字符串池等可重建报告。

当前可编译实现位于 `../firmware/`，验证工具位于 `../tools/verification/`。
