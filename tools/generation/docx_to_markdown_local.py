"""Convert the locally generated control-system DOCX report to Markdown.

Requires python-docx. Both input and output live under ``tmp/``.
"""

from pathlib import Path
from docx import Document
from docx.table import Table
from docx.text.paragraph import Paragraph


ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "tmp" / "PC6M-10_PC12M-2控制与保护系统程序分析.docx"
OUT = ROOT / "tmp" / "PC6M-10_PC12M-2控制与保护系统程序分析.md"


def blocks(parent):
    for child in parent.element.body.iterchildren():
        if child.tag.endswith("}p"):
            yield Paragraph(child, parent)
        elif child.tag.endswith("}tbl"):
            yield Table(child, parent)


def clean(text):
    return " ".join(text.replace("\r", "").splitlines()).strip()


def esc(text):
    return clean(text).replace("|", "\\|")


doc = Document(SRC)
out = []
title_seen = False
for block in blocks(doc):
    if isinstance(block, Paragraph):
        text = block.text.strip()
        if not text:
            continue
        style = block.style.name if block.style else ""
        if style == "Title":
            out += [f"# {text}", ""]
            title_seen = True
        elif style.startswith("Heading"):
            try:
                level = int(style.split()[-1]) + 1
            except ValueError:
                level = 2
            out += [f"{'#' * min(level, 6)} {text}", ""]
        elif style.startswith("List Bullet"):
            out += [f"- {clean(text)}", ""]
        elif "↓" in text and "\n" in block.text:
            out += ["```text", block.text.strip(), "```", ""]
        elif not title_seen:
            out += [f"# {text}", ""]
            title_seen = True
        else:
            out += [block.text.strip(), ""]
    else:
        rows = [[esc(c.text) for c in row.cells] for row in block.rows]
        if not rows:
            continue
        out.append("| " + " | ".join(rows[0]) + " |")
        out.append("| " + " | ".join(["---"] * len(rows[0])) + " |")
        for row in rows[1:]:
            out.append("| " + " | ".join(row) + " |")
        out.append("")

header = [
    "<!--",
    "文档依据：PC6M-10与PC12M-2原始固件反汇编、可编译复刻源码及执行级等价验证。",
    "生成日期：2026-09-10",
    "-->",
    "",
]
OUT.write_text("\n".join(header + out).rstrip() + "\n", encoding="utf-8")
print(OUT)
