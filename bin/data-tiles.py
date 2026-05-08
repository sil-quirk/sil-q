#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# ///
"""
Provides tooling for the tile-assignment PRF files: lib/pref/graf-tiles.prf
and the sibling lib/pref/flvr-tiles.prf, which uses the same line shape with
an `L:` prefix.

1. Validates the format and integrity of both files (default behavior).
2. Exports the parsed tile assignments to JSON format (stdout).
3. Optional coverage report against lib/edit/monster.txt etc.

When invoked without a positional argument, both PRF files are validated.
A positional path overrides the default and validates only that file.

Data Validation
===============

PRF tile-assignment lines have one of these shapes:
- S:0xHH:0xRR/0xCC          # specials (visual effects); index is a hex byte
- F:N:0xRR/0xCC             # features (terrain); index is decimal
- K:N:0xRR/0xCC             # objects (item kinds); index is decimal
- R:N:0xRR/0xCC             # monster races / player races (decimal index)
- R:N:rage:0xRR/0xCC        # rage-specific alternative tile for a monster
- L:N:0xRR/0xCC             # flavored objects (flvr-tiles.prf only)

where:
- HH denotes a hex byte; valid range is 0x00..0xFF
- RR is the row byte of the tile coord (attr half of 0xRR/0xCC); 0x80..00xFF
- CC is the column byte of the tile coord (char half of 0xRR/0xCC); 0x80..00xFF

RR and CC: both bytes of the (attr, char) coord must be in 0x80..0xFF. The lower
7 bits encode the tile (row, column) offset within the tileset image.

Exit codes:
  0   - All validations passed (warnings allowed)
  N>0 - Total number of validation errors found across all files
        (clamped to 255 to fit a Unix exit code byte)
"""

import argparse
import json
import re
import signal
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast

# Handle broken pipe (e.g., when piping to head) - Unix only
if hasattr(signal, "SIGPIPE"):
    _ = signal.signal(signal.SIGPIPE, signal.SIG_DFL)

###
### Constants
###
TILE_SIZE = 16

# All recognized variant names for `R:N:<variant>:<coords>` lines.
# Anything outside this set is treated as an error.
KNOWN_VARIANTS = {"rage"}

# Player race IDs, reserved in `R:` lines (0..3 = Noldor/Sindar/Naugrim/Edain).
PLAYER_RACE_IDS = {0, 1, 2, 3}

# Hallucination IDs are real monster.txt entries that only display when the
# player is hallucinating. Lacking a tile assignment for them is intentional.
HALLUCINATION_ID_MIN = 300

###
### Regexes
###
# Tiles coordinates always look like `0xRR/0xCC`. Bytes 0x80..0xFF only.
COORD_RE = re.compile(r"^0x([0-9A-Fa-f]{1,2})/0x([0-9A-Fa-f]{1,2})$")
LINE_NORMAL_RE = re.compile(
    r"^([SFKRL]):(0x[0-9A-Fa-f]+|\d+):(0x[0-9A-Fa-f]+/0x[0-9A-Fa-f]+)$"
)
LINE_VARIANT_RE = re.compile(
    r"^R:(\d+):([A-Za-z][A-Za-z0-9_]*):(0x[0-9A-Fa-f]+/0x[0-9A-Fa-f]+)$"
)

# JSON-compatible types
type JsonValue = str | int | float | bool | None | list[JsonValue] | dict[str, JsonValue]
type JsonDict = dict[str, JsonValue]


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


@dataclass
class TileEntry:
    """One parsed line from a *-tiles.prf file."""

    prefix: str  # S, F, K, R, L
    id: int  # decimal id (or hex byte for S)
    variant: str | None  # "rage" or None for normal entry
    attr: int  # raw byte (0x80..0xFF)
    char: int  # raw byte (0x80..0xFF)
    lineno: int


@dataclass
class TilesetBounds:
    """Tile dimensions in cells parsed from the BMP header."""

    cols: int
    rows: int


def parse_bmp_bounds(bmp_path: Path) -> TilesetBounds | None:
    """Read BMP width/height and return tile-grid dimensions.

    Returns None if the file is missing or unreadable; the caller treats this
    as "skip the bounds check" rather than a hard failure.
    """
    if not bmp_path.exists():
        return None
    try:
        with bmp_path.open("rb") as f:
            header = f.read(26)
    except OSError:
        return None
    if len(header) < 26 or header[0:2] != b"BM":
        return None
    width = struct.unpack("<i", header[18:22])[0]
    height = struct.unpack("<i", header[22:26])[0]
    if width <= 0 or height <= 0:
        return None
    return TilesetBounds(cols=width // TILE_SIZE, rows=abs(height) // TILE_SIZE)


def parse_prf_file(filepath: Path, result: ValidationResult) -> list[TileEntry]:
    """Parse all tile-assignment lines from filepath, recording errors.

    Comments (# ...) and blank lines are skipped silently. Conditional lines
    (?:...) are accepted but ignored. They don't affect tile rendering and
    aren't currently used in graf-tiles.prf or flvr-tiles.prf.
    """
    entries: list[TileEntry] = []
    if not filepath.exists():
        result.error(f"File not found: {filepath}")
        return entries
    try:
        text = filepath.read_text(encoding="utf-8")
    except OSError as e:
        result.error(f"Could not read {filepath}: {e}")
        return entries

    for lineno, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("?:"):
            continue
        # `%:other-file.prf` is the include directive (engine recurses into the
        # other file). We don't follow includes, the validator runs per file.
        if line.startswith("%:"):
            continue

        # Variant form (e.g., rage): R:<id>:<variant>:<coords>
        if (m := LINE_VARIANT_RE.match(line)) is not None:
            id_str, variant_name, coords_str = m.group(1), m.group(2), m.group(3)
            attr, char = parse_coords(coords_str, lineno, result)
            if attr is None or char is None:
                continue
            if variant_name not in KNOWN_VARIANTS:
                result.error(
                    f"Line {lineno}: unknown variant '{variant_name}' in '{line}' "
                    f"(known: {', '.join(sorted(KNOWN_VARIANTS))}); engine will "
                    f"silently ignore this line"
                )
            # Still record the entry so we can report on it elsewhere.
            entries.append(
                TileEntry(
                    prefix="R",
                    id=int(id_str, 10),
                    variant=variant_name,
                    attr=attr,
                    char=char,
                    lineno=lineno,
                )
            )
            continue

        # Standard form (for normal tile assignments): <prefix>:<id>:<coords>
        if (m := LINE_NORMAL_RE.match(line)) is not None:
            prefix, id_str, coords_str = m.group(1), m.group(2), m.group(3)
            id_value = (
                int(id_str, 16)
                if id_str.lower().startswith("0x")
                else int(id_str, 10)
            )
            # S: takes a hex byte (0..0xFF), others take decimal.
            if prefix == "S" and not id_str.lower().startswith("0x"):
                result.error(
                    f"Line {lineno}: S: index '{id_str}' should be hex (0xHH) "
                    f"per file format conventions"
                )
            elif prefix != "S" and id_str.lower().startswith("0x"):
                result.error(
                    f"Line {lineno}: {prefix}: index '{id_str}' should be decimal "
                    f"per file format conventions"
                )
            attr, char = parse_coords(coords_str, lineno, result)
            if attr is None or char is None:
                continue
            entries.append(
                TileEntry(
                    prefix=prefix,
                    id=id_value,
                    variant=None,
                    attr=attr,
                    char=char,
                    lineno=lineno,
                )
            )
            continue

        result.error(f"Line {lineno}: unrecognized line shape: '{line}'")

    return entries


def parse_coords(
    coords: str, lineno: int, result: ValidationResult
) -> tuple[int | None, int | None]:
    """Parse `0xRR/0xCC` into (attr, char), validating each byte is 0x80..0xFF.

    Returns (None, None) on any error.
    """
    m = COORD_RE.match(coords)
    if m is None:
        result.error(f"Line {lineno}: malformed coords '{coords}'")
        return None, None
    attr = int(m.group(1), 16)
    char = int(m.group(2), 16)
    if not (0x80 <= attr <= 0xFF):
        result.error(
            f"Line {lineno}: attr {hex(attr)} out of range (must be 0x80..0xFF)"
        )
        return None, None
    if not (0x80 <= char <= 0xFF):
        result.error(
            f"Line {lineno}: char {hex(char)} out of range (must be 0x80..0xFF)"
        )
        return None, None
    return attr, char


def check_tileset_bounds(
    entries: list[TileEntry], bounds: TilesetBounds, result: ValidationResult
) -> None:
    """Each entry's tile (row, col) must fit inside the BMP's tile grid."""
    for e in entries:
        row = e.attr - 0x80
        col = e.char - 0x80
        if row >= bounds.rows or col >= bounds.cols:
            result.error(
                f"Line {e.lineno}: tile {hex(e.attr)}/{hex(e.char)} "
                f"(row {row}, col {col}) is outside tileset bounds "
                f"(rows {bounds.rows}, cols {bounds.cols})"
            )


def check_duplicate_ids(
    entries: list[TileEntry], result: ValidationResult
) -> None:
    """Same (prefix, id, variant) appearing twice with different coords."""
    seen: dict[tuple[str, int, str | None], TileEntry] = {}
    for e in entries:
        key = (e.prefix, e.id, e.variant)
        if (first := seen.get(key)) is not None:
            if first.attr != e.attr or first.char != e.char:
                variant_tag = f":{e.variant}" if e.variant else ""
                result.error(
                    f"Line {e.lineno}: duplicate {e.prefix}:{e.id}{variant_tag} "
                    f"with different coords {hex(e.attr)}/{hex(e.char)} "
                    f"(first set at line {first.lineno} to "
                    f"{hex(first.attr)}/{hex(first.char)})"
                )
        else:
            seen[key] = e


def check_rage_orphans_and_conflicts(
    entries: list[TileEntry], result: ValidationResult
) -> None:
    """Rage-specific consistency checks:
    - Orphan: rage line with no matching normal R: line for the same id.
    - Conflict: two monsters sharing a normal tile but different rage tiles.
    """
    normal_by_id: dict[int, TileEntry] = {}
    for e in entries:
        if e.prefix == "R" and e.variant is None:
            normal_by_id.setdefault(e.id, e)

    rage_by_normal_coord: dict[tuple[int, int], TileEntry] = {}
    for e in entries:
        if not (e.prefix == "R" and e.variant == "rage"):
            continue
        normal = normal_by_id.get(e.id)
        if normal is None:
            result.error(
                f"Line {e.lineno}: R:{e.id}:rage:... has no matching normal "
                f"R:{e.id} line; engine will silently ignore"
            )
            continue
        normal_key = (normal.attr, normal.char)
        if (existing := rage_by_normal_coord.get(normal_key)) is not None:
            if existing.attr != e.attr or existing.char != e.char:
                result.error(
                    f"Line {e.lineno}: rage substitute conflict at normal tile "
                    f"{hex(normal.attr)}/{hex(normal.char)}: existing rage "
                    f"target {hex(existing.attr)}/{hex(existing.char)} "
                    f"(set at line {existing.lineno}) kept; R:{e.id} would "
                    f"have used {hex(e.attr)}/{hex(e.char)}"
                )
        else:
            rage_by_normal_coord[normal_key] = e


def report_class_tile_coherence(
    entries: list[TileEntry], result: ValidationResult
) -> None:
    """List each rage target tile and the monsters that map to it."""
    by_target: dict[tuple[int, int], list[TileEntry]] = {}
    for e in entries:
        if e.prefix == "R" and e.variant == "rage":
            by_target.setdefault((e.attr, e.char), []).append(e)
    if not by_target:
        return
    for (a, c), members in sorted(by_target.items()):
        ids = sorted(m.id for m in members)
        result.log_info(
            f"rage tile {hex(a)}/{hex(c)} <- {len(members)} monsters: {ids}"
        )


###
### Cross-file referential integrity
###
EDIT_ID_RE = re.compile(r"^N:(\d+):")


def parse_edit_ids(filepath: Path) -> set[int]:
    """Extract the set of decimal ids from `N:<id>:` lines in an edit file."""
    if not filepath.exists():
        return set()
    ids: set[int] = set()
    try:
        text = filepath.read_text(encoding="utf-8")
    except OSError:
        return set()
    for line in text.splitlines():
        if (m := EDIT_ID_RE.match(line.strip())) is not None:
            ids.add(int(m.group(1), 10))
    return ids


def check_cross_file_integrity(
    entries: list[TileEntry],
    monster_ids: set[int],
    feature_ids: set[int],
    object_ids: set[int],
    flavor_ids: set[int],
    result: ValidationResult,
) -> None:
    """Each F/K/R/L line must reference an N: in the corresponding edit file.

    R:0..3 is reserved for player races and exempt from monster.txt lookup.
    Empty id sets indicate the corresponding edit file wasn't found, in which
    case the check is skipped silently.
    """
    for e in entries:
        if e.prefix == "R":
            if e.id in PLAYER_RACE_IDS:
                continue
            if monster_ids and e.id not in monster_ids:
                result.error(
                    f"Line {e.lineno}: R:{e.id} not found in monster.txt"
                )
        elif e.prefix == "F":
            if feature_ids and e.id not in feature_ids:
                result.error(
                    f"Line {e.lineno}: F:{e.id} not found in terrain.txt"
                )
        elif e.prefix == "K":
            if object_ids and e.id not in object_ids:
                result.error(
                    f"Line {e.lineno}: K:{e.id} not found in object.txt"
                )
        elif e.prefix == "L":
            if flavor_ids and e.id not in flavor_ids:
                result.error(
                    f"Line {e.lineno}: L:{e.id} not found in flavor.txt"
                )
        # S: is checked separately (range only).


def check_special_id_range(
    entries: list[TileEntry], result: ValidationResult
) -> None:
    """S: id is a 1-byte index into the misc_to_attr/misc_to_char arrays.

    Range 0x00..0xFF. Anything outside is rejected by the parser earlier, so
    this is mostly a sanity check in case the id parser logic is relaxed.
    """
    for e in entries:
        if e.prefix == "S" and not (0x00 <= e.id <= 0xFF):
            result.error(
                f"Line {e.lineno}: S:{hex(e.id)} out of range (must be 0x00..0xFF)"
            )


###
### Coverage report (--coverage)
###
def coverage_report(
    entries: list[TileEntry],
    monster_ids: set[int],
    result: ValidationResult,
) -> None:
    """Monsters lacking a normal tile or a rage substitute.

    Player races (R:0..3) and hallucination IDs (>=300) are excluded because
    their absence is intentional. Unique monsters are not excluded.
    """
    if not monster_ids:
        result.log_info("coverage: monster.txt not found, skipping report")
        return

    normals = {
        e.id for e in entries if e.prefix == "R" and e.variant is None
    }
    rages = {e.id for e in entries if e.prefix == "R" and e.variant == "rage"}

    real_ids = {
        i for i in monster_ids
        if i not in PLAYER_RACE_IDS and i < HALLUCINATION_ID_MIN
    }
    missing_normal = sorted(real_ids - normals)
    missing_rage = sorted(real_ids - rages)

    result.log_info(
        f"coverage: {len(real_ids)} real-range monsters in monster.txt"
    )
    result.log_info(
        f"coverage: {len(missing_normal)} lack a normal R: tile assignment"
        + (f": {missing_normal}" if missing_normal else "")
    )
    result.log_info(
        f"coverage: {len(missing_rage)} lack a R:N:rage substitute"
        + (f": {missing_rage[:50]}" + ("..." if len(missing_rage) > 50 else "")
            if missing_rage else "")
    )


###
### Validation entry point
###
def validate_prf_file(
    filepath: Path,
    monster_file: Path,
    terrain_file: Path,
    object_file: Path,
    flavor_file: Path,
    bmp_file: Path,
    coverage: bool,
) -> ValidationResult:
    result = ValidationResult()
    entries = parse_prf_file(filepath, result)
    if not entries:
        return result

    # Format checks
    check_special_id_range(entries, result)
    bounds = parse_bmp_bounds(bmp_file)
    if bounds is not None:
        check_tileset_bounds(entries, bounds, result)
    else:
        result.log_info(
            f"tileset bounds: skipped ({bmp_file} not parseable as BMP)"
        )
    check_duplicate_ids(entries, result)

    # Cross-file integrity checks
    monster_ids = parse_edit_ids(monster_file)
    feature_ids = parse_edit_ids(terrain_file)
    object_ids = parse_edit_ids(object_file)
    flavor_ids = parse_edit_ids(flavor_file)
    check_cross_file_integrity(
        entries, monster_ids, feature_ids, object_ids, flavor_ids, result
    )

    # Rage-specific checks
    check_rage_orphans_and_conflicts(entries, result)
    report_class_tile_coherence(entries, result)

    # Coverage report
    if coverage:
        coverage_report(entries, monster_ids, result)

    return result


###
### JSON export
###
def entries_to_json_list(entries: list[TileEntry]) -> list[JsonDict]:
    payload: list[JsonDict] = []
    for e in entries:
        payload.append(cast(JsonDict, {
            "prefix": e.prefix,
            "id": e.id,
            "variant": e.variant,
            "attr": e.attr,
            "char": e.char,
            "coord": f"0x{e.attr:02X}/0x{e.char:02X}",
            "lineno": e.lineno,
        }))
    return payload


###
### CLI
###
def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Work with lib/pref/graf-tiles.prf and flvr-tiles.prf data files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --validate                          # Validate both PRF files
  %(prog)s --validate lib/pref/flvr-tiles.prf  # Validate one specific file
  %(prog)s --validate --coverage               # Validate + monster coverage
  %(prog)s --export-json                       # Export both files to JSON
""",
    )
    _ = parser.add_argument(
        "file",
        nargs="?",
        type=Path,
        help="Path to a single PRF file. If omitted, both graf-tiles.prf and "
        "flvr-tiles.prf are processed.",
    )
    _ = parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate the PRF file format and integrity",
    )
    _ = parser.add_argument(
        "--export-json",
        action="store_true",
        help="Export parsed tile entries to JSON (stdout)",
    )
    _ = parser.add_argument(
        "--coverage",
        action="store_true",
        help="Include monster-coverage report (use with --validate)",
    )
    return parser.parse_args()


def run_validation_one(
    prf_file: Path,
    monster_file: Path,
    terrain_file: Path,
    object_file: Path,
    flavor_file: Path,
    bmp_file: Path,
    coverage: bool,
) -> int:
    """Validate a single PRF file. Returns count of errors."""
    print("=" * 60)
    print(f"Validating: {prf_file}")

    result = validate_prf_file(
        prf_file, monster_file, terrain_file, object_file, flavor_file,
        bmp_file, coverage,
    )

    for msg in result.info:
        print(f"INFO: {msg}")
    for msg in result.warnings:
        print(f"WARNING: {msg}", file=sys.stderr)
    for msg in result.errors:
        print(f"ERROR: {msg}", file=sys.stderr)

    print("=" * 60)
    print(f"  Errors:   {len(result.errors)}")
    print(f"  Warnings: {len(result.warnings)}")
    print("=" * 60)

    if result.is_valid:
        print("OK")
    else:
        print("FAILED")
    return len(result.errors)


def run_validation(
    prf_files: list[Path],
    monster_file: Path,
    terrain_file: Path,
    object_file: Path,
    flavor_file: Path,
    bmp_file: Path,
    coverage: bool,
) -> int:
    """Validate each file in turn. Exit code is the total error count across
    all validated files (clamped to 255 to fit a Unix exit code byte). Exit
    code 0 means no errors.
    """
    total_errors = 0
    for prf_file in prf_files:
        total_errors += run_validation_one(
            prf_file, monster_file, terrain_file, object_file, flavor_file,
            bmp_file, coverage,
        )
    return min(total_errors, 255)


def run_export_json(prf_files: list[Path]) -> int:
    """Export entries from each file as a JSON object keyed by filename."""
    result = ValidationResult()
    payload: dict[str, list[JsonDict]] = {}
    for prf_file in prf_files:
        entries = parse_prf_file(prf_file, result)
        payload[prf_file.name] = entries_to_json_list(entries)
    if result.errors:
        for msg in result.errors:
            print(f"ERROR: {msg}", file=sys.stderr)
        return 1
    print(json.dumps(payload, indent=2))
    return 0


def main() -> int:
    args = parse_args()
    file_arg = cast(Path | None, args.file)
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent

    if file_arg is not None:
        prf_files = [file_arg]
    else:
        prf_files = [
            project_dir / "lib" / "pref" / "graf-tiles.prf",
            project_dir / "lib" / "pref" / "flvr-tiles.prf",
        ]
    monster_file = project_dir / "lib" / "edit" / "monster.txt"
    terrain_file = project_dir / "lib" / "edit" / "terrain.txt"
    object_file = project_dir / "lib" / "edit" / "object.txt"
    flavor_file = project_dir / "lib" / "edit" / "flavor.txt"
    bmp_file = project_dir / "lib" / "xtra" / "graf" / "16x16.bmp"

    validate = cast(bool, args.validate)
    export_json = cast(bool, args.export_json)
    coverage = cast(bool, args.coverage)

    if validate:
        return run_validation(
            prf_files, monster_file, terrain_file, object_file, flavor_file,
            bmp_file, coverage,
        )
    if export_json:
        return run_export_json(prf_files)

    print("No action specified. Use --validate or --export-json.", file=sys.stderr)
    print("Run with --help for more information.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
