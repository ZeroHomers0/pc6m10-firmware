# 工具目录

- `audit/`：只读独立审计。
- `generation/`：从原始 BIN/反编译证据生成源码或报告。
- `ghidra/`：Ghidra Java/Jython 辅助脚本。
- `jlink/`：免安装 J-Link 最小集（`JLink.exe` + DLL + USB 驱动）。**`JLink.exe` 是全项目
  J-Link 唯一调用路径**，无需安装 J-Link 软件；`flash.bat`/`install_deps.bat` 一律从这里调用。
- `maintenance/`：源码符号、位宽和常量的机械维护工具。
- `verification/`：当前离线验证入口；主入口为 `verify_firmware_equivalence.py`。
- `w8/`：实机阶段的串口、Modbus 和波形辅助脚本。
- `archive/`：一次性历史工具，仅供追溯，不应作为当前流程入口。

所有当前脚本必须从脚本位置推导项目根目录，禁止再写死 `D:\code\...`。
