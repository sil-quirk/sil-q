#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the monster.txt data file.

1. Validates the format and integrity of lib/edit/monster.txt.
2. Exports the monster/player records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: serial number : monster name
- W: depth : rarity
- G: symbol : color
- I: speed : health : mana : light radius
- A: sleepiness : perception : stealth : will
- P: [evasion bonus, protection dice]
- B: attack method : attack effect : (attack bonus, damage dice)
- S: spell frequency | spell power / spell type | spell type | etc
- F: flag | flag | etc
- D: Description

Exit codes:
  0 - All validations passed
  1 - One or more validation errors found


Documentation vs. Actual Data Discrepancies
===========================================

The following discrepancies were found between the documented format in the
comment header of monster.txt and the actual data records in the file:

1. I: line format
   - Documented: "speed : health : mana : light radius" (4 values)
   - Actual: "speed : health_dice : light_radius" (3 values after I:).
     The "mana" field mentioned in docs does not exist in practice.

2. I: light radius values
   - Documented: No mention of negative values.
   - Actual: Light radius can be negative (e.g., -2, -3), indicating the
     creature creates darkness around it rather than light.

3. B: damage format
   - Documented: "(attack bonus, damage dice)" implying both are required.
   - Actual: Damage can also be just "(+N)" without dice, used for effects
     like TERRIFY, LOSE_DEX, LOSE_STR that don't deal direct damage.

4. G: color codes
   - Documented: 16 colors listed: D,w,s,o,r,g,b,u,d,W,v,y,R,G,B,U
   - Actual: Extended colors with numeric suffixes are also used, which are not
     documented in the comment header of monster.txt:
     D1, v1, y1, U1, G1, B1, b1 (possibly shade/intensity variations?)
"""

import argparse
import json
import re
import signal
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import cast

# JSON-compatible types
type JsonValue = str | int | float | bool | None | list[JsonValue] | dict[str, JsonValue]
type JsonDict = dict[str, JsonValue]

# Handle broken pipe (e.g., when piping to head) - Unix only
if hasattr(signal, "SIGPIPE"):
    _ = signal.signal(signal.SIGPIPE, signal.SIG_DFL)


@dataclass
class ValidationResult:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    info: list[str] = field(default_factory=list)

    def error(self, msg: str) -> None:
        self.errors.append(msg)

    def warning(self, msg: str) -> None:
        self.warnings.append(msg)

    def log_info(self, msg: str) -> None:
        self.info.append(msg)

    @property
    def is_valid(self) -> bool:
        return len(self.errors) == 0


# Valid colors per the documented specification (16 colors):
# D - Dark Gray    w - White          s - Gray          o - Orange
# r - Red          g - Green          b - Blue          u - Brown
# d - Black        W - Light Gray     v - Violet        y - Yellow
# R - Light Red    G - Light Green    B - Light Blue    U - Light Brown
#
# Note: The actual file also uses extended colors with numeric suffixes
# (e.g., D1, v1, y1) which are not documented in the comment section.
DOCUMENTED_COLORS = {"D", "w", "s", "o", "r", "g", "b", "u", "d", "W", "v", "y", "R", "G", "B", "U"}
EXTENDED_COLORS = {"D1", "v1", "y1", "U1", "G1", "B1", "b1"}
VALID_COLORS = DOCUMENTED_COLORS | EXTENDED_COLORS

###
### Regex patterns for validation
###
DICE_PATTERN = re.compile(r"^\d+d\d+$")
PROTECTION_PATTERN = re.compile(r"^\[[+-]\d+(,\d+d\d+)?\]$")
# Damage can be (+N,NdM) or just (+N) for effects that don't deal damage
DAMAGE_PATTERN = re.compile(r"^\([+-]\d+(,\d+d\d+)?\)$")
SPELL_PCT_PATTERN = re.compile(r"SPELL_PCT_\d+")
# Light radius can be negative (creature creates darkness)
LIGHT_RADIUS_PATTERN = re.compile(r"^-?\d+$")


@dataclass
class Limits:
    """Limits parsed from lib/edit/limits.txt."""

    max_monster_races: int  # M:R value (note: limits.txt stores max+1)

    @property
    def max_monster_id(self) -> int:
        """Maximum valid monster ID (0-indexed, so max_monster_races - 1)."""
        return self.max_monster_races - 1


@dataclass
class Attack:
    """A monster attack (B: line)."""

    method: str
    effect: str | None = None
    attack_bonus: int | None = None
    damage_dice: str | None = None


@dataclass
class SpellInfo:
    """Spell information (S: lines)."""

    frequency: int | None = None  # SPELL_PCT_X value
    power: int | None = None  # POW_X value
    spells: list[str] = field(default_factory=list)  # List of spell types


@dataclass
class Monster:
    """A monster record parsed from monster.txt."""

    id: int
    name: str
    depth: int | None = None
    rarity: int | None = None
    symbol: str | None = None
    color: str | None = None
    speed: int | None = None
    health_dice: str | None = None
    light_radius: int | None = None
    sleepiness: int | None = None
    perception: int | None = None
    stealth: int | None = None
    will: int | None = None
    evasion_bonus: int | None = None
    protection_dice: str | None = None
    attacks: list[Attack] = field(default_factory=list)
    spell_info: SpellInfo | None = None
    flags: list[str] = field(default_factory=list)
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_monster_races = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:R:"):
            # M:R:656 - Maximum number of monster races
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_monster_races = int(parts[2])

    if max_monster_races is None:
        return None

    return Limits(max_monster_races=max_monster_races)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> int | None:
    """Validate N: line format and return the ID."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected 3: {line}")
        return None

    id_str = parts[1]
    if not id_str.isdigit():
        result.error(f"Line {lineno}: N: ID is not numeric: {id_str}")
        return None

    return int(id_str)


def validate_w_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate W: line format (depth : rarity)."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: W: line has {len(parts)} fields, expected 3: {line}")
        return

    depth, rarity = parts[1], parts[2]
    if not depth.isdigit():
        result.error(f"Line {lineno}: W: depth is not numeric: {depth}")
    if not rarity.isdigit():
        result.error(f"Line {lineno}: W: rarity is not numeric: {rarity}")


def validate_g_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate G: line format (symbol : color)."""
    parts = line.split(":")
    if len(parts) != 3:
        result.error(f"Line {lineno}: G: line has {len(parts)} fields, expected 3: {line}")
        return

    symbol, color = parts[1], parts[2]
    if len(symbol) != 1:
        result.error(f"Line {lineno}: G: symbol should be single character, got '{symbol}'")

    if color not in VALID_COLORS:
        result.warning(f"Line {lineno}: G: color '{color}' not in documented color list")


def validate_i_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate I: line format (speed : health : light radius).

    Note: The documentation says "speed : health : mana : light radius" but
    in practice, the mana field appears unused and the format is actually
    "speed : health dice : light radius" where light can be negative
    (indicating the creature creates darkness).
    """
    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: I: line has {len(parts)} fields, expected 4: {line}")
        return

    speed, health, light = parts[1], parts[2], parts[3]

    if not speed.isdigit():
        result.error(f"Line {lineno}: I: speed is not numeric: {speed}")

    if not DICE_PATTERN.match(health):
        result.error(f"Line {lineno}: I: health is not valid dice format (NdM): {health}")

    # Light radius can be negative (creature creates darkness around it)
    if not LIGHT_RADIUS_PATTERN.match(light):
        result.error(f"Line {lineno}: I: light radius is not a valid integer: {light}")


def validate_a_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate A: line format (sleepiness : perception : stealth : will)."""
    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: A: line has {len(parts)} fields, expected 5: {line}")
        return

    for i, name in enumerate(["sleepiness", "perception", "stealth", "will"], start=1):
        val = parts[i]
        if not val.isdigit():
            result.error(f"Line {lineno}: A: {name} is not numeric: {val}")


def validate_p_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate P: line format ([evasion bonus, protection dice])."""
    parts = line.split(":")
    if len(parts) != 2:
        result.error(f"Line {lineno}: P: line has {len(parts)} fields, expected 2: {line}")
        return

    pval = parts[1]
    if not PROTECTION_PATTERN.match(pval):
        result.error(f"Line {lineno}: P: invalid format, expected [+/-N] or [+/-N,NdM]: {pval}")


def validate_b_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate B: line format (method : effect : damage).

    Note: The documentation says damage format is "(attack bonus, damage dice)"
    but in practice, some effects like TERRIFY only have the attack bonus
    without damage dice, e.g., "(+15)" instead of "(+15,1d6)".
    """
    parts = line.split(":")
    # B: should have 2-4 fields (effect and damage are optional per docs)
    if len(parts) < 2 or len(parts) > 4:
        result.error(f"Line {lineno}: B: line has {len(parts)} fields, expected 2-4: {line}")
        return

    # If 4th field exists, check damage format: (+N,NdM) or just (+N)
    if len(parts) == 4:
        damage = parts[3]
        if not DAMAGE_PATTERN.match(damage):
            result.error(f"Line {lineno}: B: invalid damage format, expected (+/-N) or (+/-N,NdM): {damage}")


def validate_s_line_is_first(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate that the first S: line contains SPELL_PCT_X."""
    if not SPELL_PCT_PATTERN.search(line):
        result.error(f"Line {lineno}: First S: line must contain SPELL_PCT_X: {line}")


def parse_attack(line: str) -> Attack | None:
    """Parse a B: line into an Attack object."""
    parts = line.split(":")
    if len(parts) < 2:
        return None

    method = parts[1]
    effect = parts[2] if len(parts) > 2 else None
    attack_bonus = None
    damage_dice = None

    if len(parts) > 3:
        # Parse damage format: (+N,NdM) or (+N)
        damage_str = parts[3]
        match = re.match(r"\(([+-]\d+)(?:,(\d+d\d+))?\)", damage_str)
        if match:
            attack_bonus = int(match.group(1))
            damage_dice = match.group(2)

    return Attack(method=method, effect=effect, attack_bonus=attack_bonus, damage_dice=damage_dice)


def parse_protection(pval: str) -> tuple[int | None, str | None]:
    """Parse a P: line protection value like [+1,1d4] or [+0]."""
    match = re.match(r"\[([+-]\d+)(?:,(\d+d\d+))?\]", pval)
    if match:
        evasion_bonus = int(match.group(1))
        protection_dice = match.group(2)
        return evasion_bonus, protection_dice
    return None, None


def parse_spell_line(line: str, spell_info: SpellInfo) -> None:
    """Parse an S: line and update the SpellInfo object."""
    content = line[2:]  # Remove "S:"
    parts = [p.strip() for p in content.split("|")]

    for part in parts:
        # Check for SPELL_PCT_X
        match = re.match(r"SPELL_PCT_(\d+)", part)
        if match:
            spell_info.frequency = int(match.group(1))
            continue

        # Check for POW_X
        match = re.match(r"POW_(\d+)", part)
        if match:
            spell_info.power = int(match.group(1))
            continue

        # Otherwise it's a spell type
        if part:
            spell_info.spells.append(part)


def parse_monsters(filepath: Path) -> list[Monster]:
    """Parse monster.txt and return a list of Monster objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    monsters: list[Monster] = []
    current_monster: Monster | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new monster
        if line.startswith("N:"):
            # Save previous monster
            if current_monster is not None:
                monsters.append(current_monster)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                current_monster = Monster(id=int(parts[1]), name=parts[2])
            continue

        if current_monster is None:
            continue

        # W: depth : rarity
        if line.startswith("W:"):
            parts = line.split(":")
            if len(parts) >= 3:
                if parts[1].isdigit():
                    current_monster.depth = int(parts[1])
                if parts[2].isdigit():
                    current_monster.rarity = int(parts[2])

        # G: symbol : color
        elif line.startswith("G:"):
            parts = line.split(":")
            if len(parts) >= 3:
                current_monster.symbol = parts[1]
                current_monster.color = parts[2]

        # I: speed : health : light
        elif line.startswith("I:"):
            parts = line.split(":")
            if len(parts) >= 4:
                if parts[1].isdigit():
                    current_monster.speed = int(parts[1])
                if DICE_PATTERN.match(parts[2]):
                    current_monster.health_dice = parts[2]
                if LIGHT_RADIUS_PATTERN.match(parts[3]):
                    current_monster.light_radius = int(parts[3])

        # A: sleepiness : perception : stealth : will
        elif line.startswith("A:"):
            parts = line.split(":")
            if len(parts) >= 5:
                if parts[1].isdigit():
                    current_monster.sleepiness = int(parts[1])
                if parts[2].isdigit():
                    current_monster.perception = int(parts[2])
                if parts[3].isdigit():
                    current_monster.stealth = int(parts[3])
                if parts[4].isdigit():
                    current_monster.will = int(parts[4])

        # P: [evasion, protection]
        elif line.startswith("P:"):
            parts = line.split(":")
            if len(parts) >= 2:
                evasion, protection = parse_protection(parts[1])
                current_monster.evasion_bonus = evasion
                current_monster.protection_dice = protection

        # B: attack
        elif line.startswith("B:"):
            attack = parse_attack(line)
            if attack:
                current_monster.attacks.append(attack)

        # S: spells
        elif line.startswith("S:"):
            if current_monster.spell_info is None:
                current_monster.spell_info = SpellInfo()
            parse_spell_line(line, current_monster.spell_info)

        # F: flags
        elif line.startswith("F:"):
            content = line[2:]  # Remove "F:"
            flags = [f.strip() for f in content.split("|") if f.strip()]
            current_monster.flags.extend(flags)

        # D: description
        elif line.startswith("D:"):
            content = line[2:].strip()  # Remove "D:" and strip whitespace
            if current_monster.description is None:
                current_monster.description = content
            else:
                current_monster.description += " " + content

    if current_monster is not None:
        monsters.append(current_monster)

    return monsters


def export_monsters_to_json(monsters: list[Monster]) -> str:
    """Export monsters to a JSON string."""

    def clean_dict(d: JsonDict) -> JsonDict:
        """Recursively remove None values and empty collections from a dict."""
        result: JsonDict = {}
        for k, v in d.items():
            if v is None or v == [] or v == "":
                continue
            if isinstance(v, dict):
                cleaned = clean_dict(v)
                if cleaned:  # Only include non-empty dicts
                    result[k] = cleaned
            elif isinstance(v, list):
                # Clean each item if it's a dict
                cleaned_list: list[JsonValue] = []
                for item in v:
                    if isinstance(item, dict):
                        cleaned_item = clean_dict(item)
                        if cleaned_item:
                            cleaned_list.append(cleaned_item)
                    elif item is not None:
                        cleaned_list.append(item)
                if cleaned_list:
                    result[k] = cleaned_list
            else:
                result[k] = v
        return result

    def monster_to_dict(monster: Monster) -> JsonDict:
        """Convert a Monster to a dict, removing None values."""
        d = clean_dict(asdict(monster))
        if "flags" in d and isinstance(d["flags"], list):
            d["flags"] = cast(JsonValue, sorted(cast(list[str], d["flags"])))
        if "spell_info" in d and isinstance(d["spell_info"], dict):
            spell_info = d["spell_info"]
            if "spells" in spell_info and isinstance(spell_info["spells"], list):
                spell_info["spells"] = cast(JsonValue, sorted(cast(list[str], spell_info["spells"])))
        return d

    data = {"monsters": [monster_to_dict(m) for m in monsters]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_monster_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire monster.txt file.

    Args:
        filepath: Path to monster.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                monster count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Monster file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state. Maps id -> line number.
    ids_seen: dict[int, int] = {}

    prev_id = -1
    has_version = False
    in_monster = False
    first_s_in_monster = True

    for lineno, line in enumerate(lines, start=1):
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#"):
            continue

        # Version stamp
        if line.startswith("V:"):
            has_version = True
            continue

        # N: line - start of monster entry
        if line.startswith("N:"):
            monster_id = validate_n_line(line, lineno, result)
            if monster_id is not None:
                # Check for duplicates
                if monster_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {monster_id} " +
                        f"(first seen at line {ids_seen[monster_id]})"
                    )
                else:
                    ids_seen[monster_id] = lineno

                # Check IDs are increasing
                if monster_id <= prev_id:
                    result.error(
                        f"Line {lineno}: ID {monster_id} is not greater than previous ID {prev_id} " +
                        "(IDs must be strictly increasing)"
                    )
                prev_id = monster_id

                # Check ID against limit
                if limits and monster_id > limits.max_monster_id:
                    result.error(
                        f"Line {lineno}: Monster ID {monster_id} exceeds maximum allowed ID " +
                        f"{limits.max_monster_id} (from limits.txt M:R:{limits.max_monster_races})"
                    )

            in_monster = True
            first_s_in_monster = True
            # current monster name
            _ = line.split(":")[2] if len(line.split(":")) >= 3 else "unknown"
            continue

        # Other line types
        if line.startswith("W:"):
            validate_w_line(line, lineno, result)
        elif line.startswith("G:"):
            validate_g_line(line, lineno, result)
        elif line.startswith("I:"):
            validate_i_line(line, lineno, result)
        elif line.startswith("A:"):
            validate_a_line(line, lineno, result)
        elif line.startswith("P:"):
            validate_p_line(line, lineno, result)
        elif line.startswith("B:"):
            validate_b_line(line, lineno, result)
        elif line.startswith("S:"):
            if in_monster and first_s_in_monster:
                validate_s_line_is_first(line, lineno, result)
                first_s_in_monster = False
        elif line.startswith("F:"):
            pass  # F: lines are free-form flags, no strict validation needed
        elif line.startswith("D:"):
            pass  # D: lines are descriptions, no strict validation needed
        else:
            # Check for unknown line types (letter followed by colon)
            if len(line) >= 2 and line[1] == ":":
                result.error(f"Line {lineno}: Unknown line type '{line[0]}:' in line '{line}'")

    # Check for required version stamp
    if not has_version:
        result.error("Missing required version stamp (V: line)")

    # Check total monster count against limit
    if limits:
        monster_count = len(ids_seen)
        if monster_count > limits.max_monster_races:
            result.error(
                f"Total monster count ({monster_count}) exceeds maximum allowed " +
                f"({limits.max_monster_races}) from limits.txt M:R"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/monster.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                       # Validate the file
  %(prog)s --validate lib/edit/monster.txt  # Validate a specific file
  %(prog)s --export-json                    # Export to JSON (stdout)
  %(prog)s --export-json > monsters.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to monster.txt file (default: lib/edit/monster.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the monster.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all monster records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(monster_file: Path, limits_file: Path) -> int:
    """Run validation on the monster file."""
    print("=" * 60)
    print(f"Validating: {monster_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max monster races = {limits.max_monster_races} (max ID = {limits.max_monster_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_monster_file(monster_file, limits)

    # Print all messages of the validation result
    for msg in result.info:
        print(f"INFO: {msg}")

    for msg in result.warnings:
        print(f"WARNING: {msg}", file=sys.stderr)

    for msg in result.errors:
        print(f"ERROR: {msg}", file=sys.stderr)

    # Summary
    print("=" * 60)
    print(f"  Errors:   {len(result.errors)}")
    print(f"  Warnings: {len(result.warnings)}")
    print("=" * 60)

    if result.is_valid:
        print("OK")
    else:
        print("FAILED")

    return 0 if result.is_valid else 1


def run_export_json(monster_file: Path) -> int:
    """Export monsters to JSON on stdout."""
    monsters = parse_monsters(monster_file)

    if not monsters:
        print(f"ERROR: No monsters found in {monster_file}", file=sys.stderr)
        return 1

    print(export_monsters_to_json(monsters))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        monster_file = file_arg
    else:
        monster_file = project_dir / "lib" / "edit" / "monster.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(monster_file, limits_file)

    if export_json:
        return run_export_json(monster_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
