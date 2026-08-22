# SETUP_WINDOWS — 另一台电脑从零搭好目标B环境

> 日期：2026-08-22 ｜ 用在哪：把「能编译 firmware + 跑 W8 实测」的环境搬到**另一台 Windows 电脑**
> 一键脚本：`tools/setup_windows.bat`（双击）或 `tools/setup_windows.ps1`（PowerShell）
> 配套：`W8_SOFTWARE_OPERATION.md`（W8 要哪些软件、每步怎么操作）

---

## 0. 你要让另一台电脑能干嘛

这台电脑上只需要**两类活**，它对应的环境完全不同：

| 要做的活 | 需要什么 | 有没有强电风险 |
|---|---|---|
| **A. 编译固件**（改代码→出 `firmware.hex/.bin`） | ARM GCC 工具链 + Git Bash + Python | 无（纯编译） |
| **B. W8 硬件实测**（读出电器数据、抓波形、下载烧录） | USB-RS485 + 示波器 + 信号发生器 + Python(minimalmodbus) | **有(L3 阶段)** |

- 如果你只是**改代码/编译**，装前面的工具链 + Python 就够（下面 1~4 项）。
- 如果你要**上真机实测**（W8），还要预留那批**硬件仪器**（见 §分页 `W8_SOFTWARE_OPERATION.md`）。

> ⚠ 注意：另有一类脚本（`tools/*.py` 的**逆向**工具 `create_isr_functions.py` 等）依赖 **Ghidra 的 Jython**（`from ghidra.*`），那是**逆向**电脑才用的；**你另一台只做编译+实测，不需要装 Ghidra**。其余 verify_ 脚本是纯标准库，可直接跑。

---

## 1. 需要装什么（一句话版）

| # | 软件 | Winget 包名 | 为什么 |
|---|---|---|---|
| 1 | **Git for Windows** | `Git.Git` | 提供 `bash`（build.sh 是 bash 脚本，必需） |
| 2 | **ARM GNU Embedded Toolchain** | `Arm.GnuArmEmbeddedToolchain` | `arm-none-eabi-gcc` 编译 Cortex-M3 → hex/bin |
| 3 | **Python 3.13** | `Python.Python.3.13` | 跑验证脚本 + W8 Modbus 测试 |
| 4 | **(pip) minimalmodbus + pyserial** | 由 pip 装 | W8 阶段 B 的 Modbus 通信 |

> 这些都可从 **winget**（Windows 官方包管理器）一键获得，见下面两种方式。

---

## 2. 方式 A：一键脚本（推荐，最省事）

**在另一台电脑上：**
1. 把 `decompiled` 整个项目目录拷过去（含 `firmware/`、`tools/`、`LPC1765.bin`、`docs/`…）。
2. 双击 `decompiled\tools\setup_windows.bat`（会弹 UAC 权限确认，点**是**）。
3. 脚本会自动：
   - 装 Git + ARM GCC + Python（用 winget）
   - 用 pip 装 minimalmodbus + pyserial
   - **自动定位 `arm-none-eabi-gcc.exe` 实际路径，改写 `firmware/build.sh` 里的 `TC=` 一行**（这样即使版本号目录不同也不会编译失败）
   - 尝试跑一次 `bash build.sh` 验证
4. 看到最后一行提示后，**重开一个终端**（让新命令进入 PATH），进入 `firmware/` 跑 `bash build.sh`。

### 脚本会自动处理的坑
- **`build.sh` 的路径是硬编码的**：`TC="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin"`。另一台电脑版本号可能不同 → 不改就会编译失败。脚本安装后**自动改写这一行**为它找到的真实路径。
- **PATH 未刷新**：刚装完的命令当前终端用不了 → 脚本最后提醒你重开终端。
- **UAC 权限**：安装软件需管理员，脚本会自动提权。

---

## 3. 方式 B：手动安装（想自己看每一句在干嘛）

在 **PowerShell**（管理员）里依次：

```powershell
# 1. Git for Windows（提供 bash）
winget install --id Git.Git -e --accept-package-agreements --accept-source-agreements

# 2. ARM GCC 工具链
winget install --id Arm.GnuArmEmbeddedToolchain -e --accept-package-agreements --accept-source-agreements

# 3. Python 3.13
winget install --id Python.Python.3.13 -e --accept-package-agreements --accept-source-agreements
```

然后**重开终端**（让 python 进入 PATH），装 pip 包：

```powershell
python -m pip install minimalmodbus pyserial
```

接着**改 `firmware/build.sh` 顶部的 TC 路径**（关键，否则编译失败）。找到 `TC=` 那一行，改成你的实际工具链 **bin** 目录，把正斜杠和 `/c/` 风格写对。例如：

```bash
TC="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin"
```

> 看你的 ARM GCC 装到哪：`ls "/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/"`，把 `14.2 rel1` 换成你的版本号目录。

---

## 4. 验证（确认环境真的能用）

用 **Git Bash**（装完 Git 后开始菜单里就是它）跑到项目根 `decompiled`：

```bash
# ① 编译固件（最核心的验证）—— 在 firmware 目录
cd /d/code/LPC1765FBD100/decompiled/firmware
bash build.sh
# 预期末尾： OK: firmware.elf / firmware.hex / firmware.bin
# 正常应看到 text 60268  data 3388  bss 2188
```

```bash
# ② 跑无硬件仿真验证脚本（纯标准库，确认逆向结论数据一致）
cd /d/code/LPC1765FBD100/decompiled
python tools/verify_startup.py      # 启动链路 / 波特率表 / CRC16 表 → PASS
python tools/verify_strpool.py      # 字符串表映射 → PASS
```

```bash
# ③ 确认工具链版本
arm-none-eabi-gcc --version   # 应显示 14.x
python --version              # 应显示 Python 3.13.x
```

> ①②都 PASS 且 `build.sh` 出 `OK`，环境就绪。若没有 `bash` 命令 → 你没装 Git for Windows / 没在 Git Bash 里。

---

## 5. 常见坑

| 现象 | 原因 | 解决 |
|---|---|---|
| `bash: command not found` | 没装 Git，或没在 Git Bash 里跑 | 装 Git；改从"Git Bash"窗口进入 |
| 编译报 `/c/Program Files...: 权限` 或找不到 gcc | `build.sh` 的 `TC=` 路径不对（版本号目录不同） | 见 §3 改 `TC=` 为真实 bin 目录；或用一键脚本自动改写 |
| `python: command not found` | 刚装完 Python，PATH 未刷新 | 重开终端；或直接用完整路径 |
| 链接时报 `-lgcc` 找不到 | 工具链没装全 | 重装 `Arm.GnuArmEmbeddedToolchain` |
| `setup_windows.bat` 报 winget 不存在 | 系统太老/没应用安装程序 | 从 Microsoft Store 装"应用安装程序"，或手动装（§3） |
| 提示需管理员权限 | 安装软件要 UAC | 脚本会自动提权；手动则右键"以管理员运行" |

---

## 6. Q&A

**Q：另一台电脑要不要装 Ghidra？**
A：只做编译 + W8 实测**不需要**。Ghidra 只在逆向那台电脑上用（分析原 bin）。

**Q：能不能不装 Python？**
A：编译固件不需要 Python（build.sh 是 bash+gcc）。但**跑 verify_ 验证脚本、W8 的 Modbus 测试**需要。建议装上。

**Q：脚本装到默认路径还是自定义？**
A：winget 装到默认路径（Git → `C:\Program Files\Git`、ARM GCC → `C:\Program Files (x86)\Arm GNU Toolchain...`、Python → `%LOCALAPPDATA%\Programs\Python\...`）。脚本能自动探测并改写 build.sh，所以默认即可。

**Q：脚本装完还要做什么？**
A：**重开终端** → 进 `firmware/` 跑 `bash build.sh` 看是否 OK（§4）。之后就可用这个环境编译固件了。

---

## 7. 下一步：上真机实测（W8）

环境装好后，如果要接真机（示波器/RS485），按 `W8_SOFTWARE_OPERATION.md` 安装硬件配套软件、按 `W8_HARDWARE_TEST_2026-08-22.md` 的清单逐步实测。
