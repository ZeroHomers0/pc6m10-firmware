# PC6M 可读性重构记录

日期：2026-09-04
分支：firmware-readability-2026-09-04

## 1. 重构目标

在不改变原固件关键行为、SRAM 地址、外设地址、访问宽度和初始化顺序的前提下，把
反编译产物整理为便于人类阅读、维护和继续开发的 C 工程。

本轮重点处理：

- 移除业务源码中的 DAT_、PTR_、g_、iVar、uVar、local_ 等反编译命名；
- 将地址映射按功能重新组织并改为语义名称；
- 将参数区、运行时状态、公共接口和驱动模块分层；
- 删除旧的集中式 globals.c / globals.h；
- 保持原固件的地址、位宽、指针偏移和调用关系；
- 让构建、测试和验证脚本适配新的源码组织。

## 2. 地址映射重构

原先由 globals.c / globals.h 集中承载的地址映射，现按用途拆分为：

| 文件 | 内容 |
|---|---|
| firmware/inc/firmware_state.h | 运行时状态、输入扫描、LCD、ADC、时钟、外设和通信状态 |
| firmware/inc/firmware_parameters.h | 配置银行、实时参数、EEPROM 镜像和参数同步地址 |
| firmware/inc/firmware_api.h | 模块间公共函数声明和统一头文件入口 |

语义宏只改变源码表达方式，不会创建新的 RAM 对象。例如：

~~~
#define parameter_control_method_ptr ((volatile uint8_t *)0x10001655u)
#define parameter_frequency_adjust_ptr ((volatile uint32_t *)0x1000162Cu)
~~~

这些名称直接表达用途，同时保留原始地址和访问宽度。

## 3. 文件与结构调整

- 删除 firmware/globals.c。
- 删除 firmware/inc/globals.h。
- 从 Makefile、build.sh、build.ps1 中移除 globals.o 的编译和链接。
- types.h 不再提供 undefined1/2/4/8、byte、uint 等 Ghidra 兼容别名，
  业务代码统一使用标准固定宽度类型。
- 01—13 业务模块继续按启动、显示、输入、I2C、ADC、参数、状态机、Modbus、UART、
  输出、认证、闭环和 GPIO 初始化分工。
- 已删除 stub.c；频率微调和 UART3 接收组帧分别归入 `src/06_frequency_adjust.c`
  与 `src/08_uart3_receive.c`，构建入口不再编译占位对象。

## 4. 验证工具和文档同步

已同步修改：

- 读宽检查脚本改为读取 firmware_state.h 和 firmware_parameters.h；
- 外设地址交叉引用脚本改为解析语义地址宏；
- 参数同步、Modbus 和执行级仿真测试改用语义变量说明；
- 软件设计文档生成脚本、工程 README、工具 README 和逆向总指南更新为新目录结构；
- W8 辅助脚本中的旧地址说明改为“历史地址证据槽”表述。

历史逆向反汇编、原始 BIN、历史报告和一次性迁移工具没有删除。它们用于证明地址来源、
访问宽度和行为等价性，不属于当前固件源码接口。

## 5. 行为保持原则

本轮属于可读性和结构重构，不主动改变控制逻辑。以下内容保持原样或按已有证据恢复：

- 绝对地址和 SRAM 布局；
- uint8_t / uint16_t / uint32_t 访问宽度；
- 指针解引用与 (*pointer)++ 自增语义；
- UART3 外设寄存器和波特率表地址；
- 输入扫描的字节偏移；
- 启动时 .data 镜像、.bss 清零和中断向量顺序；
- LCD、ADC、I2C、Modbus、状态机和闭环函数的调用链。

早期分支中曾经存在的输入扫描地址缩放和 5 处 SRAM 读宽问题，已经在本分支建立前的
历史修复提交中处理；本轮只完成命名和结构迁移，并通过回归测试确认没有重新引入。

## 6. 验证结果

构建命令：

~~~
pwsh -NoProfile -ExecutionPolicy Bypass -File firmware/build.ps1
~~~

结果：

~~~
text 58592  data 0  bss 2188  dec 60780  hex ed6c
OK: firmware.elf / firmware.hex / firmware.bin / firmware.map
~~~

构建使用统一的 `firmware/compiler_flags.txt`，已启用 `-Wextra`、`-Wshadow`、
`-Wstrict-prototypes`、`-Wmissing-prototypes` 和 `-Wundef`，并在清理对应源码后保持零警告。

完整测试：

~~~
[run_tests] 模块通过 26/26
~~~

验证工具结果：

- `verify_firmware_equivalence.py`：全部项目通过，包括输入、UART、ADC、I2C、Modbus、
  闭环、输出级、状态机、显示和继电器；`DISPLAY_FULL_EXEC` 4/4 通过。
- `verify_periph_xref.py`：通过；金标准中未直接出现在 C 文本的地址均已归类为宏或别名覆盖。
- `verify_readwidth_all.py`：通过；无未解释的读宽不一致，16 位参数的合法分字节访问共 59 处。
- `git diff --check`：通过。

显示全执行验证比较真实 LCD 命令/数据总线字节序列及 SRAM 结果。Unicorn 对 GPIO 读改写的
写入值受链接布局影响，因此不把仿真器的 RMW 中间值误判为固件差异；GPIO 叶函数和输出
写序列仍由独立轨迹测试覆盖。

## 7. 活动代码边界与历史证据

本轮实施完成后，活动固件源码和正式构建入口已经不再使用 `DAT_`、`PTR_`、`g_`、
`undefined*`、`iVar`、`uVar`、`local_` 或未说明的 `func_0x...` 命名；
`firmware/globals.c`、`firmware/inc/globals.h` 和 `firmware/stub.c` 已删除。

历史证据中的原始符号没有批量抹除，也没有删除逆向来源文件。这样可以同时满足：

1. 当前可编译源码、构建入口和验证入口使用人类可读命名；
2. 未来仍能从原始 BIN、反汇编和历史脚本追溯每个地址与访问宽度；
3. 不因清理名称而丢失已经验证过的证据链。

历史反汇编、原始 BIN、历史报告和一次性迁移工具属于证据或维护资产，不属于固件编译输入；
测试中用于表达原始地址来源的说明也保留必要的物理地址，但不再依赖旧符号作为代码接口。
