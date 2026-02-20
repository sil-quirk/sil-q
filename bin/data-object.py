#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the object.txt data file.

1. Validates the format and integrity of lib/edit/object.txt.
2. Exports the object records to JSON format (stdout).

Data Validation
===============

Based on the format specification in the file's comment header:
- N: serial number : & object name~
- G: symbol : color
- I: tval : sval : pval
- W: depth : rarity : weight : cost
- P: plus to-hit : damage dice : plus to-evasion : protection dice
- A: depth/rarity : depth/rarity : etc
- F: flag | flag | etc
- D: description

Exit codes:
  0 - All validations passed
  1 - One or more validation errors found


Documentation vs. Actual Data Discrepancies
===========================================

The following discrepancies were found between the documented format in the
comment header of object.txt and the actual data records in the file:

1. N: line format
   - Documented: "serial number : & object name~" implying & and ~ markers.
   - Actual: The & marker is optional. Out of 196 entries, only 119 include it.
     Examples without &: "Leather Armour~", "Studded Leather~", "Galvorn Armour~"
   - Actual: The ~ marker is also optional. 85 entries omit it entirely.
     Examples without ~: "Rage", "Last Chances", "Constitution", "Secrets"

2. P: line format
   - Documented: "plus to-hit : damage dice : plus to-evasion : protection dice"
     (4 values after P:)
   - Actual: 5 light source items have a 5th value (e.g., P:0:0d0:0:0d0:0).
     These items are: Lesser Jewel, Wooden Torch, Brass Lantern, Feanorian Lamp,
     Mallorn Torch. The 5th field is always 0; its purpose is unclear.

3. B: line (undocumented)
   - Not documented in the header comments at all.
   - Actual: B: lines appear with format "X/Y" representing ability references
     (skill_id/ability_id pairs). Found 3 occurrences:
       B:4/2 (ability 4/2 is Keen Senses, on "Amulet of the Vigilant Eye")
       B:4/4 (ability 4/4 is Alchemy, on "Ring of Secrets")
       B:2/0 (ability 2/0 is Dodging, on "Ring of Cowardice")

4. G: color codes
   - Documented: 16 colors listed: D,w,s,o,r,g,b,u,W,v,y,R,G,B,U plus 'd' for
     flavored items.
   - Actual: Extended colors with numeric suffixes are also used, which are not
     documented: D1, g1, s1, U1, v1, W1, y1 (shade/intensity variations?)
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
# d - flavored     W - Light Gray     v - Violet        y - Yellow
# R - Light Red    G - Light Green    B - Light Blue    U - Light Brown
#
# Note: The actual file also uses extended colors with numeric suffixes
# (e.g., s1, U1, D1) which are not documented in the comment section.
DOCUMENTED_COLORS = {"D", "w", "s", "o", "r", "g", "b", "u", "d", "W", "v", "y", "R", "G", "B", "U"}
EXTENDED_COLORS = {"D1", "g1", "s1", "U1", "v1", "W1", "y1"}
VALID_COLORS = DOCUMENTED_COLORS | EXTENDED_COLORS

###
### Regex patterns for validation
###
DICE_PATTERN = re.compile(r"^\d+d\d+$")
# Allocation format: depth/rarity pairs
ALLOCATION_PATTERN = re.compile(r"^\d+/\d+$")
# Ability format: skill_id/ability_id
ABILITY_PATTERN = re.compile(r"^\d+/\d+$")


@dataclass
class Limits:
    """Limits parsed from lib/edit/limits.txt."""

    max_object_kinds: int  # M:K value (note: limits.txt stores max+1)
    max_abilities: int  # M:B value

    @property
    def max_object_id(self) -> int:
        """Maximum valid object ID (0-indexed, so max_object_kinds - 1)."""
        return self.max_object_kinds - 1

    @property
    def max_ability_id(self) -> int:
        """Maximum valid ability ID."""
        return self.max_abilities - 1


@dataclass
class Allocation:
    """An allocation entry (depth/rarity pair)."""

    depth: int
    rarity: int


@dataclass
class AbilityRef:
    """An ability reference (B: line)."""

    skill_id: int
    ability_id: int


@dataclass
class ObjectKind:
    """An object record parsed from object.txt."""

    id: int
    name: str
    symbol: str | None = None
    color: str | None = None
    tval: int | None = None
    sval: int | None = None
    pval: int | None = None
    depth: int | None = None
    rarity: int | None = None
    weight: int | None = None
    cost: int | None = None
    attack_bonus: int | None = None
    damage_dice: str | None = None
    evasion_bonus: int | None = None
    protection_dice: str | None = None
    allocations: list[Allocation] = field(default_factory=list)
    abilities: list[AbilityRef] = field(default_factory=list)
    flags: list[str] = field(default_factory=list)
    description: str | None = None


def parse_limits_file(filepath: Path) -> Limits | None:
    """Parse limits.txt and extract relevant limits.

    Returns None if the file cannot be parsed.
    """
    if not filepath.exists():
        return None

    max_object_kinds = None
    max_abilities = None

    for line in filepath.read_text(encoding="latin-1").splitlines():
        line = line.strip()
        if line.startswith("M:K:"):
            # M:K:600 - Maximum number of object kinds
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_object_kinds = int(parts[2])
        elif line.startswith("M:B:"):
            # M:B:240 - Maximum number of abilities
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].isdigit():
                max_abilities = int(parts[2])

    if max_object_kinds is None or max_abilities is None:
        return None

    return Limits(max_object_kinds=max_object_kinds, max_abilities=max_abilities)


def validate_n_line(line: str, lineno: int, result: ValidationResult) -> int | None:
    """Validate N: line format and return the ID."""
    parts = line.split(":")
    if len(parts) < 3:
        result.error(f"Line {lineno}: N: line has {len(parts)} fields, expected at least 3: {line}")
        return None

    id_str = parts[1]
    if not id_str.isdigit():
        result.error(f"Line {lineno}: N: ID is not numeric: {id_str}")
        return None

    return int(id_str)


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
    """Validate I: line format (tval : sval : pval)."""
    parts = line.split(":")
    if len(parts) != 4:
        result.error(f"Line {lineno}: I: line has {len(parts)} fields, expected 4: {line}")
        return

    tval, sval, pval = parts[1], parts[2], parts[3]

    if not tval.isdigit():
        result.error(f"Line {lineno}: I: tval is not numeric: {tval}")

    if not sval.isdigit():
        result.error(f"Line {lineno}: I: sval is not numeric: {sval}")

    # pval can be signed
    if not re.match(r"^-?\d+$", pval):
        result.error(f"Line {lineno}: I: pval is not a valid integer: {pval}")


def validate_w_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate W: line format (depth : rarity : weight : cost)."""
    parts = line.split(":")
    if len(parts) != 5:
        result.error(f"Line {lineno}: W: line has {len(parts)} fields, expected 5: {line}")
        return

    depth, rarity, weight, cost = parts[1], parts[2], parts[3], parts[4]

    if not depth.isdigit():
        result.error(f"Line {lineno}: W: depth is not numeric: {depth}")
    if not rarity.isdigit():
        result.error(f"Line {lineno}: W: rarity is not numeric: {rarity}")
    if not weight.isdigit():
        result.error(f"Line {lineno}: W: weight is not numeric: {weight}")
    if not cost.isdigit():
        result.error(f"Line {lineno}: W: cost is not numeric: {cost}")


def validate_p_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate P: line format (attack_bonus : damage : evasion_bonus : protection).

    Note: Some entries have 5 fields instead of 4 (extra trailing field).
    """
    parts = line.split(":")
    # Allow 5 or 6 fields (P: + 4 or 5 values)
    if len(parts) < 5 or len(parts) > 6:
        result.error(f"Line {lineno}: P: line has {len(parts)} fields, expected 5-6: {line}")
        return

    attack_bonus, damage, evasion_bonus, protection = parts[1], parts[2], parts[3], parts[4]

    # attack_bonus can be signed
    if not re.match(r"^[+-]?\d+$", attack_bonus):
        result.error(f"Line {lineno}: P: attack_bonus is not a valid integer: {attack_bonus}")

    # damage should be dice format (NdM)
    if not DICE_PATTERN.match(damage):
        result.error(f"Line {lineno}: P: damage is not valid dice format (NdM): {damage}")

    # evasion_bonus can be signed
    if not re.match(r"^[+-]?\d+$", evasion_bonus):
        result.error(f"Line {lineno}: P: evasion_bonus is not a valid integer: {evasion_bonus}")

    # protection should be dice format
    if not DICE_PATTERN.match(protection):
        result.error(f"Line {lineno}: P: protection is not valid dice format (NdM): {protection}")


def validate_a_line(line: str, lineno: int, result: ValidationResult) -> None:
    """Validate A: line format (depth/rarity pairs)."""
    parts = line.split(":")
    if len(parts) < 2:
        result.error(f"Line {lineno}: A: line has no allocation data: {line}")
        return

    # Each part after A: should be depth/rarity
    for part in parts[1:]:
        if not ALLOCATION_PATTERN.match(part):
            result.error(f"Line {lineno}: A: invalid allocation format '{part}', expected depth/rarity")


def validate_b_line(line: str, lineno: int, result: ValidationResult, limits: Limits | None) -> None:
    """Validate B: line format (ability references)."""
    parts = line.split(":")
    if len(parts) != 2:
        result.error(f"Line {lineno}: B: line has {len(parts)} fields, expected 2: {line}")
        return

    ability_str = parts[1]
    if not ABILITY_PATTERN.match(ability_str):
        result.error(f"Line {lineno}: B: invalid ability format '{ability_str}', expected X/Y")
        return

    # Validate ability IDs are within limits
    if limits:
        # skill_id, ability_id
        _, ability_id = map(int, ability_str.split("/"))
        # We can't fully validate skill_id range without knowing all skills
        # but we can check ability_id
        if ability_id > limits.max_ability_id:
            result.error(
                f"Line {lineno}: B: ability_id {ability_id} exceeds max {limits.max_ability_id}"
            )


def parse_allocation(alloc_str: str) -> Allocation | None:
    """Parse an allocation string like "5/3" into an Allocation object."""
    if ALLOCATION_PATTERN.match(alloc_str):
        depth, rarity = map(int, alloc_str.split("/"))
        return Allocation(depth=depth, rarity=rarity)
    return None


def parse_ability(ability_str: str) -> AbilityRef | None:
    """Parse an ability string like "4/2" into an AbilityRef object."""
    if ABILITY_PATTERN.match(ability_str):
        skill_id, ability_id = map(int, ability_str.split("/"))
        return AbilityRef(skill_id=skill_id, ability_id=ability_id)
    return None


def parse_p_line(parts: list[str]) -> tuple[int | None, str | None, int | None, str | None]:
    """Parse P: line values and return attack_bonus, damage, evasion_bonus, protection."""
    if len(parts) < 5:
        return None, None, None, None

    attack_bonus = None
    damage_dice = None
    evasion_bonus = None
    protection_dice = None

    if re.match(r"^[+-]?\d+$", parts[1]):
        attack_bonus = int(parts[1])
    if DICE_PATTERN.match(parts[2]):
        damage_dice = parts[2]
    if re.match(r"^[+-]?\d+$", parts[3]):
        evasion_bonus = int(parts[3])
    if DICE_PATTERN.match(parts[4]):
        protection_dice = parts[4]

    return attack_bonus, damage_dice, evasion_bonus, protection_dice


def parse_objects(filepath: Path) -> list[ObjectKind]:
    """Parse object.txt and return a list of ObjectKind objects."""
    if not filepath.exists():
        return []

    lines = filepath.read_text(encoding="latin-1").splitlines()
    objects: list[ObjectKind] = []
    current_object: ObjectKind | None = None

    for line in lines:
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#") or line.startswith("V:"):
            continue

        # N: line - start of new object
        if line.startswith("N:"):
            # Save previous object
            if current_object is not None:
                objects.append(current_object)

            parts = line.split(":")
            if len(parts) >= 3 and parts[1].isdigit():
                # Join remaining parts for name (in case name contains colons)
                name = ":".join(parts[2:])
                current_object = ObjectKind(id=int(parts[1]), name=name)
            continue

        if current_object is None:
            continue

        # G: symbol : color
        if line.startswith("G:"):
            parts = line.split(":")
            if len(parts) >= 3:
                current_object.symbol = parts[1]
                current_object.color = parts[2]

        # I: tval : sval : pval
        elif line.startswith("I:"):
            parts = line.split(":")
            if len(parts) >= 4:
                if parts[1].isdigit():
                    current_object.tval = int(parts[1])
                if parts[2].isdigit():
                    current_object.sval = int(parts[2])
                if re.match(r"^-?\d+$", parts[3]):
                    current_object.pval = int(parts[3])

        # W: depth : rarity : weight : cost
        elif line.startswith("W:"):
            parts = line.split(":")
            if len(parts) >= 5:
                if parts[1].isdigit():
                    current_object.depth = int(parts[1])
                if parts[2].isdigit():
                    current_object.rarity = int(parts[2])
                if parts[3].isdigit():
                    current_object.weight = int(parts[3])
                if parts[4].isdigit():
                    current_object.cost = int(parts[4])

        # P: attack_bonus : damage : evasion_bonus : protection
        elif line.startswith("P:"):
            parts = line.split(":")
            attack, damage, evasion, protection = parse_p_line(parts)
            current_object.attack_bonus = attack
            current_object.damage_dice = damage
            current_object.evasion_bonus = evasion
            current_object.protection_dice = protection

        # A: allocation pairs
        elif line.startswith("A:"):
            parts = line.split(":")
            for part in parts[1:]:
                alloc = parse_allocation(part)
                if alloc:
                    current_object.allocations.append(alloc)

        # B: ability reference
        elif line.startswith("B:"):
            parts = line.split(":")
            if len(parts) >= 2:
                ability = parse_ability(parts[1])
                if ability:
                    current_object.abilities.append(ability)

        # F: flags
        elif line.startswith("F:"):
            content = line[2:]  # Remove "F:"
            flags = [f.strip() for f in content.split("|") if f.strip()]
            current_object.flags.extend(flags)

        # D: description
        elif line.startswith("D:"):
            content = line[2:].strip()  # Remove "D:" and strip whitespace
            if current_object.description is None:
                current_object.description = content
            else:
                current_object.description += " " + content

    if current_object is not None:
        objects.append(current_object)

    return objects


def export_objects_to_json(objects: list[ObjectKind]) -> str:
    """Export objects to a JSON string."""

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

    def object_to_dict(obj: ObjectKind) -> JsonDict:
        """Convert an ObjectKind to a dict, removing None values."""
        d = clean_dict(asdict(obj))
        if "flags" in d and isinstance(d["flags"], list):
            d["flags"] = cast(JsonValue, sorted(cast(list[str], d["flags"])))
        return d

    data = {"objects": [object_to_dict(o) for o in objects]}
    return json.dumps(data, indent=2, ensure_ascii=False)


def validate_object_file(filepath: Path, limits: Limits | None = None) -> ValidationResult:
    """Validate the entire object.txt file.

    Args:
        filepath: Path to object.txt file.
        limits: Optional limits from limits.txt. If provided, validates
                object count and IDs against the maximum allowed.
    """
    result = ValidationResult()

    if not filepath.exists():
        result.error(f"Object file not found: {filepath}")
        return result

    lines = filepath.read_text(encoding="latin-1").splitlines()

    # Track state. Maps id -> line number.
    ids_seen: dict[int, int] = {}

    prev_id = -1
    has_version = False

    for lineno, line in enumerate(lines, start=1):
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#"):
            continue

        # Version stamp
        if line.startswith("V:"):
            has_version = True
            continue

        # N: line - start of object entry
        if line.startswith("N:"):
            object_id = validate_n_line(line, lineno, result)
            if object_id is not None:
                # Check for duplicates
                if object_id in ids_seen:
                    result.error(
                        f"Line {lineno}: Duplicate ID {object_id} " +
                        f"(first seen at line {ids_seen[object_id]})"
                    )
                else:
                    ids_seen[object_id] = lineno

                # Check IDs are increasing
                if object_id <= prev_id:
                    result.error(
                        f"Line {lineno}: ID {object_id} is not greater than previous ID {prev_id} " +
                        "(IDs must be strictly increasing)"
                    )
                prev_id = object_id

                # Check ID against limit
                if limits and object_id > limits.max_object_id:
                    result.error(
                        f"Line {lineno}: Object ID {object_id} exceeds maximum allowed ID " +
                        f"{limits.max_object_id} (from limits.txt M:K:{limits.max_object_kinds})"
                    )
            continue

        # Other line types
        if line.startswith("G:"):
            validate_g_line(line, lineno, result)
        elif line.startswith("I:"):
            validate_i_line(line, lineno, result)
        elif line.startswith("W:"):
            validate_w_line(line, lineno, result)
        elif line.startswith("P:"):
            validate_p_line(line, lineno, result)
        elif line.startswith("A:"):
            validate_a_line(line, lineno, result)
        elif line.startswith("B:"):
            validate_b_line(line, lineno, result, limits)
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

    # Check total object count against limit
    if limits:
        object_count = len(ids_seen)
        if object_count > limits.max_object_kinds:
            result.error(
                f"Total object count ({object_count}) exceeds maximum allowed " +
                f"({limits.max_object_kinds}) from limits.txt M:K"
            )

    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/edit/object.txt data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                      # Validate the file
  %(prog)s --validate lib/edit/object.txt  # Validate a specific file
  %(prog)s --export-json                   # Export to JSON (stdout)
  %(prog)s --export-json > objects.json    # Export to JSON file
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to object.txt file (default: lib/edit/object.txt)",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the object.txt file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export all object records to JSON (stdout)",
    )
    return parser.parse_args()


def run_validation(object_file: Path, limits_file: Path) -> int:
    """Run validation on the object file."""
    print("=" * 60)
    print(f"Validating: {object_file}")

    # Parse limits file
    limits = parse_limits_file(limits_file)
    if limits:
        print(f"Limits: max object kinds = {limits.max_object_kinds} (max ID = {limits.max_object_id})")
    else:
        print(f"WARNING: Could not parse limits from {limits_file}", file=sys.stderr)

    # Validate the file
    result = validate_object_file(object_file, limits)

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


def run_export_json(object_file: Path) -> int:
    """Export objects to JSON on stdout."""
    objects = parse_objects(object_file)

    if not objects:
        print(f"ERROR: No objects found in {object_file}", file=sys.stderr)
        return 1

    print(export_objects_to_json(objects))
    return 0


def main() -> int:
    args = parse_args()

    # Determine file paths
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg:
        object_file = file_arg
    else:
        object_file = project_dir / "lib" / "edit" / "object.txt"

    limits_file = project_dir / "lib" / "edit" / "limits.txt"
    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)

    if validate:
        return run_validation(object_file, limits_file)

    if export_json:
        return run_export_json(object_file)

    # No action specified
    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
