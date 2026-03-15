#!/usr/bin/env python3
"""List all valid base-item × ego combinations with rarity + stat ranges.

This mirrors the allocation-combining logic in `src/drop_system.c`:
- Base item allocation schedule comes from `A:` lines in `lib/edit/object.txt`.
- Ego allocation schedule comes from `A:` lines in `lib/edit/special.txt`.
  If an ego has no `A:` schedule, its `W:` rarity_percent is used at `W:` depth.
- Combined schedule weights are computed as:

    combined_rarity = (base_rarity * ego_rarity) // 100

The output is a CSV with one row per valid pairing, including combined
allocation schedule and combined stat ranges.

By default, also includes prefix+suffix ego combos (as in drop_system).
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Sequence, Tuple

# Enable UTF-8/ANSI output on Windows consoles that support it.
_USE_COLOR = True
if sys.platform == "win32":
    try:
        import ctypes

        ctypes.windll.kernel32.SetConsoleMode(
            ctypes.windll.kernel32.GetStdHandle(-11), 7
        )
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
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

DROP_ALLOC_MAX = 8  # matches src/drop_system.c
BASE_ALLOC_MAX = 4  # matches collect_kind_allocations()
EGO_ALLOC_MAX = 4   # matches collect_ego_allocations()

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)

DEFAULT_OBJECT_TXT = os.path.join(REPO_ROOT, "lib", "edit", "object.txt")
DEFAULT_SPECIAL_TXT = os.path.join(REPO_ROOT, "lib", "edit", "special.txt")
DEFAULT_ABILITY_TXT = os.path.join(REPO_ROOT, "lib", "edit", "ability.txt")
DEFAULT_OUTPUT_CSV = os.path.join(REPO_ROOT, "scripts", "output", "ego_combinations.csv")

# tval groups from src/defines.h
CATEGORY_TVALS: dict[str, set[int]] = {
    "weapon": {19, 20, 21, 22, 23},
    "jewelry": {40, 45},
    "armor": {30, 31, 32, 33, 34, 35, 36, 37},
}

DAMAGED_KIND_BASIS_NAMES: dict[str, str] = {
    "Rusty Helm": "Helm",
    "Pair of Shabby Boots": "Pair of Boots",
    "Broken Shield": "Round Shield",
    "Chipped Dagger": "Dagger",
    "Bent Shortsword": "Shortsword",
    "Splintered Spear": "Spear",
    "Warped Shortbow": "Shortbow",
    "Torn Cloak": "Cloak",
    "Set of Cracked Gauntlets": "Set of Gauntlets",
    "Pair of Dented Greaves": "Pair of Steel Greaves",
    "Dented Mail Corslet": "Mail Corslet",
}

BONUS_TOKEN_ALIASES = {
    "ARC": "ARCHERY",
    "MEL": "MELEE",
    "PER": "PERCEPTION",
    "SNG": "SONG",
    "SMT": "SMITHING",
    "STL": "STEALTH",
    "WIL": "WILL",
}

FLAG_ALIASES = {
    "NOBLE": "NOBLE_ITEM",
}

STAT_BONUS_TOKENS = ("STR", "DEX", "CON", "GRA")
SKILL_BONUS_TOKENS = (
    "MELEE",
    "ARCHERY",
    "STEALTH",
    "PERCEPTION",
    "WILL",
    "SMITHING",
    "SONG",
)
MISC_BONUS_TOKENS = ("DAMAGE_SIDES", "TUNNEL")

BONUS_DISPLAY_NAMES = {
    "ARCHERY": "Archery",
    "CON": "Con",
    "DEX": "Dex",
    "GRA": "Gra",
    "MELEE": "Melee",
    "PERCEPTION": "Perception",
    "SMITHING": "Smithing",
    "SONG": "Song",
    "STEALTH": "Stealth",
    "STR": "Str",
    "TUNNEL": "Tunnel",
    "WILL": "Will",
}

HIDDEN_BINARY_FLAGS = {
    "EVIL_ITEM",
    "INSTA_ART",
    "LESS_SPECIAL",
    "MORE_SPECIAL",
    "NOBLE_ITEM",
}

PVAL_FLAG_TOKENS = {
    "STR",
    "DEX",
    "CON",
    "GRA",
    "NEG_STR",
    "NEG_DEX",
    "NEG_CON",
    "NEG_GRA",
    "DAMAGE_SIDES",
    "MEL",
    "MELEE",
    "ARC",
    "ARCHERY",
    "STL",
    "STEALTH",
    "PER",
    "PERCEPTION",
    "WIL",
    "WILL",
    "SMT",
    "SMITHING",
    "SNG",
    "SONG",
    "TUNNEL",
}

_INT_RE = re.compile(r"-?\d+")
AbilityRef = tuple[int, int]


@dataclass(frozen=True)
class AllocPair:
    depth: int
    rarity: int


@dataclass(frozen=True)
class StatRange:
    att_min: int
    att_max: int
    ds_min: int
    ds_max: int
    evn_min: int
    evn_max: int
    ps_min: int
    ps_max: int
    pval_min: int
    pval_max: int
    dd_min: int
    dd_max: int
    pd_min: int
    pd_max: int
    pval_allowed: bool


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


def _flags_to_str(flags: Iterable[str]) -> str:
    return "|".join(sorted({flag for flag in flags if flag}))


def _combined_flags_str(*flag_sets: Iterable[str]) -> str:
    merged: set[str] = set()
    for flag_set in flag_sets:
        merged.update(flag_set)
    return _flags_to_str(merged)


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


def _shorten_display_name(name: str) -> str:
    short_name = name.strip()
    for prefix in ("pair of ", "set of "):
        if short_name.lower().startswith(prefix):
            short_name = short_name[len(prefix):].strip()
            break
    if short_name.lower().startswith("the "):
        short_name = short_name[4:].strip()
    short_name = re.sub(r"\bmail corslet\b", "Corslet", short_name, flags=re.IGNORECASE)
    return short_name


def _parse_int(value: str, default: int = 0) -> int:
    try:
        return int(value.strip())
    except (TypeError, ValueError, AttributeError):
        return default


def _parse_dice(value: str) -> tuple[int, int]:
    if "d" not in value:
        return 0, 0
    left, right = value.split("d", 1)
    return _parse_int(left), _parse_int(right)


def _parse_ability_pairs(payload: str) -> List[AbilityRef]:
    abilities: List[AbilityRef] = []
    for part in payload.split(":"):
        if "/" not in part:
            continue
        left, right = part.split("/", 1)
        try:
            abilities.append((int(left.strip()), int(right.strip())))
        except ValueError:
            continue
    return abilities


def parse_ability_txt(path: str) -> dict[AbilityRef, str]:
    ability_names: dict[AbilityRef, str] = {}
    current_name: Optional[str] = None

    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            if line.startswith("N:"):
                match = re.match(r"^N:\d+:(.*)$", line)
                current_name = match.group(1).strip() if match else None
                continue

            if line.startswith("I:") and current_name is not None:
                parts = [part.strip() for part in line[2:].split(":")]
                if len(parts) >= 2:
                    try:
                        skill_num = int(parts[0])
                        ability_num = int(parts[1])
                    except ValueError:
                        continue
                    ability_names[(skill_num, ability_num)] = current_name

    return ability_names


def _normalize_bonus_token(token: str) -> str:
    normalized = token.strip().upper()
    if normalized.startswith("NEG_"):
        base = BONUS_TOKEN_ALIASES.get(normalized[4:], normalized[4:])
        return f"NEG_{base}"
    return BONUS_TOKEN_ALIASES.get(normalized, normalized)


def _normalize_flag_token(token: str) -> str:
    normalized = token.strip().upper()
    return FLAG_ALIASES.get(normalized, normalized)


def _smithing_step_from_ego_bonus(value: int) -> int:
    if value > 0:
        return 1
    if value < 0:
        return -1
    return 0


def _kind_allows_pval(kind: "ObjectKind") -> bool:
    if kind.pval != 0:
        return True
    return any(flag in PVAL_FLAG_TOKENS for flag in kind.flags)


def _normalize_stat_range(stats: StatRange) -> StatRange:
    ds_min = max(0, stats.ds_min)
    ds_max = max(0, stats.ds_max)
    dd_min = max(0, stats.dd_min)
    dd_max = max(0, stats.dd_max)
    pd_min = max(0, stats.pd_min)
    pd_max = max(0, stats.pd_max)
    ps_min = max(0, stats.ps_min)
    ps_max = max(0, stats.ps_max)

    att_min = stats.att_min
    att_max = stats.att_max
    evn_min = stats.evn_min
    evn_max = stats.evn_max
    pval_min = stats.pval_min
    pval_max = stats.pval_max

    if att_min > att_max:
        att_min = att_max
    if ds_min > ds_max:
        ds_min = ds_max
    if evn_min > evn_max:
        evn_min = evn_max
    if ps_min > ps_max:
        ps_min = ps_max
    if pval_min > pval_max:
        pval_min = pval_max
    if dd_min > dd_max:
        dd_min = dd_max
    if pd_min > pd_max:
        pd_min = pd_max

    return StatRange(
        att_min=att_min,
        att_max=att_max,
        ds_min=ds_min,
        ds_max=ds_max,
        evn_min=evn_min,
        evn_max=evn_max,
        ps_min=ps_min,
        ps_max=ps_max,
        pval_min=pval_min,
        pval_max=pval_max,
        dd_min=dd_min,
        dd_max=dd_max,
        pd_min=pd_min,
        pd_max=pd_max,
        pval_allowed=stats.pval_allowed,
    )


def _stats_to_str(stats: StatRange) -> str:
    pval = "n/a" if not stats.pval_allowed else f"{stats.pval_min}..{stats.pval_max}"
    return (
        f"att={stats.att_min}..{stats.att_max},"
        f"dd={stats.dd_min}..{stats.dd_max},"
        f"ds={stats.ds_min}..{stats.ds_max},"
        f"evn={stats.evn_min}..{stats.evn_max},"
        f"pd={stats.pd_min}..{stats.pd_max},"
        f"ps={stats.ps_min}..{stats.ps_max},"
        f"pval={pval}"
    )


def _base_stat_range(kind: "ObjectKind") -> StatRange:
    stats = StatRange(
        att_min=kind.att,
        att_max=kind.max_att,
        ds_min=kind.ds,
        ds_max=kind.max_ds,
        evn_min=kind.evn,
        evn_max=kind.max_evn,
        ps_min=kind.ps,
        ps_max=kind.max_ps,
        pval_min=kind.pval,
        pval_max=kind.max_pval,
        dd_min=kind.dd,
        dd_max=kind.dd,
        pd_min=kind.pd,
        pd_max=kind.pd,
        pval_allowed=_kind_allows_pval(kind),
    )
    return _normalize_stat_range(stats)


def _single_ego_stat_range(kind: "ObjectKind", ego: "Ego") -> StatRange:
    ego_pval_min_inc = ego.min_pval if ego.min_pval > 0 else (1 if ego.max_pval > 0 else 0)
    stats = StatRange(
        att_min=kind.att + _smithing_step_from_ego_bonus(ego.max_att),
        att_max=kind.max_att + ego.max_att,
        ds_min=kind.ds + _smithing_step_from_ego_bonus(ego.to_ds),
        ds_max=kind.max_ds + ego.to_ds,
        evn_min=kind.evn + _smithing_step_from_ego_bonus(ego.max_evn),
        evn_max=kind.max_evn + ego.max_evn,
        ps_min=kind.ps + _smithing_step_from_ego_bonus(ego.to_ps),
        ps_max=kind.max_ps + ego.to_ps,
        pval_min=kind.pval + ego_pval_min_inc,
        pval_max=kind.max_pval + ego.max_pval,
        dd_min=kind.dd + _smithing_step_from_ego_bonus(ego.to_dd),
        dd_max=kind.dd + ego.to_dd,
        pd_min=kind.pd + _smithing_step_from_ego_bonus(ego.to_pd),
        pd_max=kind.pd + ego.to_pd,
        pval_allowed=_kind_allows_pval(kind) or ego.max_pval > 0,
    )
    return _normalize_stat_range(stats)


def _dual_ego_stat_range(kind: "ObjectKind", prefix: "Ego", suffix: "Ego") -> StatRange:
    max_att_bonus = prefix.max_att + suffix.max_att
    max_evn_bonus = prefix.max_evn + suffix.max_evn
    to_dd_bonus = prefix.to_dd + suffix.to_dd
    to_ds_bonus = prefix.to_ds + suffix.to_ds
    to_pd_bonus = prefix.to_pd + suffix.to_pd
    to_ps_bonus = prefix.to_ps + suffix.to_ps
    max_pval_bonus = prefix.max_pval + suffix.max_pval
    prefix_pval_min_inc = prefix.min_pval if prefix.min_pval > 0 else (1 if prefix.max_pval > 0 else 0)
    suffix_pval_min_inc = suffix.min_pval if suffix.min_pval > 0 else (1 if suffix.max_pval > 0 else 0)

    stats = StatRange(
        att_min=kind.att + _smithing_step_from_ego_bonus(prefix.max_att) + _smithing_step_from_ego_bonus(suffix.max_att),
        att_max=kind.max_att + max_att_bonus,
        ds_min=kind.ds + _smithing_step_from_ego_bonus(prefix.to_ds) + _smithing_step_from_ego_bonus(suffix.to_ds),
        ds_max=kind.max_ds + to_ds_bonus,
        evn_min=kind.evn + _smithing_step_from_ego_bonus(prefix.max_evn) + _smithing_step_from_ego_bonus(suffix.max_evn),
        evn_max=kind.max_evn + max_evn_bonus,
        ps_min=kind.ps + _smithing_step_from_ego_bonus(prefix.to_ps) + _smithing_step_from_ego_bonus(suffix.to_ps),
        ps_max=kind.max_ps + to_ps_bonus,
        pval_min=kind.pval + prefix_pval_min_inc + suffix_pval_min_inc,
        pval_max=kind.max_pval + max_pval_bonus,
        dd_min=kind.dd + _smithing_step_from_ego_bonus(prefix.to_dd) + _smithing_step_from_ego_bonus(suffix.to_dd),
        dd_max=kind.dd + to_dd_bonus,
        pd_min=kind.pd + _smithing_step_from_ego_bonus(prefix.to_pd) + _smithing_step_from_ego_bonus(suffix.to_pd),
        pd_max=kind.pd + to_pd_bonus,
        pval_allowed=_kind_allows_pval(kind) or max_pval_bonus > 0,
    )
    return _normalize_stat_range(stats)


def _stats_row_fields(stats: StatRange) -> dict[str, object]:
    return {
        "combined_stats": _stats_to_str(stats),
        "att_min": stats.att_min,
        "att_max": stats.att_max,
        "dd_min": stats.dd_min,
        "dd_max": stats.dd_max,
        "ds_min": stats.ds_min,
        "ds_max": stats.ds_max,
        "evn_min": stats.evn_min,
        "evn_max": stats.evn_max,
        "pd_min": stats.pd_min,
        "pd_max": stats.pd_max,
        "ps_min": stats.ps_min,
        "ps_max": stats.ps_max,
        "pval_min": stats.pval_min,
        "pval_max": stats.pval_max,
        "pval_allowed": int(stats.pval_allowed),
    }


def _normalized_flags(flags: Iterable[str]) -> set[str]:
    return {_normalize_bonus_token(flag) for flag in flags if flag}


def _token_delta_sign(flags: Iterable[str], token: str) -> int:
    normalized_flags = _normalized_flags(flags)
    if token in STAT_BONUS_TOKENS:
        return int(token in normalized_flags) - int(f"NEG_{token}" in normalized_flags)
    return 1 if token in normalized_flags else 0


def _component_default_bonus(component: object, token: str, pval: int) -> int:
    overrides = getattr(component, "bonus_overrides", {})
    if token in overrides:
        return int(overrides[token])
    return _token_delta_sign(getattr(component, "flags", set()), token) * pval


def _item_bonus_at_base_pval(
    kind: ObjectKind,
    extra_components: Sequence[object],
    token: str,
) -> int:
    bonus = _component_default_bonus(kind, token, kind.pval)

    for component in extra_components:
        sign = _token_delta_sign(getattr(component, "flags", set()), token)
        overrides = getattr(component, "bonus_overrides", {})
        explicit = int(overrides.get(token, 0))

        if sign != 0:
            if bonus == 0:
                bonus = sign * kind.pval
            bonus += explicit
        elif explicit != 0:
            bonus += explicit

    return bonus


def _combined_delta_sign(kind: ObjectKind, extra_components: Sequence[object], token: str) -> int:
    combined_flags = set(kind.flags)
    for component in extra_components:
        combined_flags.update(getattr(component, "flags", set()))
    return _token_delta_sign(combined_flags, token)


def _bonus_range_for_item(
    kind: ObjectKind,
    stats: StatRange,
    components: Sequence[object],
    token: str,
) -> tuple[int, int]:
    base_bonus = _item_bonus_at_base_pval(kind, components[1:], token)
    if not stats.pval_allowed:
        return base_bonus, base_bonus

    sign = _combined_delta_sign(kind, components[1:], token)
    low = base_bonus + sign * (stats.pval_min - kind.pval)
    high = base_bonus + sign * (stats.pval_max - kind.pval)
    return min(low, high), max(low, high)


def _range_diff(base_range: tuple[int, int], current_range: tuple[int, int]) -> tuple[int, int]:
    low = current_range[0] - base_range[0]
    high = current_range[1] - base_range[1]
    return min(low, high), max(low, high)


def _is_zero_range(value_range: tuple[int, int]) -> bool:
    return value_range[0] == 0 and value_range[1] == 0


def _format_signed_range(minimum: int, maximum: int) -> str:
    if minimum == maximum:
        return f"{minimum:+d}"
    if minimum >= 0 and maximum >= 0:
        return f"+{minimum}..{maximum}"
    if minimum <= 0 and maximum <= 0:
        return f"{minimum}..{maximum}"
    return f"{minimum:+d}..{maximum:+d}"


def _format_unsigned_range(minimum: int, maximum: int) -> str:
    if minimum == maximum:
        return str(minimum)
    return f"{minimum}..{maximum}"


def _format_prefixed_delta(minimum: int, maximum: int, prefix: str = "", suffix: str = "") -> str:
    if minimum == maximum:
        sign = "+" if minimum >= 0 else "-"
        return f"{sign}{prefix}{abs(minimum)}{suffix}"

    if minimum >= 0 and maximum >= 0:
        return f"+{prefix}{minimum}..{maximum}{suffix}"

    if minimum <= 0 and maximum <= 0:
        return f"-{prefix}{abs(maximum)}..{abs(minimum)}{suffix}"

    return f"{prefix}{minimum:+d}..{maximum:+d}{suffix}"


def _format_dice_range(dd_min: int, dd_max: int, ds_min: int, ds_max: int) -> str:
    return f"{_format_unsigned_range(dd_min, dd_max)}d{_format_unsigned_range(ds_min, ds_max)}"


def _damage_sides_range(
    kind: ObjectKind,
    stats: StatRange,
    components: Sequence[object],
) -> tuple[int, int]:
    return _bonus_range_for_item(kind, stats, components, "DAMAGE_SIDES")


def _visible_binary_flags(flags: Iterable[str]) -> list[str]:
    visible: list[str] = []
    for flag in sorted({flag for flag in flags if flag}):
        normalized = _normalize_bonus_token(flag)
        if normalized in PVAL_FLAG_TOKENS:
            continue
        if flag in HIDDEN_BINARY_FLAGS:
            continue
        visible.append(flag)
    return visible


def _ability_counts_with_order(components: Sequence[object]) -> tuple[Counter[AbilityRef], list[AbilityRef]]:
    counts: Counter[AbilityRef] = Counter()
    order: list[AbilityRef] = []

    for component in components:
        for ability in getattr(component, "abilities", []):
            if counts[ability] == 0:
                order.append(ability)
            counts[ability] += 1

    return counts, order


def _format_ability_ref(ability: AbilityRef, ability_names: dict[AbilityRef, str]) -> str:
    return ability_names.get(ability, f"Ability {ability[0]}/{ability[1]}")


def _format_ability_parts(
    counts: Counter[AbilityRef],
    order: Sequence[AbilityRef],
    ability_names: dict[AbilityRef, str],
) -> list[str]:
    parts: list[str] = []

    for ability in order:
        count = counts.get(ability, 0)
        if count <= 0:
            continue
        label = _format_ability_ref(ability, ability_names)
        parts.append(f"{label} x{count}" if count > 1 else label)

    return parts


def _abilities_to_str(components: Sequence[object], ability_names: dict[AbilityRef, str]) -> str:
    counts, order = _ability_counts_with_order(components)
    return "|".join(_format_ability_parts(counts, order, ability_names))


def _absolute_combat_summary(kind: ObjectKind, stats: StatRange, components: Sequence[object]) -> list[str]:
    summary_parts: list[str] = []
    damage_bonus = _damage_sides_range(kind, stats, components)
    effective_ds_min = stats.ds_min + damage_bonus[0]
    effective_ds_max = stats.ds_max + damage_bonus[1]

    show_weapon = (
        stats.att_min != 0
        or stats.att_max != 0
        or stats.dd_min != 0
        or stats.dd_max != 0
        or effective_ds_min != 0
        or effective_ds_max != 0
    )
    if show_weapon:
        weapon_parts = [_format_signed_range(stats.att_min, stats.att_max)]
        if stats.dd_min != 0 or stats.dd_max != 0 or effective_ds_min != 0 or effective_ds_max != 0:
            weapon_parts.append(_format_dice_range(stats.dd_min, stats.dd_max, effective_ds_min, effective_ds_max))
        summary_parts.append(f"({', '.join(weapon_parts)})")

    show_defense = (
        stats.evn_min != 0
        or stats.evn_max != 0
        or stats.pd_min != 0
        or stats.pd_max != 0
        or stats.ps_min != 0
        or stats.ps_max != 0
    )
    if show_defense:
        defense_parts = [_format_signed_range(stats.evn_min, stats.evn_max)]
        if stats.pd_min != 0 or stats.pd_max != 0 or stats.ps_min != 0 or stats.ps_max != 0:
            defense_parts.append(_format_dice_range(stats.pd_min, stats.pd_max, stats.ps_min, stats.ps_max))
        summary_parts.append(f"[{', '.join(defense_parts)}]")

    return summary_parts


def _delta_combat_summary(
    kind: ObjectKind,
    base_stats: StatRange,
    current_stats: StatRange,
    base_components: Sequence[object],
    current_components: Sequence[object],
) -> list[str]:
    summary_parts: list[str] = []

    att_delta = _range_diff(
        (base_stats.att_min, base_stats.att_max),
        (current_stats.att_min, current_stats.att_max),
    )
    dd_delta = _range_diff(
        (base_stats.dd_min, base_stats.dd_max),
        (current_stats.dd_min, current_stats.dd_max),
    )
    base_ds_range = (
        base_stats.ds_min + _damage_sides_range(kind, base_stats, base_components)[0],
        base_stats.ds_max + _damage_sides_range(kind, base_stats, base_components)[1],
    )
    current_ds_range = (
        current_stats.ds_min + _damage_sides_range(kind, current_stats, current_components)[0],
        current_stats.ds_max + _damage_sides_range(kind, current_stats, current_components)[1],
    )
    ds_delta = _range_diff(base_ds_range, current_ds_range)

    weapon_parts: list[str] = []
    if not _is_zero_range(att_delta):
        weapon_parts.append(_format_signed_range(att_delta[0], att_delta[1]))
    if not _is_zero_range(dd_delta):
        weapon_parts.append(_format_prefixed_delta(dd_delta[0], dd_delta[1], suffix="d"))
    if not _is_zero_range(ds_delta):
        weapon_parts.append(_format_prefixed_delta(ds_delta[0], ds_delta[1], prefix="d"))
    if weapon_parts:
        summary_parts.append(f"({', '.join(weapon_parts)})")

    evn_delta = _range_diff(
        (base_stats.evn_min, base_stats.evn_max),
        (current_stats.evn_min, current_stats.evn_max),
    )
    pd_delta = _range_diff(
        (base_stats.pd_min, base_stats.pd_max),
        (current_stats.pd_min, current_stats.pd_max),
    )
    ps_delta = _range_diff(
        (base_stats.ps_min, base_stats.ps_max),
        (current_stats.ps_min, current_stats.ps_max),
    )

    defense_parts: list[str] = []
    if not _is_zero_range(evn_delta):
        defense_parts.append(_format_signed_range(evn_delta[0], evn_delta[1]))
    if not _is_zero_range(pd_delta):
        defense_parts.append(_format_prefixed_delta(pd_delta[0], pd_delta[1], suffix="d"))
    if not _is_zero_range(ps_delta):
        defense_parts.append(_format_prefixed_delta(ps_delta[0], ps_delta[1], prefix="d"))
    if defense_parts:
        summary_parts.append(f"[{', '.join(defense_parts)}]")

    return summary_parts


def _format_named_bonus(token: str, bonus_range: tuple[int, int]) -> str:
    label = BONUS_DISPLAY_NAMES.get(token, token.title())
    if token == "TUNNEL":
        return f"{label}{_format_signed_range(bonus_range[0], bonus_range[1])}"
    return f"{label}{_format_signed_range(bonus_range[0], bonus_range[1])}"


def _base_summary(kind: ObjectKind, stats: StatRange, ability_names: dict[AbilityRef, str]) -> str:
    components: Sequence[object] = (kind,)
    parts = _absolute_combat_summary(kind, stats, components)

    for token in (*STAT_BONUS_TOKENS, *SKILL_BONUS_TOKENS, "TUNNEL"):
        bonus_range = _bonus_range_for_item(kind, stats, components, token)
        if _is_zero_range(bonus_range):
            continue
        parts.append(_format_named_bonus(token, bonus_range))

    ability_counts, ability_order = _ability_counts_with_order(components)
    parts.extend(_format_ability_parts(ability_counts, ability_order, ability_names))
    parts.extend(_visible_binary_flags(kind.flags))
    return " ".join(parts) if parts else "-"


def _delta_summary(
    kind: ObjectKind,
    stats: StatRange,
    extra_components: Sequence[object],
    ability_names: dict[AbilityRef, str],
) -> str:
    base_stats = _base_stat_range(kind)
    base_components: Sequence[object] = (kind,)
    current_components: Sequence[object] = (kind, *extra_components)
    parts = _delta_combat_summary(kind, base_stats, stats, base_components, current_components)

    for token in (*STAT_BONUS_TOKENS, *SKILL_BONUS_TOKENS, "TUNNEL"):
        base_bonus = _bonus_range_for_item(kind, base_stats, base_components, token)
        current_bonus = _bonus_range_for_item(kind, stats, current_components, token)
        delta = _range_diff(base_bonus, current_bonus)
        if _is_zero_range(delta):
            continue
        parts.append(_format_named_bonus(token, delta))

    base_ability_counts, _ = _ability_counts_with_order(base_components)
    current_ability_counts, current_ability_order = _ability_counts_with_order(current_components)
    delta_ability_counts: Counter[AbilityRef] = Counter()
    delta_ability_order: list[AbilityRef] = []
    for ability in current_ability_order:
        diff = current_ability_counts[ability] - base_ability_counts[ability]
        if diff > 0:
            delta_ability_counts[ability] = diff
            delta_ability_order.append(ability)
    parts.extend(_format_ability_parts(delta_ability_counts, delta_ability_order, ability_names))

    base_visible_flags = set(_visible_binary_flags(kind.flags))
    current_flags = set(kind.flags)
    for component in extra_components:
        current_flags.update(getattr(component, "flags", set()))
    for flag in _visible_binary_flags(current_flags):
        if flag not in base_visible_flags:
            parts.append(flag)

    return " ".join(parts) if parts else "-"


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
    pval: int = 0
    level: int = 1  # W: depth
    att: int = 0
    dd: int = 0
    ds: int = 0
    evn: int = 0
    pd: int = 0
    ps: int = 0
    max_att: int = 0
    max_ds: int = 0
    max_evn: int = 0
    max_ps: int = 0
    max_pval: int = 0
    alloc_pairs: List[AllocPair] = field(default_factory=list)
    flags: set[str] = field(default_factory=set)
    bonus_overrides: dict[str, int] = field(default_factory=dict)
    abilities: List[AbilityRef] = field(default_factory=list)

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
    max_att: int = 0
    to_dd: int = 0
    to_ds: int = 0
    max_evn: int = 0
    to_pd: int = 0
    to_ps: int = 0
    max_pval: int = 0
    min_pval: int = 0
    tval_ranges: List[Tuple[int, int, int]] = field(default_factory=list)  # (tval, min_sval, max_sval)
    alloc_pairs: List[AllocPair] = field(default_factory=list)
    flags: set[str] = field(default_factory=set)
    bonus_overrides: dict[str, int] = field(default_factory=dict)
    abilities: List[AbilityRef] = field(default_factory=list)

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
                if len(parts) >= 3:
                    current.pval = _parse_int(parts[2], current.pval)
                    current.max_pval = current.pval
                continue

            if line.startswith("P:"):
                parts = [p.strip() for p in line[2:].split(":")]
                if len(parts) >= 4:
                    current.att = _parse_int(parts[0], current.att)
                    current.dd, current.ds = _parse_dice(parts[1])
                    current.evn = _parse_int(parts[2], current.evn)
                    current.pd, current.ps = _parse_dice(parts[3])
                    current.max_att = current.att
                    current.max_ds = current.ds
                    current.max_evn = current.evn
                    current.max_ps = current.ps
                continue

            if line.startswith("R:"):
                parts = [p.strip() for p in line[2:].split(":", 1)]
                if len(parts) == 2:
                    stat_name = parts[0].upper()
                    value = _parse_int(parts[1], 0)
                    if stat_name == "ATT":
                        current.max_att = value
                    elif stat_name == "DS":
                        current.max_ds = value
                    elif stat_name == "EVN":
                        current.max_evn = value
                    elif stat_name == "PS":
                        current.max_ps = value
                    elif stat_name == "PVAL":
                        current.max_pval = value
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
                        current.flags.add(_normalize_flag_token(fl))
                continue

            if line.startswith("B:"):
                current.abilities.extend(_parse_ability_pairs(line[2:]))
                continue

            if line.startswith("M:"):
                parts = [p.strip() for p in line[2:].split(":", 1)]
                if len(parts) == 2:
                    token = _normalize_bonus_token(parts[0])
                    current.bonus_overrides[token] = _parse_int(parts[1], 0)
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
                parts = [p.strip() for p in line[2:].split(":")]
                if parts and "/" in parts[0]:
                    pairs = _parse_alloc_pairs_from_a_line(line[2:])
                    current.alloc_pairs.extend(pairs)
                elif len(parts) >= 3:
                    count = max(0, _parse_int(parts[0], 0))
                    skill_num = _parse_int(parts[1], -1)
                    ability_num = _parse_int(parts[2], -1)
                    if count > 0 and skill_num >= 0 and ability_num >= 0:
                        current.abilities.extend([(skill_num, ability_num)] * count)
                continue

            if line.startswith("C:"):
                parts = [p.strip() for p in line[2:].split(":")]
                if len(parts) >= 1:
                    current.max_att = _parse_int(parts[0], current.max_att)
                if len(parts) >= 2:
                    current.to_dd = _parse_int(parts[1], current.to_dd)
                if len(parts) >= 3:
                    current.to_ds = _parse_int(parts[2], current.to_ds)
                if len(parts) >= 4:
                    current.max_evn = _parse_int(parts[3], current.max_evn)
                if len(parts) >= 5:
                    current.to_pd = _parse_int(parts[4], current.to_pd)
                if len(parts) >= 6:
                    current.to_ps = _parse_int(parts[5], current.to_ps)
                if len(parts) >= 7:
                    current.max_pval = _parse_int(parts[6], current.max_pval)
                if len(parts) >= 8:
                    current.min_pval = _parse_int(parts[7], current.min_pval)
                if current.max_pval > 0 and current.min_pval == 0:
                    current.min_pval = 1
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
                        current.flags.add(_normalize_flag_token(flag_name))
                continue

            if line.startswith("B:"):
                current.abilities.extend(_parse_ability_pairs(line[2:]))
                continue

            if line.startswith("M:"):
                parts = [p.strip() for p in line[2:].split(":", 1)]
                if len(parts) == 2:
                    token = _normalize_bonus_token(parts[0])
                    current.bonus_overrides[token] = _parse_int(parts[1], 0)
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


def _schedule_has_positive_spawn(pairs: Sequence[AllocPair]) -> bool:
    return any(p.rarity > 0 for p in pairs)


def _ensure_visible_schedule(pairs: Sequence[AllocPair], fallback_depth: int) -> List[AllocPair]:
    if pairs:
        return list(pairs)
    return [AllocPair(depth=max(1, fallback_depth), rarity=0)]


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


def _kind_uses_special_metal(kind: "ObjectKind") -> bool:
    return "MITHRIL" in kind.flags or "STAR_IRON" in kind.flags


def _ego_applies_to_kind(
    ego: "Ego",
    kind: "ObjectKind",
    kinds_by_name: dict[str, "ObjectKind"],
) -> bool:
    if ego.applies_to(kind):
        return True

    if "EVIL_ITEM" not in ego.flags:
        return False

    basis_name = DAMAGED_KIND_BASIS_NAMES.get(kind.name)
    if not basis_name:
        return False

    basis_kind = kinds_by_name.get(basis_name)
    if basis_kind is None:
        return False

    return ego.applies_to(basis_kind)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="List all valid base-item × ego combinations with combined rarity schedules and stat ranges",
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
        "--ability-txt",
        default=DEFAULT_ABILITY_TXT,
        help="Path to ability.txt (default: <repo>/lib/edit/ability.txt)",
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
        "--no-color",
        action="store_true",
        help="Disable ANSI color output in the console table",
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

    global _USE_COLOR
    if args.no_color:
        _USE_COLOR = False

    try:
        selected_categories = _parse_item_categories(args.item_categories)
    except ValueError as e:
        parser.error(str(e))

    kinds = parse_object_txt(args.object_txt)
    egos = parse_special_txt(args.special_txt)
    ability_names = parse_ability_txt(args.ability_txt)

    # Filter to "real" kinds (must have tval/sval).
    kinds = [k for k in kinds if k.tval is not None and k.sval is not None and not k.is_insta_art and "Note" not in k.name]
    egos = [e for e in egos if e.tval_ranges]

    if selected_categories:
        allowed_tvals: set[int] = set()
        for category in selected_categories:
            allowed_tvals.update(CATEGORY_TVALS[category])
        kinds = [k for k in kinds if k.tval in allowed_tvals]

    kinds_by_name = {k.name: k for k in kinds}

    out_dir = os.path.dirname(args.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    rows: List[dict] = []

    # Base items without egos (will be sorted first per group later).
    for kind in kinds:
        base_pairs_orig = _base_schedule_for_kind(kind)
        stat_range = _base_stat_range(kind)
        base_summary = _base_summary(kind, stat_range, ability_names)

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
                "kind_flags": _flags_to_str(kind.flags),
                "ego_flags": "",
                "combined_flags": _flags_to_str(kind.flags),
                "combined_alloc": _pairs_to_str(base_pairs_orig),
                "spawnable": int(_schedule_has_positive_spawn(base_pairs_orig)),
                "min_depth": min_depth,
                "max_depth": max_depth,
                "base_alloc": _pairs_to_str(_sort_alloc_pairs(base_pairs_orig)),
                "ego_alloc": "",
                "kind_abilities": _abilities_to_str((kind,), ability_names),
                "ego_abilities": "",
                "combined_abilities": _abilities_to_str((kind,), ability_names),
                "base_summary": base_summary,
                "delta_summary": "",
                "display_summary": base_summary,
                **_stats_row_fields(stat_range),
            }
        )

    # Single ego variants.
    for kind in kinds:
        base_pairs_orig = _base_schedule_for_kind(kind)
        base_summary = _base_summary(kind, _base_stat_range(kind), ability_names)

        for ego in egos:
            if not _ego_applies_to_kind(ego, kind, kinds_by_name):
                continue
            if _has_alignment_conflict(kind.flags, ego.flags):
                continue
            if _kind_uses_special_metal(kind) and "EVIL_ITEM" in ego.flags:
                continue

            ego_pairs = _ego_schedule_for_ego(ego)
            stat_range = _single_ego_stat_range(kind, ego)
            delta_summary = _delta_summary(kind, stat_range, (ego,), ability_names)

            # Apply MORE_SPECIAL / LESS_SPECIAL modifiers to the *base* schedule when combining single egos.
            base_pairs = list(base_pairs_orig)
            if kind.more_special:
                base_pairs = [AllocPair(p.depth, _more_special_rarity_bonus(p.rarity)) for p in base_pairs]
            if kind.less_special:
                base_pairs = [AllocPair(p.depth, _less_special_rarity_penalty(p.rarity)) for p in base_pairs]

            base_min = _schedule_min_depth([p.depth for p in base_pairs_orig], fallback=max(kind.level, 1))
            ego_min = _schedule_min_depth([p.depth for p in ego_pairs], fallback=max(ego.level, 1))
            min_depth = max(1, base_min, ego_min)
            combined_pairs = _combine_allocations(base_pairs, ego_pairs)
            combined_pairs = _ensure_visible_schedule(combined_pairs, min_depth)
            spawnable = _schedule_has_positive_spawn(combined_pairs)

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
                    "kind_flags": _flags_to_str(kind.flags),
                    "ego_flags": _flags_to_str(ego.flags),
                    "combined_flags": _combined_flags_str(kind.flags, ego.flags),
                    "combined_alloc": _pairs_to_str(combined_pairs),
                    "spawnable": int(spawnable),
                    "min_depth": min_depth,
                    "max_depth": max_depth,
                    "base_alloc": _pairs_to_str(_sort_alloc_pairs(base_pairs_orig)),
                    "ego_alloc": _pairs_to_str(_sort_alloc_pairs(ego_pairs)),
                    "kind_abilities": _abilities_to_str((kind,), ability_names),
                    "ego_abilities": _abilities_to_str((ego,), ability_names),
                    "combined_abilities": _abilities_to_str((kind, ego), ability_names),
                    "base_summary": base_summary,
                    "delta_summary": delta_summary,
                    "display_summary": delta_summary,
                    **_stats_row_fields(stat_range),
                }
            )

    # Prefix+suffix combos.
    if args.include_combos:
        prefixes = [e for e in egos if e.is_prefix]
        suffixes = [e for e in egos if not e.is_prefix]

        for kind in kinds:
            base_pairs_orig = _base_schedule_for_kind(kind)
            base_summary = _base_summary(kind, _base_stat_range(kind), ability_names)

            for prefix_ego in prefixes:
                if not _ego_applies_to_kind(prefix_ego, kind, kinds_by_name):
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
                    if not _ego_applies_to_kind(suffix_ego, kind, kinds_by_name):
                        continue
                    if _has_alignment_conflict(kind.flags, prefix_ego.flags, suffix_ego.flags):
                        continue
                    if _kind_uses_special_metal(kind) and (
                        "EVIL_ITEM" in prefix_ego.flags or "EVIL_ITEM" in suffix_ego.flags
                    ):
                        continue

                    suffix_pairs = _ego_schedule_for_ego(suffix_ego)
                    stat_range = _dual_ego_stat_range(kind, prefix_ego, suffix_ego)
                    delta_summary = _delta_summary(kind, stat_range, (prefix_ego, suffix_ego), ability_names)

                    base_min = _schedule_min_depth([p.depth for p in base_pairs_orig], fallback=max(kind.level, 1))
                    p_min = _schedule_min_depth([p.depth for p in prefix_pairs], fallback=max(prefix_ego.level, 1))
                    s_min = _schedule_min_depth([p.depth for p in suffix_pairs], fallback=max(suffix_ego.level, 1))
                    min_depth = max(1, base_min, p_min, s_min)
                    combined_pairs = _combine_allocations(tmp_pairs, suffix_pairs)
                    combined_pairs = _ensure_visible_schedule(combined_pairs, min_depth)
                    spawnable = _schedule_has_positive_spawn(combined_pairs)

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
                            "kind_flags": _flags_to_str(kind.flags),
                            "ego_flags": _flags_to_str(set(prefix_ego.flags).union(suffix_ego.flags)),
                            "combined_flags": _combined_flags_str(kind.flags, prefix_ego.flags, suffix_ego.flags),
                            "combined_alloc": _pairs_to_str(combined_pairs),
                            "spawnable": int(spawnable),
                            "min_depth": min_depth,
                            "max_depth": max_depth,
                            "base_alloc": _pairs_to_str(_sort_alloc_pairs(base_pairs_orig)),
                            "ego_alloc": _pairs_to_str(_sort_alloc_pairs(prefix_pairs))
                            + " + "
                            + _pairs_to_str(_sort_alloc_pairs(suffix_pairs)),
                            "kind_abilities": _abilities_to_str((kind,), ability_names),
                            "ego_abilities": _abilities_to_str((prefix_ego, suffix_ego), ability_names),
                            "combined_abilities": _abilities_to_str((kind, prefix_ego, suffix_ego), ability_names),
                            "base_summary": base_summary,
                            "delta_summary": delta_summary,
                            "display_summary": delta_summary,
                            **_stats_row_fields(stat_range),
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
        "kind_flags",
        "ego_flags",
        "combined_flags",
        "combined_alloc",
        "spawnable",
        "combined_stats",
        "att_min",
        "att_max",
        "dd_min",
        "dd_max",
        "ds_min",
        "ds_max",
        "evn_min",
        "evn_max",
        "pd_min",
        "pd_max",
        "ps_min",
        "ps_max",
        "pval_min",
        "pval_max",
        "pval_allowed",
        "min_depth",
        "max_depth",
        "base_alloc",
        "ego_alloc",
        "kind_abilities",
        "ego_abilities",
        "combined_abilities",
        "base_summary",
        "delta_summary",
        "display_summary",
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
                    "kind_name": _shorten_display_name(str(r.get("kind_name", "") or "")),
                    "ego": ego_name,
                    "details": r.get("display_summary", ""),
                    "combined_alloc": r.get("combined_alloc", ""),
                    "depths": depths,
                    "alignment": _alignment_from_flags(r.get("combined_flags", "")),
                }
            )

        _print_table(
            table_rows,
            columns=["kind_name", "ego", "details", "combined_alloc", "depths"],
            limit=None if args.table_all else max(0, args.table_limit),
            max_col_width=max(0, int(args.table_max_col_width)),
        )
        print(
            "Colors:  "
            + _yellow("noble")
            + " | "
            + _green("normal")
            + " | "
            + _red("evil")
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

    # helper that formats a cell value; the `details` column is exempt from
    # truncation so that users always see the entire summary string even when
    # a global max_col_width is specified.
    def cell_str(value: object, col: str) -> str:
        if value is None:
            s = ""
        else:
            s = str(value)
        # don't truncate the details column
        if col == "details":
            return s
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
            widths[col] = max(widths[col], len(cell_str(r.get(col, ""), col)))

    header = " | ".join(col.ljust(widths[col]) for col in columns)
    sep = "-+-".join("-" * widths[col] for col in columns)
    print("\n" + header)
    print(sep)

    for r in view:
        color_fn = ALIGNMENT_COLORS.get(str(r.get("alignment", "") or ""), lambda text: text)
        rendered_cells: List[str] = []
        for col in columns:
            cell = cell_str(r.get(col, ""), col).ljust(widths[col])
            if col in {"kind_name", "ego"}:
                cell = color_fn(cell)
            rendered_cells.append(cell)
        print(" | ".join(rendered_cells))

    if limit is not None and len(rows) > len(view):
        print(f"... ({len(rows) - len(view)} more rows; use --table-all to print all)")


if __name__ == "__main__":
    main()
