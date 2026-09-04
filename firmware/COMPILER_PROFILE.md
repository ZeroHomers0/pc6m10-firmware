# 固件编译配置

`compiler_flags.txt` 是 Makefile、Git Bash 和 PowerShell 构建入口共用的编译选项
来源。每个入口只负责读取这份文件并补充自己的链接参数，避免三套脚本逐渐产生差异。

当前警告门槛包括：

- `-Wall`、`-Wextra`：基础问题和额外可疑构造；
- `-Wshadow`：避免局部名称遮蔽状态变量；
- `-Wstrict-prototypes`、`-Wmissing-prototypes`：要求跨模块接口有明确原型；
- `-Wundef`：禁止条件编译依赖未定义宏；
- `-Wno-parentheses`：保留反汇编迁移代码中已验证的 `a + b & mask` 表达式，避免
  把运算优先级的样式提示误认为行为问题。

当前活动固件已移除 `-Wno-unused-variable`、`-Wno-unused-but-set-variable` 和
`-Wno-pointer-sign`。编译输出应保持零警告；修改硬件访问或中断代码时仍需运行完整
等价性测试，而不是只以“能编译”为验收条件。
