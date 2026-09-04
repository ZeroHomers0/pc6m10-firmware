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
    + [ROOT / "firmware" / "stub.c"]
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

ALLOWED_COMPATIBILITY_SYMBOLS = {
    "func_0x0000aed0",
}


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


def main():
    issues = check_source_names() + check_build_inputs() + check_removed_files()
    if issues:
        print("=== 可读性契约失败 ===")
        for issue in issues:
            print(f"  {issue}")
        return 1

    print("  [PASS] 活动源码不含反编译变量命名")
    print("  [PASS] 构建入口不依赖 globals.c / globals.h / globals.o")
    print("  [PASS] 旧集中式全局文件和对象不存在")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
