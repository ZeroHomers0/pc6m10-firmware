# -*- coding: utf-8 -*-
"""活动固件源码的人工可读性契约。

该测试只扫描当前源码、公共头文件和构建入口，不扫描 evidence、历史文档或一次性迁移工具。
历史反编译名称可以作为证据保留，但不能重新进入可编译固件。
"""
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]

SOURCE_FILES = (
    sorted((ROOT / "firmware" / "src").glob("*.c"))
    + sorted((ROOT / "firmware" / "inc").glob("*.h"))
)
BUILD_FILES = (
    ROOT / "firmware" / "Makefile",
    ROOT / "firmware" / "build.sh",
    ROOT / "firmware" / "build.ps1",
)

FORBIDDEN_PATTERNS = (
    (re.compile(r"\bDAT_[A-Za-z0-9_]*\b"), "DAT_"),
    (re.compile(r"\bPTR_[A-Za-z0-9_]*\b"), "PTR_"),
    (re.compile(r"\bg_[A-Za-z0-9_]+\b"), "g_"),
    (re.compile(r"\b(?:iVar|uVar|puVar|cVar|bVar|local_|param_[0-9]+)\b"), "反编译局部名"),
)

ALLOWED_COMPATIBILITY_SYMBOLS = set()


def check_source_names():
    issues = []
    for path in SOURCE_FILES:
        text = path.read_text(encoding="utf-8", errors="ignore")
        relative_path = path.relative_to(ROOT).as_posix()
        for pattern, label in FORBIDDEN_PATTERNS:
            for match in pattern.finditer(text):
                line_number = text.count("\n", 0, match.start()) + 1
                issues.append(f"{relative_path}:{line_number}: {label} {match.group(0)}")
        for match in re.finditer(r"\bfunc_0x[0-9A-Fa-f]+\b", text):
            if match.group(0) not in ALLOWED_COMPATIBILITY_SYMBOLS:
                line_number = text.count("\n", 0, match.start()) + 1
                issues.append(f"{relative_path}:{line_number}: 未说明的兼容符号 {match.group(0)}")
    return issues


def check_build_inputs():
    issues = []
    for path in BUILD_FILES:
        text = path.read_text(encoding="utf-8", errors="ignore")
        relative_path = path.relative_to(ROOT).as_posix()
        for legacy_name in ("globals.c", "globals.h", "globals.o"):
            if legacy_name in text:
                issues.append(f"{relative_path}: 构建入口仍引用 {legacy_name}")
    return issues


def check_removed_files():
    issues = []
    for relative_path in ("firmware/globals.c", "firmware/inc/globals.h", "firmware/globals.o"):
        if (ROOT / relative_path).exists():
            issues.append(f"{relative_path}: 旧文件或构建残留仍存在")
    return issues


def check_display_and_input_names():
    issues = []
    display_header = ROOT / "firmware" / "inc" / "firmware_display_strings.h"
    input_header = ROOT / "firmware" / "inc" / "firmware_input_pins.h"
    if not display_header.exists():
        issues.append("firmware/inc/firmware_display_strings.h: 显示文本地址表不存在")
    if not input_header.exists():
        issues.append("firmware/inc/firmware_input_pins.h: 输入引脚掩码表不存在")

    display_sources = (
        ROOT / "firmware" / "src" / "01_startup.c",
        ROOT / "firmware" / "src" / "02_lcd_display.c",
        ROOT / "firmware" / "src" / "06_frequency_adjust.c",
        ROOT / "firmware" / "src" / "07_state_machine.c",
    )
    for path in display_sources:
        text = path.read_text(encoding="utf-8", errors="ignore")
        relative_path = path.relative_to(ROOT).as_posix()
        if '"inc/firmware_display_strings.h"' not in text:
            issues.append(f"{relative_path}: 未引入显示文本地址表")
        for match in re.finditer(r"\bdisp_string\s*\(\s*(?:\(int\)\s*)?0x[0-9A-Fa-f]+", text):
            line_number = text.count("\n", 0, match.start()) + 1
            issues.append(f"{relative_path}:{line_number}: disp_string 仍直接使用 flash 地址")

    input_source = ROOT / "firmware" / "src" / "03_input_debounce.c"
    input_text = input_source.read_text(encoding="utf-8", errors="ignore")
    if '"inc/firmware_input_pins.h"' not in input_text:
        issues.append("firmware/src/03_input_debounce.c: 未引入输入引脚掩码表")
    for match in re.finditer(r"FIO[0-3]->PIN\s*&\s*0x[0-9A-Fa-f]+", input_text):
        line_number = input_text.count("\n", 0, match.start()) + 1
        issues.append(f"firmware/src/03_input_debounce.c:{line_number}: 输入扫描仍直接使用 GPIO 掩码")
    for match in re.finditer(r"\bsm[3456]_[A-Za-z0-9_]*\b", input_text + "\n" + (ROOT / "firmware" / "src" / "07_state_machine.c").read_text(encoding="utf-8", errors="ignore")):
        issues.append(f"状态机仍保留反编译辅助函数名 {match.group(0)}")
    return issues


def check_style_and_warning_profile():
    issues = []
    style_files = (ROOT / ".editorconfig", ROOT / ".clang-format")
    for path in style_files:
        if not path.exists():
            issues.append(f"{path.relative_to(ROOT).as_posix()}: 风格配置不存在")

    compiler_flags = ROOT / "firmware" / "compiler_flags.txt"
    required_flags = (
        "-Wall",
        "-Wextra",
        "-Wshadow",
        "-Wstrict-prototypes",
        "-Wmissing-prototypes",
        "-Wundef",
    )
    if not compiler_flags.exists():
        issues.append("firmware/compiler_flags.txt: 共用编译选项不存在")
    else:
        flags = compiler_flags.read_text(encoding="utf-8", errors="ignore").split()
        for flag in required_flags:
            if flag not in flags:
                issues.append(f"firmware/compiler_flags.txt: 缺少 {flag}")

    for path in (ROOT / "firmware" / "Makefile", ROOT / "firmware" / "build.sh", ROOT / "firmware" / "build.ps1"):
        text = path.read_text(encoding="utf-8", errors="ignore")
        relative_path = path.relative_to(ROOT).as_posix()
        if "compiler_flags.txt" not in text:
            issues.append(f"{relative_path}: 未使用共用编译选项")
        for legacy_flag in ("-Wno-unused-variable", "-Wno-unused-but-set-variable", "-Wno-pointer-sign"):
            if legacy_flag in text:
                issues.append(f"{relative_path}: 仍抑制已清理的警告 {legacy_flag}")

    for path in SOURCE_FILES:
        data = path.read_bytes()
        text = data.decode("utf-8", errors="ignore")
        relative_path = path.relative_to(ROOT).as_posix()
        for line_number, line in enumerate(text.splitlines(), start=1):
            if line.endswith((" ", "\t")):
                issues.append(f"{relative_path}:{line_number}: 行尾空白")
        if data and not data.endswith(b"\n"):
            issues.append(f"{relative_path}: 缺少文件末尾换行")
    return issues


def main():
    issues = (
        check_source_names()
        + check_build_inputs()
        + check_removed_files()
        + check_display_and_input_names()
        + check_style_and_warning_profile()
    )
    if issues:
        print("=== 可读性契约失败 ===")
        for issue in issues:
            print(f"  {issue}")
        return 1

    print("  [PASS] 活动源码不含反编译变量命名")
    print("  [PASS] 构建入口不依赖 globals.c / globals.h / globals.o")
    print("  [PASS] 旧集中式全局文件和对象不存在")
    print("  [PASS] 显示文本地址、输入 GPIO 掩码和页面辅助函数已语义化")
    print("  [PASS] 共用编译警告配置和活动源码格式检查通过")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
