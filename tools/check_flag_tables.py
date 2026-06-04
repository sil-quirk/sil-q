#!/usr/bin/env python3
"""
Validate object flag table entries and common flag-word usages.

This catches mistakes where a flag from one TR set is tested against the wrong
flag word, such as checking `f3 & TR2_TRAITOR`.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
DEFINES_PATH = SRC_DIR / "defines.h"
INIT_FLAGS_PATH = SRC_DIR / "init" / "init-flags.c"

DEFINE_RE = re.compile(r"^\s*#define\s+(TR(?P<set>[1-4])_[A-Z0-9_]+)\b", re.M)
TABLE_RE = re.compile(
    r'\{\s*"(?P<name>[^"]+)"\s*,\s*TR(?P<table_set>[1-4])\s*,\s*'
    r'(?P<constant>TR(?P<const_set>[1-4])_[A-Z0-9_]+)\s*\}',
    re.M,
)
BIT_TEST_RE = re.compile(
    r"(?P<lhs>[A-Za-z_][A-Za-z0-9_>\.-]*)\s*&\s*"
    r"(?P<tr>TR(?P<tr_set>[1-4])_[A-Z0-9_]+)"
)


def line_col(text: str, index: int) -> tuple[int, int]:
    line = text.count("\n", 0, index) + 1
    last_newline = text.rfind("\n", 0, index)
    col = index + 1 if last_newline < 0 else index - last_newline
    return line, col


def infer_flag_set(lhs: str) -> int | None:
    field_match = re.search(r"(?:->|\.)flags([1-4])\b", lhs)
    if field_match:
        return int(field_match.group(1))

    ident_match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", lhs)
    if not ident_match:
        return None

    ident = ident_match.group(1)
    suffix_match = re.search(r"(?:^|_)(?:f|flags)([1-4])$", ident)
    if suffix_match:
        return int(suffix_match.group(1))

    return None


def load_tr_defines() -> set[str]:
    text = DEFINES_PATH.read_text(encoding="utf-8")
    return {match.group(0).split()[1] for match in DEFINE_RE.finditer(text)}


def check_init_flags_table(tr_defines: set[str]) -> list[str]:
    text = INIT_FLAGS_PATH.read_text(encoding="utf-8")
    errors: list[str] = []

    for match in TABLE_RE.finditer(text):
        table_set = int(match.group("table_set"))
        const_set = int(match.group("const_set"))
        constant = match.group("constant")

        if table_set != const_set:
            line, col = line_col(text, match.start())
            errors.append(
                f"{INIT_FLAGS_PATH.relative_to(REPO_ROOT)}:{line}:{col}: "
                f"table set TR{table_set} mismatches {constant}"
            )

        if constant not in tr_defines:
            line, col = line_col(text, match.start("constant"))
            errors.append(
                f"{INIT_FLAGS_PATH.relative_to(REPO_ROOT)}:{line}:{col}: "
                f"{constant} is not defined in {DEFINES_PATH.relative_to(REPO_ROOT)}"
            )

    return errors


def check_source_usage() -> list[str]:
    errors: list[str] = []

    for path in sorted(SRC_DIR.rglob("*")):
        if path.suffix not in {".c", ".h"}:
            continue

        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(REPO_ROOT)

        for match in BIT_TEST_RE.finditer(text):
            lhs = match.group("lhs")
            inferred_set = infer_flag_set(lhs)
            if inferred_set is None:
                continue

            tr_set = int(match.group("tr_set"))
            if inferred_set != tr_set:
                line, col = line_col(text, match.start("lhs"))
                errors.append(
                    f"{rel}:{line}:{col}: {lhs} uses {match.group('tr')} "
                    f"(expected TR{inferred_set}_*)"
                )

    return errors


def main() -> int:
    tr_defines = load_tr_defines()
    errors = check_init_flags_table(tr_defines)
    errors.extend(check_source_usage())

    if errors:
        print("Flag table audit failed:")
        for error in errors:
            print(error)
        return 1

    print("Flag table audit passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
