#!/usr/bin/env python3
"""Assigns stable GUIDs to Sil-QH data templates.

Usage:
    python tools/make_guid.py              # process default template files
    python tools/make_guid.py lib/edit/race.txt lib/edit/character.txt
    python tools/make_guid.py --dry-run    # report changes without writing
"""

import argparse
import secrets
import string
import sys
from pathlib import Path
from typing import List, Sequence, Set


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RELATIVE_TARGETS = (
    Path("lib/edit/monster.txt"),
    Path("lib/edit/artefact.txt"),
    Path("lib/edit/race.txt"),
    Path("lib/edit/character.txt"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Assign GUIDs to data entries missing Q: lines."
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="Specific files to process (defaults to monster/artefact/race/character).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show how many GUIDs would be added without modifying files.",
    )
    return parser.parse_args()


def is_entry_line(line: str) -> bool:
    stripped = line.lstrip()
    if not stripped or stripped.startswith("#"):
        return False
    return stripped.startswith("N:")


def is_guid_line(line: str) -> bool:
    stripped = line.lstrip()
    if not stripped or stripped.startswith("#"):
        return False
    return stripped.startswith("Q:")


def extract_guid_value(line: str) -> str:
    if ":" not in line:
        return ""
    _, remainder = line.split(":", 1)
    cleaned = "".join(ch for ch in remainder if ch in string.hexdigits)
    return cleaned.lower()


def collect_existing_guids(lines: Sequence[str]) -> Set[str]:
    values: Set[str] = set()
    for line in lines:
        if is_guid_line(line):
            value = extract_guid_value(line)
            if value:
                values.add(value)
    return values


def generate_guid(existing: Set[str]) -> str:
    while True:
        candidate = secrets.token_hex(8)
        if candidate not in existing:
            return candidate


def process_file(path: Path, dry_run: bool, global_guids: Set[str]) -> int:
    text = path.read_text(encoding="utf-8")
    # Preserve trailing newline information via splitlines(keepends=False).
    lines = text.splitlines()
    file_guids = collect_existing_guids(lines)
    global_guids.update(file_guids)

    new_lines: List[str] = []
    inserted = 0
    total_lines = len(lines)
    i = 0

    while i < total_lines:
        line = lines[i]
        new_lines.append(line)

        if is_entry_line(line):
            has_guid = False
            j = i + 1
            while j < total_lines:
                next_line = lines[j]
                if is_entry_line(next_line):
                    break
                if is_guid_line(next_line):
                    guid_value = extract_guid_value(next_line)
                    if guid_value:
                        global_guids.add(guid_value)
                        has_guid = True
                    break
                j += 1

            if not has_guid:
                guid = generate_guid(global_guids)
                global_guids.add(guid)
                new_lines.append(f"Q:{guid}")
                inserted += 1

        i += 1

    if inserted and not dry_run:
        path.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

    return inserted


def resolve_targets(args: argparse.Namespace) -> List[Path]:
    if args.files:
        return [Path(target).expanduser().resolve() for target in args.files]
    return [REPO_ROOT / rel for rel in DEFAULT_RELATIVE_TARGETS]


def format_path(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def main() -> None:
    args = parse_args()
    targets = resolve_targets(args)
    global_guids: Set[str] = set()

    had_error = False
    total_inserted = 0

    for target in targets:
        if not target.exists():
            print(f"[error] Missing file: {target}", file=sys.stderr)
            had_error = True
            continue

        inserted = process_file(target, args.dry_run, global_guids)
        total_inserted += inserted
        verb = "would add" if args.dry_run else "added"
        suffix = "" if inserted == 1 else "s"
        print(f"{format_path(target)}: {verb} {inserted} GUID{suffix}")

    if not args.dry_run:
        print(f"Total new GUIDs: {total_inserted}")

    if had_error:
        sys.exit(1)


if __name__ == "__main__":
    main()
