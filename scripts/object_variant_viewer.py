#!/usr/bin/env python3
"""Show object variants for a specific base item and optional ego filters.

Usage:
  python object_variant_viewer.py [object|all] [prefix|all] [suffix|all]

Examples:
  python object_variant_viewer.py 64
  python object_variant_viewer.py 64 all all
  python object_variant_viewer.py 64 no all
  python object_variant_viewer.py 64 12 all
  python object_variant_viewer.py all all all

The selectors are the `N:` indices from `lib/edit/object.txt` and
`lib/edit/special.txt`. Use `all` to include every applicable object or ego
for that selector. Use `no` in the prefix/suffix position to require that the
item has no ego in that slot.

When a prefix or suffix selector is a concrete `N:` value, it acts as a
filter: rows that do not contain that exact ego are omitted.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
from typing import Any


_USE_COLOR = sys.stdout.isatty()
if sys.platform == "win32" and _USE_COLOR:
    try:
        import ctypes

        ctypes.windll.kernel32.SetConsoleMode(
            ctypes.windll.kernel32.GetStdHandle(-11), 7
        )
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        _USE_COLOR = False
        pass


def _c(code: str, text: str) -> str:
    if not _USE_COLOR:
        return text
    return f"\033[{code}m{text}\033[0m"


def _yellow(text: str) -> str:
    return _c("93", text)


def _green(text: str) -> str:
    return _c("92", text)


def _red(text: str) -> str:
    return _c("91", text)


ALIGNMENT_COLORS = {
    "noble": _yellow,
    "normal": _green,
    "evil": _red,
}

NO_EGO = object()


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)

DEFAULT_OBJECT_TXT = os.path.join(REPO_ROOT, "lib", "edit", "object.txt")
DEFAULT_SPECIAL_TXT = os.path.join(REPO_ROOT, "lib", "edit", "special.txt")
DEFAULT_ABILITY_TXT = os.path.join(REPO_ROOT, "lib", "edit", "ability.txt")


# Allow importing the sibling analysis script.
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

import calc_artefact_difficulty as cad
from calc_artefact_difficulty import (
    calculate_difficulty,
    clean_obj_name,
    format_rarity_schedule,
    generate_dual_ego_variants,
    generate_normal_variants,
    generate_special_variants,
    parse_ability_file,
    parse_object_file,
    parse_special_file,
    populate_objects_dict,
)


def _parse_selector(raw: str, allow_no: bool = False) -> int | object | None:
    value = raw.strip()
    if value.lower() == "all":
        return None
    if allow_no and value.lower() == "no":
        return NO_EGO
    try:
        return int(value)
    except ValueError as exc:
        allowed = "integer, 'all'"
        if allow_no:
            allowed += ", or 'no'"
        raise SystemExit(f"Selector must be {allowed}, got {raw!r}") from exc


def _alignment_from_flags(flags: object) -> str:
    if isinstance(flags, str):
        parsed_flags = {part for part in flags.split("|") if part}
    else:
        parsed_flags = {str(flag) for flag in flags if flag}

    if "NOBLE_ITEM" in parsed_flags:
        return "noble"
    if "EVIL_ITEM" in parsed_flags:
        return "evil"
    return "normal"


def _select_objects(objects: list[dict[str, Any]], selector: int | None) -> list[dict[str, Any]]:
    if selector is None:
        return list(objects)

    selected = [obj for obj in objects if obj.get("k_idx") == selector]
    if not selected:
        raise SystemExit(f"No object with N:{selector} was found in object.txt")
    return selected


def _select_specials(
    specials: list[dict[str, Any]],
    selector: int | object | None,
    is_prefix: bool,
) -> list[dict[str, Any]]:
    if selector is NO_EGO:
        return []

    candidates = [spec for spec in specials if bool(spec.get("is_prefix")) == is_prefix]
    if selector is None:
        return candidates

    selected = [spec for spec in candidates if spec.get("idx") == selector]
    if not selected:
        side = "prefix" if is_prefix else "suffix"
        raise SystemExit(f"No {side} ego with N:{selector} was found in special.txt")
    return selected


def _variant_kind(row: dict[str, Any]) -> str:
    if row.get("type") == "normal":
        return "BASE"
    if row.get("type") == "special":
        return "PFX" if row.get("is_prefix") else "SFX"
    if row.get("type") == "dual_ego":
        return "DUO"
    return str(row.get("type", "?")).upper()


def _object_label(row: dict[str, Any]) -> str:
    base_obj = cad.OBJECTS_BY_TYPE.get((row.get("tval"), row.get("sval")), {})
    object_idx = row.get("k_idx", base_obj.get("k_idx", ""))

    if row.get("type") == "normal":
        object_name = row.get("name", "")
    else:
        object_name = row.get("base_name") or row.get("name") or clean_obj_name(
            base_obj.get("name", "")
        )

    if object_idx == "":
        return object_name
    return f"{object_idx} {object_name}".strip()


def _ego_label(idx: Any, name: Any) -> str:
    if idx == "" or idx is None:
        return "-"
    return f"{idx} {name}".strip()


def _format_combat_pair(bonus: int, dice: int, sides: int) -> str:
    return f"{bonus:+d},{dice}d{sides}"


def _format_stats(row: dict[str, Any]) -> str:
    ability_count = len(row.get("ability_list", []))
    att = int(row.get("att", 0))
    evn = int(row.get("evn", 0))
    dd = int(row.get("dd", 0))
    ds = int(row.get("ds", 0))
    pd = int(row.get("pd", 0))
    ps = int(row.get("ps", 0))
    pval = int(row.get("pval", 0))

    parts = [
        f"({_format_combat_pair(att, dd, ds)})",
        f"[{_format_combat_pair(evn, pd, ps)}]",
    ]

    extras: list[str] = []
    if pval != 0:
        extras.append(f"pval={pval:+d}")
    if ability_count:
        extras.append(f"abil={ability_count}")
    if extras:
        parts.append(" ".join(extras))

    return " ".join(parts)


def _report_row(row: dict[str, Any]) -> dict[str, Any]:
    kind = _variant_kind(row)
    object_label = _object_label(row)
    schedule = format_rarity_schedule(row.get("rarity_schedule", []))

    if row.get("type") == "normal":
        prefix_idx = ""
        prefix_name = ""
        suffix_idx = ""
        suffix_name = ""
    elif row.get("type") == "special":
        if row.get("is_prefix"):
            prefix_idx = row.get("special_idx", "")
            prefix_name = row.get("special_name", "")
            suffix_idx = ""
            suffix_name = ""
        else:
            prefix_idx = ""
            prefix_name = ""
            suffix_idx = row.get("special_idx", "")
            suffix_name = row.get("special_name", "")
    else:
        prefix_idx = row.get("prefix_idx", "")
        prefix_name = row.get("prefix_name", "")
        suffix_idx = row.get("suffix_idx", "")
        suffix_name = row.get("suffix_name", "")

    base_obj = cad.OBJECTS_BY_TYPE.get((row.get("tval"), row.get("sval")), {})
    object_idx = row.get("k_idx", base_obj.get("k_idx", ""))
    object_name = (
        row.get("base_name")
        or row.get("name")
        or clean_obj_name(base_obj.get("name", ""))
    )

    return {
        "object": object_label,
        "object_idx": object_idx,
        "object_name": object_name,
        "kind": kind,
        "name": row.get("name", ""),
        "prefix_idx": prefix_idx,
        "prefix_name": prefix_name,
        "suffix_idx": suffix_idx,
        "suffix_name": suffix_name,
        "prefix": _ego_label(prefix_idx, prefix_name),
        "suffix": _ego_label(suffix_idx, suffix_name),
        "tval": row.get("tval", ""),
        "sval": row.get("sval", ""),
        "depth": row.get("depth", 0),
        "rarity": row.get("rarity", 0),
        "schedule": schedule,
        "difficulty": row.get("difficulty", 0),
        "stats": _format_stats(row),
        "att": row.get("att", 0),
        "evn": row.get("evn", 0),
        "dd": row.get("dd", 0),
        "ds": row.get("ds", 0),
        "pd": row.get("pd", 0),
        "ps": row.get("ps", 0),
        "pval": row.get("pval", 0),
        "ability_count": len(row.get("ability_list", [])),
        "flags": "|".join(row.get("flags", [])),
        "alignment": _alignment_from_flags(row.get("flags", [])),
    }


def _matches_query(
    row: dict[str, Any],
    object_selector: int | object | None,
    prefix_selector: int | object | None,
    suffix_selector: int | object | None,
) -> bool:
    if object_selector is not None:
        if int(row.get("object_idx", -1) or -1) != object_selector:
            return False
    if prefix_selector is NO_EGO:
        if row.get("prefix_idx", "") != "":
            return False
    elif prefix_selector is not None:
        if row.get("prefix_idx", "") != prefix_selector:
            return False
    if suffix_selector is NO_EGO:
        if row.get("suffix_idx", "") != "":
            return False
    elif suffix_selector is not None:
        if row.get("suffix_idx", "") != suffix_selector:
            return False
    return True


def _print_table(rows: list[dict[str, Any]]) -> None:
    columns = [
        "object",
        "kind",
        "name",
        "schedule",
        "difficulty",
        "stats",
        "prefix",
        "suffix",
    ]
    numeric_right_align = {"difficulty"}
    max_widths = {
        "object": 28,
        "kind": 5,
        "name": 40,
        "schedule": 24,
        "difficulty": 9,
        "stats": 44,
        "prefix": 28,
        "suffix": 28,
    }

    def cell(value: Any, column: str, width: int) -> str:
        text = "" if value is None else str(value)
        if width > 0 and len(text) > width:
            if width <= 3:
                text = text[:width]
            else:
                text = text[: width - 3] + "..."
        if column in numeric_right_align:
            return text.rjust(width)
        return text.ljust(width)

    widths: dict[str, int] = {}
    for column in columns:
        widest = len(column)
        for row in rows:
            widest = max(widest, len("" if row.get(column) is None else str(row.get(column))))
        widths[column] = min(widest, max_widths[column])

    header = "  ".join(column.ljust(widths[column]) for column in columns)
    print(header)
    print("-" * len(header))
    for row in rows:
        color_fn = ALIGNMENT_COLORS.get(str(row.get("alignment", "") or ""), lambda text: text)
        print(
            "  ".join(
                color_fn(cell(row.get(column), column, widths[column]))
                if column in {"object", "kind", "name", "prefix", "suffix"}
                else cell(row.get(column), column, widths[column])
                for column in columns
            )
        )


def _write_csv(rows: list[dict[str, Any]], path: str) -> None:
    out_dir = os.path.dirname(path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    fieldnames = [
        "object_idx",
        "object_name",
        "kind",
        "name",
        "prefix_idx",
        "prefix_name",
        "suffix_idx",
        "suffix_name",
        "tval",
        "sval",
        "depth",
        "rarity",
        "schedule",
        "difficulty",
        "att",
        "evn",
        "dd",
        "ds",
        "pd",
        "ps",
        "pval",
        "ability_count",
        "flags",
        "stats",
    ]

    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fieldnames})


def _load_catalogs(object_txt: str, special_txt: str, ability_txt: str) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    if not os.path.exists(object_txt):
        raise FileNotFoundError(f"object.txt not found: {object_txt}")
    if not os.path.exists(special_txt):
        raise FileNotFoundError(f"special.txt not found: {special_txt}")

    if os.path.exists(ability_txt):
        parse_ability_file(ability_txt)
        print(f"Loaded {len(cad.ABILITY_LEVELS)} ability levels from ability.txt")
    else:
        print("ability.txt not found; using default ability levels")

    objects = parse_object_file(object_txt)
    specials = parse_special_file(special_txt)
    populate_objects_dict(objects)
    return objects, specials


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Show all valid object variants with rarity schedules and smithing difficulty."
        )
    )
    parser.add_argument(
        "object",
        nargs="?",
        default="all",
        help="Object N: index to inspect, or 'all' for every object",
    )
    parser.add_argument(
        "prefix",
        nargs="?",
        default="all",
        help="Prefix ego N: index to inspect, or 'all' for every applicable prefix",
    )
    parser.add_argument(
        "suffix",
        nargs="?",
        default="all",
        help="Suffix ego N: index to inspect, or 'all' for every applicable suffix",
    )
    parser.add_argument(
        "--object-txt",
        default=DEFAULT_OBJECT_TXT,
        help="Path to object.txt",
    )
    parser.add_argument(
        "--special-txt",
        default=DEFAULT_SPECIAL_TXT,
        help="Path to special.txt",
    )
    parser.add_argument(
        "--ability-txt",
        default=DEFAULT_ABILITY_TXT,
        help="Path to ability.txt",
    )
    parser.add_argument(
        "--csv",
        metavar="FILE",
        help="Write the report to a CSV file",
    )
    args = parser.parse_args(argv)

    object_selector = _parse_selector(args.object)
    prefix_selector = _parse_selector(args.prefix, allow_no=True)
    suffix_selector = _parse_selector(args.suffix, allow_no=True)

    objects, specials = _load_catalogs(args.object_txt, args.special_txt, args.ability_txt)

    selected_objects = _select_objects(objects, object_selector)
    selected_prefixes = _select_specials(specials, prefix_selector, True)
    selected_suffixes = _select_specials(specials, suffix_selector, False)

    print(f"Loaded {len(objects)} objects and {len(specials)} egos")
    print(
        f"Selected {len(selected_objects)} objects, {len(selected_prefixes)} prefixes, "
        f"{len(selected_suffixes)} suffixes"
    )
    print(
        "Query: "
        f"object={args.object} "
        f"prefix={args.prefix} "
        f"suffix={args.suffix}"
    )

    raw_rows: list[dict[str, Any]] = []

    base_rows = generate_normal_variants(selected_objects)
    for row in base_rows:
        row["difficulty"] = calculate_difficulty(row)
        raw_rows.append(row)

    for special in selected_prefixes:
        special_rows = generate_special_variants(special, selected_objects)
        for row in special_rows:
            row["difficulty"] = calculate_difficulty(row)
            raw_rows.append(row)

    for special in selected_suffixes:
        special_rows = generate_special_variants(special, selected_objects)
        for row in special_rows:
            row["difficulty"] = calculate_difficulty(row)
            raw_rows.append(row)

    if selected_prefixes and selected_suffixes:
        dual_rows = generate_dual_ego_variants(
            selected_prefixes + selected_suffixes, selected_objects
        )
        for row in dual_rows:
            row["difficulty"] = calculate_difficulty(row)
            raw_rows.append(row)

    report_rows = [
        report_row
        for report_row in (_report_row(row) for row in raw_rows)
        if _matches_query(report_row, object_selector, prefix_selector, suffix_selector)
    ]
    report_rows.sort(
        key=lambda row: (
            int(row["object_idx"]) if str(row["object_idx"]).isdigit() else 0,
            {"BASE": 0, "PFX": 1, "SFX": 2, "DUO": 3}.get(row["kind"], 9),
            -int(row["difficulty"]),
            row["name"],
        )
    )

    base_count = sum(1 for row in report_rows if row["kind"] == "BASE")
    prefix_count = sum(1 for row in report_rows if row["kind"] == "PFX")
    suffix_count = sum(1 for row in report_rows if row["kind"] == "SFX")
    dual_count = sum(1 for row in report_rows if row["kind"] == "DUO")

    print(
        f"Variants: base={base_count}, prefix={prefix_count}, "
        f"suffix={suffix_count}, dual={dual_count}, total={len(report_rows)}"
    )

    if args.csv:
        _write_csv(report_rows, args.csv)
        print(f"Wrote CSV -> {args.csv}")

    if report_rows:
        print()
        _print_table(report_rows)
        if _USE_COLOR:
            print(
                "Colors:  "
                + _yellow("noble")
                + " | "
                + _green("normal")
                + " | "
                + _red("evil")
            )
    else:
        print("No variants matched the selected filters.")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BrokenPipeError:
        raise SystemExit(0)
