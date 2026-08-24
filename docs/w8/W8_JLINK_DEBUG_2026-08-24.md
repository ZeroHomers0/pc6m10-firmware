# W8 J-Link SWD 连不上排查（2026-08-24）

> 前置：本文档是 W8 实机验证的**阻塞排查专项记录**。W8 总入口与唯一执行顺序见
> `W8_ONBOARDING_2026-08-22.md`；硬件接线与阶段标准见 `W8_HARDWARE_TEST_2026-08-22.md`；
> 软件清单与工具操作见 `W8_SOFTWARE_OPERATION.md`。
> **本文只记录「J-Link 无法连接 LPC1765」这一件事的诊断过程、已排除项、剩余嫌疑与仪器方案。**

## 0. 一句话结论

软件层已穷尽、接线确认无误、上电与 VTref 均正常，但 SWD 始终收不到目标应答。**高度怀疑是
SEGGER J-Link OB（2012 年板载旧款）与 LPC1765 的 SWD 兼容性/能力问题。** 待万用表 + 示波器
到位后按本文 §4 的四个阶段做决定性测量。

---

## 1. 环境与对象

| 项 | 值 |
|---|---|
| 目标板 | PC6M V1.0 三相 SCR 移相触发功率控制板 |
| MCU | NXP LPC1765（Cortex-M3，Flash 256 KB） |
| 调试器 | SEGGER J-Link ARM-OB STM32，V7.00，S/N 20090928，**编译于 2012-08-22** |
| J-Link 软件 | J-Link Commander / J-Flash / JLinkARM.dll **V9.70**（2026 版软件，老硬件） |
| 目标固件 | 仓库金标准 `LPC1765.bin` SHA-256 `DD629EAC…3F65`；当前 `firmware.bin` `F032EFB7…AA44` |
| 调试接口 | P12：1=VTref、2=GND、6=SWDIO、8=SWCLK（P12-3 **不是**复位脚） |
| 阶段 | W8 阶段 0（原固件备份）前置 —— 必须先能连上才可读取/备份 |

## 2. 现状症状（已确认）

J-Link Commander（100 kHz）连接日志关键行：

```
Connecting to target via SWD
Connect fallback: Reset via Reset pin & Connect.
Error occurred: Could not connect to the target device.
```

J-Flash（4000 kHz）连接日志关键行：

```
- Device "LPC1765" selected.
- VTarget = 3.300V
- Failed to attach to CPU. Trying connect under reset.
- Connect fallback: Reset via Reset pin & Connect.
- ERROR: Failed to connect.
```

**共性：主机侧（USB / 命令文件 / 设备名 / VTref=3.3V）全部正常；唯独 SWD 无法建立，
且因复位脚未接，connect-under-reset 的 fallback 也必然失败。**

## 3. 已排除项（证据齐全，勿重复）

| # | 排除项 | 说明 |
|---|---|---|
| 1 | SWD 接口与设备名 | `si SWD`、`device LPC1765` 均正确 |
| 2 | 速率 | 10 / 25 / 100 / 400 kHz 结果一致；J-Flash 4000 kHz 也一致 → 非速率 |
| 3 | 命令行直连参数 | `-Device -If SWD -Speed -AutoConnect` 一样失败 |
| 4 | 复位类型 | `RSetType 1`（SWD 内核复位）在 connect 前因「需先连目标」而失败 |
| 5 | SWDIO/SWCLK 交叉 | 已对调，仍失败 |
| 6 | 目标上电 | VTref=3.3V、已上电，仍失败 |
| 7 | 接线逻辑 | 用户已确认线序与针号无误（绿=SWDIO→板 P12-6，SWCLK→P12-8） |
| 8 | Flash/参数/工具 | `Failed to attach to CPU` 是底层 CPU 连接失败，与 Flash、工程参数无关 |

**软件层至此穷尽。** 剩余只能靠硬件测量（§4）区分，且大概率落在「J-Link 能力不足」。

## 4. 仪器到位后的排查方案（万用表 + 示波器）

> 安全：只测 3.3V 逻辑区（P12 附近、MCU 调试脚），普通 10x 探头；**禁止**用普通探头
> 触碰门极驱动 / 强电输出区。

### 阶段 1 —— 万用表：确认目标是否真的活着

直流电压档（20V），黑表笔接板 GND。

| # | 测点 | 正常 | 异常含义 |
|---|---|---|---|
| 1 | P12-1（VTref/3.3V） | ≈3.3V | 此脚无 3.3V |
| 2 | **MCU 主供电 VDD**（板上 3.3V 电感和主电容） | ≈3.3V | **≈0 → MCU 没上电**（优先怀疑） |
| 3 | P12-6（SWDIO）对 GND | 1~3V 上拉 | ≈0V → 线没到/无上拉 |
| 4 | P12-8（SWCLK）对 GND | 1~3V | ≈0V → 同上 |

**要点**：确认 MCU 的 VDD 真的有 3.3V，不只是 P12-1 有（VTref 可能只是采样点）。
板子控制电务必**独立供电**，不靠 J-Link 那根 VTref/VCC 红线借电。

### 阶段 2 —— 万用表：确认复位已释放

- 量 LPC1765 复位脚（nRESET，一般有复位按键/电容）对 GND：**应 ≈3.3V（释放）**。
- 若 ≈0V = 被按在复位态，SWD 会彻底冻结（连不上的常见根因）。

### 阶段 3 —— 示波器：判断「J-Link 在不在工作」（决定性）

- **CH1 挂 SWCLK（P12-8）**，**CH2 挂 SWDIO（P12-6）**，探头地夹夹板 GND。
- 档位：垂直 1 V/div、水平 1 ms/div、上升沿触发 CH1，用 **Single/单次** 捕捉。
- 触发一次 `connect`（用 J-Link 连接），看波形。

| 波形 | 结论 |
|---|---|
| **SWCLK 有连续时钟方波、SWDIO 有翻转** | J-Link 正常驱动，目标不应答 → 查目标侧（供电/复位/上拉） |
| **SWCLK 无任何时钟** | J-Link 没发信号 → 基本坐实 **J-Link 兼容性/故障** |
| 有时钟但**幅度仅 1~2V、歪斜** | 电平/上拉/驱动强度问题 → 查上拉与走线 |

### 阶段 4 —— 按结果收敛

- 阶段3 =「J-Link 完全没发时钟」→ **换调试器**（J-Link V9 或几十元 CMSIS-DAP / ST-Link V2）。
- 阶段3 =「J-Link 发了、目标不动」→ 用万用表查 SWDIO/SWCLK 上拉；无上拉且 MCU VDD 正常，
  则 **外接 10k 上拉（SWDIO→3.3V）** 试；再查复位释放、MCU VDD 真到位 —— 此类多属**可修**。

---

## 5. 记录回填模板

```text
[阶段 1/2/3/4]
仪器型号：
板卡与目标固件哈希：
控制电供电（独立 or J-Link 借电）：
实测电压/波形：
预期值：
判定：PASS / FAIL / 未确认
附件：照片、波形截图
操作者与日期：
```
