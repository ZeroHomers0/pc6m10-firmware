# 硬件证据目录

这些资料从原 `docs/doc/` 原样迁移，并于 2026-08-31 按内容重新命名、归类。
文件内容未改动；以下 SHA-256 继续作为完整性基线。

## 目录职责

- `board/`：PC6M-10 整板原理图、BOM、LPC1765/U38 引脚与接线。
- `display/`：CYW-B12864G 显示操作面板手册及其与控制板的接口接线。
- `display/extracted/`：从显示面板资料提取的检索用纯文本，不替代原始 Office 文件。
- `reports/`：基于原理图、BOM 和接口资料形成的综合分析报告。

## 文件清单

| 分类 | SHA-256 | 文件 | 内容 |
|---|---|---|---|
| 整板 | `2338A4E05D74F33715410FCD07102448ECA4E005716F3B5ECFA7C87C07A5C1F1` | `board/PC6M-10_BOM.xlsx` | 物料清单，70 个条目、387 个安装位号 |
| 整板 | `8538828ADDDCFCE9B6760B0B92A0E149F87577E11DECA9CD9F157AF808C3A5A6` | `board/PC6M-10_Schematic.pdf` | PC6M-10 单页整板原理图 |
| 整板 | `742EEC6F7F5AB89299FD05CA2670CBA7CEEFF7DBCBB316A7D3A0DF717482C279` | `board/LPC1765_U38_Pinout_Wiring.xlsx` | U38 全部管脚、网络、方向、分类与功能分组 |
| 显示面板 | `3305FAC09E85E793C92D65F8C73E97C0DF32BF88B6EEF6E13C024690B8DAF2D3` | `display/PC6M-10_CYW-B12864G_Interface_Wiring.xlsx` | P4、牛角头、LCD 与 U38 的连接关系 |
| 显示面板 | `F9838BF4E7D02BC815A60E753CD9F6E7AA7D780E72339B141F898F368790A765` | `display/CYW-B12864G_Operation_Manual.docx` | 面板操作、菜单和参数设置手册 |
| 文本提取 | `42BFFD27BE9DF05C4004556285B672B69E7F9BD257B7AB9C15F0EDB75F26756A` | `display/extracted/PC6M-10_CYW-B12864G_Interface_Wiring.txt` | 接线表 XML 内容提取 |
| 文本提取 | `B01690EF062A27DADCF58256D910BB6402A0C68E45316CDAA428B28608D61AC5` | `display/extracted/CYW-B12864G_Operation_Manual.txt` | 面板手册检索文本 |
| 分析报告 | `DFCF7D475C96B7314E4E6F39C458F4AAF05B786FAACE983C4F675221393DAF02` | `reports/PC6M-10_Hardware_Analysis_Report_2026-07-14.docx` | 控制板硬件结构、接口和证据综合分析 |
