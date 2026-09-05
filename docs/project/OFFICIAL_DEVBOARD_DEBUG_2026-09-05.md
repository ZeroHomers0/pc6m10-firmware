# 官方 LPC1769 开发板 VS Code 源码级调试（仅官方开发板）

> ⚠️ **适用范围：仅官方 NXP LPC1769 开发板**（板载 CMSIS-DAP，经板载 micro-USB 连接）。
> **不适用于 PC6M-10 目标板**——目标板的调试/烧写用 J-Link，见 `AGENTS.md`、`操作文档.md`
> 与 `tools/jlink`，本文件结论对其不适用。
>
> 日期：2026-09-05
> 分支：firmware-readability-2026-09-04
> 相关代码位置：`.vscode/launch.json`、`.vscode/tasks.json`

## 1. 背景与结论速览

官方 LPC1769 板（LPC1768/1769 与目标芯片同属 Cortex-M3 r2p0、512KB flash，可直接充当
本固件的调试载体）经过一整轮实机验证，最终可用的源码级调试方式是：

- **后端 = LinkServer gdbserver**（NXP 原生驱动）；pyocd 在板载 CMSIS-DAP 上不可靠，已弃用。
- **工作流 = 物理断电重启 + Cortex-Debug attach**（免烧写、免调试器复位）。
- 两块 VS Code 配置见 §3，日常用 attach 配置即可；配置已配好并实测通过（2026-09-05）。

## 2. 环境清单

| 项 | 值 |
|---|---|
| 硬件 | NXP LPC1769 官方板，板载 CMSIS-DAP，USB 枚举名 "LPC-Link2"，探针 UID `17009011`，**经板上 micro-USB 连接** |
| 调试软件 | LinkServer `D:\software\LinkServer_26.6.137\dist\LinkServer.exe` |
| IDE | VS Code + Cortex-Debug 1.12.1（无原生 linkserver，用 `servertype:"external"`） |
| GDB | `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gdb.exe` |
| 固件 | `firmware/firmware.elf`（`-g` 已加入，源码级单步所需；对二进制产物无影响） |

## 3. 已验证配方

### 3.1 LinkServer gdbserver 启动命令

~~~bash
D:\software\LinkServer_26.6.137\dist\LinkServer.exe gdbserver LPC1765: --gdb-port 3336 -p 17009011
~~~

要点：
- 设备名用 **`LPC1765:`**（非 LPC1769）在这块板上烧写/复位/连接全部可用，为实测定论；
- gdbserver 在 gdb 客户端断开后自动退出（每个 F5 会话由 `tasks.json` 的 preLaunchTask 重新拉起）；
- 端口 3336；`-p` 指定板载探针 UID。

### 3.2 `.vscode/launch.json` 关键配置

- **配置① attach（推荐日常）**：`request:"attach"`、`servertype:"external"`、
  `gdbTarget:"localhost:3336"`、**`overrideAttachCommands:[]`**。
  > 必须为空：LinkServer **不支持 `monitor halt`**（cortex-debug 默认 attach 会发它 → launch 失败）；
  > LinkServer 在 gdb 连接时已自动 halt，无需再停。
- **配置② 烧写+复位跑到 main**：同样是 attach 容器 + `overrideAttachCommands:[]` +
  `postAttachCommands:["load","monitor reset","tbreak main","continue"]`。改固件后想一版到位时用。
- 两个配置都带 `preLaunchTask` = `tasks.json` 里的 "LinkServer gdbserver (官方板 17009011)"
  （`type:"process"` 后台任务，匹配 "GDB server listening on port" 即绪）。

完整内容以 `.vscode/` 下实际文件为准。

## 4. 使用步骤（标准工作流）

1. **物理断电重启**官方板（拔 micro-USB，等 2 秒，插回）——固件自然 boot，Boot ROM 全程不被
   调试器打扰（与 `AGENTS.md`"重启一律物理断电"铁律一致）。
2. VS Code 按 `F5` 选 **配置① attach**。
3. 调试会话 halt 在 `main()` 源码行，`F10` 单步、`F9` 断点、变量/外设查看均可用。

改完固件要烧新版：用配置②（自动烧写→软复位→跑 main）；若报 `Error finishing flash operation`
（烧写收尾软复位的间歇故障），**直接再按一次 F5**——flash 内容已写好、sector 会跳过，基本必过。

## 5. 关键硬件结论（为何只能 attach）

反复实测（pyocd 与 LinkServer 均复现）后定型：

- **LPC176x 复位后芯片必经 Boot ROM**（入口约 `0x1fff0084`）。LinkServer `monitor reset` 显示
  PC=0x300 只是其"复位到镜像入口"的表象，一旦 resume，硬件实际从 Boot ROM 启动。
- **单步进入 Boot ROM 约 4 条指令后 SWD 必挂**（约 `0x1fff0090` 的引导源采样代码区）。
- **全速跑过 Boot ROM 是瞬时噪音**：复位→`tbreak main`→`continue`，核心能靠 FPB 停到 main 入口
  `0x5bc`；仅 halt 瞬间一次可恢复的寄存器读取错误（gdb remote reply '22' / LinkServer
  `request to clear DAP error failed - status 5`）。
- flash loader 软复位后停在 Reset_Handler `0x300` 是**伪 halt**（单步不前进），不可用。
- **结论**：调试器触发的复位（任何工具、任何方式）在这块板上都无法可靠地"复位后从第一行单步"；
  **物理断电才是唯一可靠复位**，因此调试模型为"断电 boot 完 → attach"。
- 固件跑过 `adc_init`（P1.30/31 复用）**不杀**官方板 SWD：attach 后 halt 在 main 节拍等待循环
  （约 `0x6ac`）可正常读写、单步。

## 6. 已知限制与注意事项

- **烧写间歇**：gdbserver `load` 偶尔在收尾软复位交接时报 `Error finishing flash operation`
  （phase 实测 ~1/3 概率），重试即过。
- **局部变量多显示 `<optimized out>`**：固件 `-Os` 编译。需要直接看变量值时，另做 `-O0`
  调试固件（独立产物，不影响 `-Os` 正式固件与 A/B 验证）。
- **pyocd 弃用原因**：板载 CMSIS-DAP 上 flash 算法 HardFault（IPSR=3）、SWD No ACK、丢链；
  LinkServer 烧写干净（~31-38 KB/s）、attach/单步稳定。
- gdb 控制台偶见 `could not convert 'uint8_t'... (CP1252)` 警告：Windows 控制台编码噪声，无碍。

## 7. 与 PC6M-10 目标板的界线（重申）

本文件所有设备名、探针、复位/SWD 行为、烧写与调试步骤**仅对官方 LPC1769 开发板有效**。
PC6M-10 目标板（LPC1765）的调试/烧写一律走 `AGENTS.md` 规定的仓库自带 `tools/jlink/JLink.exe`
与 `操作文档.md` 流程，不要套用本文件结论。
