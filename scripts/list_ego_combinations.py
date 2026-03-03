#!/usr/bin/env python3
"""List all valid base-item × ego combinations with computed rarity weights.

This mirrors the allocation-combining logic in `src/drop_system.c`:
- Base item allocation schedule comes from `A:` lines in `lib/edit/object.txt`.
- Ego allocation schedule comes from `A:` lines in `lib/edit/special.txt`.
  If an ego has no `A:` schedule, its `W:` rarity_percent is used at `W:` depth.
- Combined schedule weights are computed as:

    combined_rarity = (base_rarity * ego_rarity) // 100

The output is a CSV with one row per valid pairing.

By default, also includes prefix+suffix ego combos (as in drop_system).
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Sequence, Tuple

DROP_ALLOC_MAX = 8  # matches src/drop_system.c
BASE_ALLOC_MAX = 4  # matches collect_kind_allocations()
EGO_ALLOC_MAX = 4   # matches collect_ego_allocations()

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)

DEFAULT_OBJECT_TXT = os.path.join(REPO_ROOT, "lib", "edit", "object.txt")
DEFAULT_SPECIAL_TXT = os.path.join(REPO_ROOT, "lib", "edit", "special.txt")
DEFAULT_OUTPUT_CSV = os.path.join(REPO_ROOT, "scripts", "output", "ego_combinations.csv")

# tval groups from src/defines.h
CATEGORY_TVALS: dict[str, set[int]] = {
    "weapon": {19, 20, 21, 22, 23},
    "jewelry": {40, 45},
    "armor": {30, 31, 32, 33, 34, 35, 36, 37},
}

_INT_RE = re.compile(r"-?\d+")


@dataclass(frozen=True)
class AllocPair:
    depth: int
    rarity: int


def _parse_alloc_pairs_from_a_line(a_payload: str) -> List[AllocPair]:
    """Parse an A: payload into depth/rarity pairs.

    The edit files are *mostly* written as depth/rarity pairs separated by ':' and '/'.
    In practice, `special.txt` contains lines like `A:1:0:13:50` and even
    `A:2/100:10:50/15:33`, so we parse *all integers* in order and pair them.
    """
    values = [int(m.group(0)) for m in _INT_RE.finditer(a_payload)]
    pairs: List[AllocPair] = []
    for i in range(0, len(values) - 1, 2):
        pairs.append(AllocPair(depth=values[i], rarity=values[i + 1]))
    return pairs


def _sort_alloc_pairs(pairs: Sequence[AllocPair]) -> List[AllocPair]:
    return sorted(pairs, key=lambda p: p.depth)


def _schedule_min_depth(depths: Sequence[int], fallback: int) -> int:
    if not depths:
        return fallback
    min_depth = min(depths)
    return min_depth if min_depth > 0 else fallback


def _schedule_max_depth_cap(depths: Sequence[int], rarities: Sequence[int]) -> int:
    """Return -1 if all zero rarities; 0 if no cap; else the inclusive cap depth."""
    last_positive = -1
    for i, rarity in enumerate(rarities):
        if rarity > 0:
            last_positive = i

    if last_positive < 0:
        return -1

    for i in range(last_positive + 1, len(rarities)):
        if rarities[i] == 0:
            return depths[i]

    return 0


def _rarity_from_schedule(
    depths: Sequence[int],
    rarities: Sequence[int],
    depth: int,
    default_rarity: int,
) -> int:
    if not depths:
        return default_rarity

    # Trailing zero-rarity entries are treated as max-depth markers.
    end = len(rarities)
    while end > 1 and rarities[end - 1] == 0:
        end -= 1

    rarity = rarities[0]
    for i in range(1, end):
        if depth >= depths[i]:
            rarity = rarities[i]
        else:
            break
    return rarity


def _combine_allocations(
    base_pairs: Sequence[AllocPair],
    ego_pairs: Sequence[AllocPair],
) -> List[AllocPair]:
    """Combine base + ego allocation schedules (mirror combine_allocations() in C)."""
    base_pairs = _sort_alloc_pairs(base_pairs)
    ego_pairs = _sort_alloc_pairs(ego_pairs)

    base_depths = [p.depth for p in base_pairs]
    base_rarities = [p.rarity for p in base_pairs]
    ego_depths = [p.depth for p in ego_pairs]
    ego_rarities = [p.rarity for p in ego_pairs]

    base_cap = _schedule_max_depth_cap(base_depths, base_rarities)
    ego_cap = _schedule_max_depth_cap(ego_depths, ego_rarities)

    combined_cap = 0
    if base_cap > 0 and ego_cap > 0:
        combined_cap = min(base_cap, ego_cap)
    elif base_cap > 0:
        combined_cap = base_cap
    elif ego_cap > 0:
        combined_cap = ego_cap

    merged_depths: List[int] = []
    for d in base_depths:
        if combined_cap > 0 and d > combined_cap:
            continue
        if d not in merged_depths:
            merged_depths.append(d)
    for d in ego_depths:
        if combined_cap > 0 and d > combined_cap:
            continue
        if d not in merged_depths:
            merged_depths.append(d)

    merged_depths.sort()

    out: List[AllocPair] = []
    last_rarity: Optional[int] = None
    for d in merged_depths:
        if combined_cap > 0 and d > combined_cap:
            continue
        base_r = _rarity_from_schedule(base_depths, base_rarities, d, default_rarity=1)
        ego_r = _rarity_from_schedule(ego_depths, ego_rarities, d, default_rarity=1)
        combined = (base_r * ego_r) // 100
        combined = min(combined, 255)
        if last_rarity is None or combined != last_rarity:
            out.append(AllocPair(depth=d, rarity=combined))
            last_rarity = combined

    # Preserve inclusive max-depth cap marker.
    if combined_cap > 0 and len(out) < DROP_ALLOC_MAX:
        out.append(AllocPair(depth=combined_cap, rarity=0))

    return out[:DROP_ALLOC_MAX]


def _more_special_rarity_bonus(rarity: int) -> int:
    if rarity <= 0:
        return 0
    rarity += 20
    return min(rarity, 255)


def _less_special_rarity_penalty(rarity: int) -> int:
    if rarity <= 0:
        return 0
    rarity -= 20
    return max(rarity, 0)


def _pairs_to_str(pairs: Sequence[AllocPair]) -> str:
    return ":".join(f"{p.depth}/{p.rarity}" for p in pairs)


def _parse_item_categories(raw_values: Optional[Sequence[str]]) -> set[str]:
    if not raw_values:
        return set()

    selected: set[str] = set()
    for raw_value in raw_values:
        for part in raw_value.split(","):
            category = part.strip().lower()
            if not category:
                continue
            if category not in CATEGORY_TVALS:
                valid = ", ".join(sorted(CATEGORY_TVALS.keys()))
                raise ValueError(f"Invalid category '{category}'. Valid values: {valid}")
            selected.add(category)

    return selected


@dataclass
class ObjectKind:
    idx: int
    name: str
    tval: Optional[int] = None
    sval: Optional[int] = None
    level: int = 1  # W: depth
    alloc_pairs: List[AllocPair] = field(default_factory=list)
    flags: set[str] = field(default_factory=set)

    @property
    def is_insta_art(self) -> bool:
        return "INSTA_ART" in self.flags

    @property
    def more_special(self) -> bool:
        return "MORE_SPECIAL" in self.flags

    @property
    def less_special(self) -> bool:
        return "LESS_SPECIAL" in self.flags


@dataclass
class Ego:
    idx: int
    name: str
    level: int = 1  # W: depth
    rarity: int = 1  # W: rarity_percent (only used if no A:)
    max_level: int = 0  # W: max_depth (0 means no cap)
    tval_ranges: List[Tuple[int, int, int]] = field(default_factory=list)  # (tval, min_sval, max_sval)
    alloc_pairs: List[AllocPair] = field(default_factory=list)
    flags: set[str] = field(default_factory=set)

    @property
    def is_prefix(self) -> bool:
        return len(self.name) >= 2 and self.name[0] == "(" and self.name[-1] == ")"

    def applies_to(self, kind: ObjectKind) -> bool:
        if kind.tval is None or kind.sval is None:
            return False
        for tval, min_sval, max_sval in self.tval_ranges:
            if kind.tval != tval:
                continue
            if min_sval <= kind.sval <= max_sval:
                return True
        return False


def parse_object_txt(path: str) -> List[ObjectKind]:
    kinds: List[ObjectKind] = []
    current: Optional[ObjectKind] = None

    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            if line.startswith("N:"):
                if current is not None:
                    kinds.append(current)
                m = re.match(r"^N:(\d+):(.*)$", line)
                if not m:
                    current = None
                    continue
                idx = int(m.group(1))
                name_raw = m.group(2).strip()
                name = name_raw.replace("&", "").replace("~", "").strip()
                current = ObjectKind(idx=idx, name=name)
                continue

            if current is None:
                continue

            if line.startswith("I:"):
                parts = [p.strip() for p in line[2:].split(":")]
                if len(parts) >= 2:
                    try:
                        current.tval = int(parts[0])
                        current.sval = int(parts[1])
                    except ValueError:
                        pass
                continue

            if line.startswith("W:"):
                parts = [p.strip() for p in line[2:].split(":")]
                if parts:
                    try:
                        current.level = int(parts[0]) if int(parts[0]) > 0 else 1
                    except ValueError:
                        pass
                continue

            if line.startswith("A:"):
                pairs = _parse_alloc_pairs_from_a_line(line[2:])
                current.alloc_pairs.extend(pairs)
                continue

            if line.startswith("F:"):
                payload = line[2:]
                flags = [x.strip() for x in payload.split("|")]
                for fl in flags:
                    if fl:
                        current.flags.add(fl)
                continue

    if current is not None:
        kinds.append(current)

    # Cap to the engine limits and sort.
    for k in kinds:
        k.alloc_pairs = _sort_alloc_pairs(k.alloc_pairs)[:BASE_ALLOC_MAX]

    return kinds


def parse_special_txt(path: str) -> List[Ego]:
    egos: List[Ego] = []
    current: Optional[Ego] = None

    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            if line.startswith("N:"):
                if current is not None:
                    egos.append(current)
                m = re.match(r"^N:(\d+):(.*)$", line)
                if not m:
                    current = None
                    continue
                idx = int(m.group(1))
                name = m.group(2).strip()
                current = Ego(idx=idx, name=name)
                continue

            if current is None:
                continue

            if line.startswith("W:"):
                parts = [p.strip() for p in line[2:].split(":")]
                if len(parts) >= 1:
                    try:
                        current.level = int(parts[0]) if int(parts[0]) > 0 else 1
                    except ValueError:
                        pass
                if len(parts) >= 2:
                    try:
                        current.rarity = int(parts[1]) if int(parts[1]) > 0 else 1
                    except ValueError:
                        pass
                if len(parts) >= 3:
                    try:
                        current.max_level = int(parts[2])
                    except ValueError:
                        pass
                continue

            if line.startswith("A:"):
                pairs = _parse_alloc_pairs_from_a_line(line[2:])
                current.alloc_pairs.extend(pairs)
                continue

            if line.startswith("T:"):
                parts = [p.strip() for p in line[2:].split(":")]
                if len(parts) >= 3:
                    try:
                        tval = int(parts[0])
                        min_sval = int(parts[1])
                        max_sval = int(parts[2])
                        current.tval_ranges.append((tval, min_sval, max_sval))
                    except ValueError:
                        pass
                continue

            if line.startswith("F:"):
                payload = line[2:]
                flags = [x.strip() for x in payload.split("|")]
                for flag_name in flags:
                    if flag_name:
                        current.flags.add(flag_name)
                continue

    if current is not None:
        egos.append(current)

    for e in egos:
        e.alloc_pairs = _sort_alloc_pairs(e.alloc_pairs)[:EGO_ALLOC_MAX]

    return egos


def _base_schedule_for_kind(kind: ObjectKind) -> List[AllocPair]:
    if kind.alloc_pairs:
        return kind.alloc_pairs[:BASE_ALLOC_MAX]
    return [AllocPair(depth=max(kind.level, 1), rarity=1)]


def _ego_schedule_for_ego(ego: Ego) -> List[AllocPair]:
    if ego.alloc_pairs:
        return ego.alloc_pairs[:EGO_ALLOC_MAX]
    return [AllocPair(depth=max(ego.level, 1), rarity=max(ego.rarity, 1))]


def _schedule_has_any_spawn(pairs: Sequence[AllocPair]) -> bool:
    depths = [p.depth for p in pairs]
    rarities = [p.rarity for p in pairs]
    if any(r > 0 for r in rarities):
        return True
    return _schedule_max_depth_cap(depths, rarities) >= 0


def _has_alignment_conflict(*flag_sets: Iterable[str]) -> bool:
    has_noble = False
    has_evil = False

    for flags in flag_sets:
        if "NOBLE_ITEM" in flags:
            has_noble = True
        if "EVIL_ITEM" in flags:
            has_evil = True
        if has_noble and has_evil:
            return True

    return False


def main() -> None:
    parser = argparse.ArgumentParser(
        description="List all valid base-item × ego combinations and their combined rarity schedules",
    )
    parser.add_argument(
        "--object-txt",
        default=DEFAULT_OBJECT_TXT,
        help="Path to object.txt (default: <repo>/lib/edit/object.txt)",
    )
    parser.add_argument(
        "--special-txt",
        default=DEFAULT_SPECIAL_TXT,
        help="Path to special.txt (default: <repo>/lib/edit/special.txt)",
    )
    parser.add_argument(
        "--out",
        default=DEFAULT_OUTPUT_CSV,
        help="Output CSV path (default: <repo>/scripts/output/ego_combinations.csv)",
    )
    parser.add_argument(
        "--table",
        dest="table",
        action="store_true",
        help="Print a console table (default: on)",
    )
    parser.add_argument(
        "--no-table",
        dest="table",
        action="store_false",
        help="Disable console table output (CSV only)",
    )
    parser.add_argument(
        "--table-limit",
        type=int,
        default=50,
        help="Max rows to print when using --table (default: 50)",
    )
    parser.add_argument(
        "--table-max-col-width",
        type=int,
        default=50,
        help="Max width for any printed table column; 0 disables truncation (default: 50)",
    )
    parser.add_argument(
        "--table-all",
        action="store_true",
        help="Print all rows when using --table (can be very large)",
    )
    parser.add_argument(
        "--no-combos",
        dest="include_combos",
        action="store_false",
        help="Disable prefix+suffix ego combos (default: include combos)",
    )
    parser.add_argument(
        "--include-unquenched-fire-combos",
        dest="exclude_unquenched_fire_combos",
        action="store_false",
        help="Include combos where suffix ego idx == 148 (default: exclude; matches drop_system)",
    )
    parser.add_argument(
        "--item-category",
        "--item-categories",
        dest="item_categories",
        nargs="+",
        help=(
            "Filter to item categories: weapon, jewelry, armor. "
            "Can be provided multiple times and/or comma-separated, "
            "e.g. --item-category weapon armor or --item-category weapon,jewelry"
        ),
    )
    parser.set_defaults(include_combos=True, exclude_unquenched_fire_combos=True, table=True)
    args = parser.parse_args()

    try:
        selected_categories = _parse_item_categories(args.item_categories)
    except ValueError as e:
        parser.error(str(e))

    kinds = parse_object_txt(args.object_txt)
    egos = parse_special_txt(args.special_txt)

    # Filter to "real" kinds (must have tval/sval).
    kinds = [k for k in kinds if k.tval is not None and k.sval is not None and not k.is_insta_art and "Note" not in k.name]
    egos = [e for e in egos if e.tval_ranges]

    if selected_categories:
        allowed_tvals: set[int] = set()
        for category in selected_categories:
            allowed_tvals.update(CATEGORY_TVALS[category])
        kinds = [k for k in kinds if k.tval in allowed_tvals]

    out_dir = os.path.dirname(args.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    rows: List[dict] = []

    # Base items without egos (will be sorted first per group later).
    for kind in kinds:
        base_pairs_orig = _base_schedule_for_kind(kind)

        base_min = _schedule_min_depth([p.depth for p in base_pairs_orig], fallback=max(kind.level, 1))
        min_depth = max(1, base_min)

        base_depths = [p.depth for p in base_pairs_orig]
        base_rarities = [p.rarity for p in base_pairs_orig]
        rarity_cap = _schedule_max_depth_cap(base_depths, base_rarities)
        max_depth = rarity_cap if rarity_cap > 0 else 0

        rows.append(
            {
                "kind_idx": kind.idx,
                "kind_name": kind.name,
                "tval": kind.tval,
                "sval": kind.sval,
                "kind_more_special": int(kind.more_special),
                "kind_less_special": int(kind.less_special),
                "ego_prefix_idx": "",
                "ego_prefix_name": "",
                "ego_suffix_idx": "",
                "ego_suffix_name": "",
                "combined_alloc": _pairs_to_str(base_pairs_orig),
                "min_depth": min_depth,
                "max_depth": max_depth,
                "base_alloc": _pairs_to_str(_sort_alloc_pairs(base_pairs_orig)),
                "ego_alloc": "",
            }
        )

    # Single ego variants.
    for kind in kinds:
        base_pairs_orig = _base_schedule_for_kind(kind)

        for ego in egos:
            if not ego.applies_to(kind):
                continue
            if _has_alignment_conflict(kind.flags, ego.flags):
                continue

            ego_pairs = _ego_schedule_for_ego(ego)

            # Apply MORE_SPECIAL / LESS_SPECIAL modifiers to the *base* schedule when combining single egos.
            base_pairs = list(base_pairs_orig)
            if kind.more_special:
                base_pairs = [AllocPair(p.depth, _more_special_rarity_bonus(p.rarity)) for p in base_pairs]
            if kind.less_special:
                base_pairs = [AllocPair(p.depth, _less_special_rarity_penalty(p.rarity)) for p in base_pairs]

            combined_pairs = _combine_allocations(base_pairs, ego_pairs)
            if not combined_pairs:
                continue
            if not _schedule_has_any_spawn(combined_pairs):
                continue

            base_min = _schedule_min_depth([p.depth for p in base_pairs_orig], fallback=max(kind.level, 1))
            ego_min = _schedule_min_depth([p.depth for p in ego_pairs], fallback=max(ego.level, 1))
            min_depth = max(1, base_min, ego_min)

            combined_depths = [p.depth for p in combined_pairs]
            combined_rarities = [p.rarity for p in combined_pairs]
            rarity_cap = _schedule_max_depth_cap(combined_depths, combined_rarities)

            max_depth = ego.max_level if ego.max_level and ego.max_level > 0 else 0
            if rarity_cap > 0 and (max_depth == 0 or rarity_cap < max_depth):
                max_depth = rarity_cap

            rows.append(
                {
                    "kind_idx": kind.idx,
                    "kind_name": kind.name,
                    "tval": kind.tval,
                    "sval": kind.sval,
                    "kind_more_special": int(kind.more_special),
                    "kind_less_special": int(kind.less_special),
                    "ego_prefix_idx": ego.idx if ego.is_prefix else "",
                    "ego_prefix_name": ego.name if ego.is_prefix else "",
                    "ego_suffix_idx": ego.idx if not ego.is_prefix else "",
                    "ego_suffix_name": ego.name if not ego.is_prefix else "",
                    "combined_alloc": _pairs_to_str(combined_pairs),
                    "min_depth": min_depth,
                    "max_depth": max_depth,
                    "base_alloc": _pairs_to_str(_sort_alloc_pairs(base_pairs_orig)),
                    "ego_alloc": _pairs_to_str(_sort_alloc_pairs(ego_pairs)),
                }
            )

    # Prefix+suffix combos.
    if args.include_combos:
        prefixes = [e for e in egos if e.is_prefix]
        suffixes = [e for e in egos if not e.is_prefix]

        for kind in kinds:
            base_pairs_orig = _base_schedule_for_kind(kind)

            for prefix_ego in prefixes:
                if not prefix_ego.applies_to(kind):
                    continue
                if _has_alignment_conflict(kind.flags, prefix_ego.flags):
                    continue

                prefix_pairs = _ego_schedule_for_ego(prefix_ego)
                tmp_pairs = _combine_allocations(base_pairs_orig, prefix_pairs)
                if not tmp_pairs:
                    continue

                for suffix_ego in suffixes:
                    if args.exclude_unquenched_fire_combos and suffix_ego.idx == 148:
                        continue
                    if not suffix_ego.applies_to(kind):
                        continue
                    if _has_alignment_conflict(kind.flags, prefix_ego.flags, suffix_ego.flags):
                        continue

                    suffix_pairs = _ego_schedule_for_ego(suffix_ego)
                    combined_pairs = _combine_allocations(tmp_pairs, suffix_pairs)
                    if not combined_pairs:
                        continue
                    if not _schedule_has_any_spawn(combined_pairs):
                        continue

                    base_min = _schedule_min_depth([p.depth for p in base_pairs_orig], fallback=max(kind.level, 1))
                    p_min = _schedule_min_depth([p.depth for p in prefix_pairs], fallback=max(prefix_ego.level, 1))
                    s_min = _schedule_min_depth([p.depth for p in suffix_pairs], fallback=max(suffix_ego.level, 1))
                    min_depth = max(1, base_min, p_min, s_min)

                    combined_depths = [p.depth for p in combined_pairs]
                    combined_rarities = [p.rarity for p in combined_pairs]
                    rarity_cap = _schedule_max_depth_cap(combined_depths, combined_rarities)

                    max_depth = 0
                    if prefix_ego.max_level and prefix_ego.max_level > 0:
                        max_depth = prefix_ego.max_level
                    if suffix_ego.max_level and suffix_ego.max_level > 0:
                        max_depth = suffix_ego.max_level if max_depth == 0 else min(max_depth, suffix_ego.max_level)
                    if rarity_cap > 0 and (max_depth == 0 or rarity_cap < max_depth):
                        max_depth = rarity_cap

                    rows.append(
                        {
                            "kind_idx": kind.idx,
                            "kind_name": kind.name,
                            "tval": kind.tval,
                            "sval": kind.sval,
                            "kind_more_special": int(kind.more_special),
                            "kind_less_special": int(kind.less_special),
                            "ego_prefix_idx": prefix_ego.idx,
                            "ego_prefix_name": prefix_ego.name,
                            "ego_suffix_idx": suffix_ego.idx,
                            "ego_suffix_name": suffix_ego.name,
                            "combined_alloc": _pairs_to_str(combined_pairs),
                            "min_depth": min_depth,
                            "max_depth": max_depth,
                            "base_alloc": _pairs_to_str(_sort_alloc_pairs(base_pairs_orig)),
                            "ego_alloc": _pairs_to_str(_sort_alloc_pairs(prefix_pairs))
                            + " + "
                            + _pairs_to_str(_sort_alloc_pairs(suffix_pairs)),
                        }
                    )

    # Sort rows: group by kind_idx, with base item (no egos) first in each group.
    rows.sort(key=lambda r: (
        r["kind_idx"],
        (r["ego_prefix_idx"] == "" and r["ego_suffix_idx"] == "") == False,  # False (base) sorts before True (ego variants)
        int(r["ego_prefix_idx"]) if r["ego_prefix_idx"] else 0,
        int(r["ego_suffix_idx"]) if r["ego_suffix_idx"] else 0,
    ))

    fieldnames = [
        "kind_idx",
        "kind_name",
        "tval",
        "sval",
        "kind_more_special",
        "kind_less_special",
        "ego_prefix_idx",
        "ego_prefix_name",
        "ego_suffix_idx",
        "ego_suffix_name",
        "combined_alloc",
        "min_depth",
        "max_depth",
        "base_alloc",
        "ego_alloc",
    ]

    wrote_csv = False
    try:
        with open(args.out, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for row in rows:
                writer.writerow(row)
        wrote_csv = True
    except PermissionError as e:
        print(
            f"Could not write CSV to '{args.out}' (PermissionError: {e}). "
            "If the file is open (e.g. in Excel), close it or pass --out <other path>.",
            file=sys.stderr,
        )

    if wrote_csv:
        print(f"Wrote {len(rows)} rows -> {args.out}")
    else:
        print(f"Computed {len(rows)} rows (CSV not written)")

    if args.table:
        table_rows: List[dict] = []
        for r in rows:
            prefix_name = str(r.get("ego_prefix_name", "") or "")
            suffix_name = str(r.get("ego_suffix_name", "") or "")
            if prefix_name and suffix_name:
                ego_name = f"{prefix_name} + {suffix_name}"
            else:
                ego_name = prefix_name or suffix_name

            min_depth = int(r.get("min_depth", 0) or 0)
            max_depth = int(r.get("max_depth", 0) or 0)
            depths = f"{min_depth}+" if max_depth == 0 else f"{min_depth}-{max_depth}"

            table_rows.append(
                {
                    "kind_name": r.get("kind_name", ""),
                    "ego": ego_name,
                    "combined_alloc": r.get("combined_alloc", ""),
                    "depths": depths,
                }
            )

        _print_table(
            table_rows,
            columns=["kind_name", "ego", "combined_alloc", "depths"],
            limit=None if args.table_all else max(0, args.table_limit),
            max_col_width=max(0, int(args.table_max_col_width)),
        )


def _print_table(
    rows: Sequence[dict],
    columns: Sequence[str],
    limit: Optional[int],
    max_col_width: int,
) -> None:
    if not rows:
        print("(no rows)")
        return

    if limit is None:
        view = list(rows)
    else:
        view = list(rows[:limit])

    def cell_str(value: object) -> str:
        if value is None:
            s = ""
        else:
            s = str(value)
        if max_col_width > 0 and len(s) > max_col_width:
            if max_col_width <= 3:
                return s[:max_col_width]
            return s[: max_col_width - 3] + "..."
        return s

    widths: dict[str, int] = {}
    for col in columns:
        widths[col] = len(col)

    for r in view:
        for col in columns:
            widths[col] = max(widths[col], len(cell_str(r.get(col, ""))))

    header = " | ".join(col.ljust(widths[col]) for col in columns)
    sep = "-+-".join("-" * widths[col] for col in columns)
    print("\n" + header)
    print(sep)

    for r in view:
        line = " | ".join(cell_str(r.get(col, "")).ljust(widths[col]) for col in columns)
        print(line)

    if limit is not None and len(rows) > len(view):
        print(f"... ({len(rows) - len(view)} more rows; use --table-all to print all)")


if __name__ == "__main__":
    main()
