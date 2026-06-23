#!/usr/bin/env python3
"""
Vault visualization tool for Sil-QH.

Highlights:
- Parses `lib/edit/vault.txt` and `lib/edit/style.txt`
- Renders vault layouts to PNG using the MicroChasm tileset (`lib/xtra/graf/16x16.png`)
- Quickly compare the same vault under multiple visual styles

Examples:
  py tools/vaultviz.py list --filter Dragon
  py tools/vaultviz.py styles --filter Grey
  py tools/vaultviz.py show --vault 220
  py tools/vaultviz.py render --vault 220 --styles 0-8 --scale 3
  py tools/vaultviz.py render --vault \"Dragon Lair\" --styles vault --out-dir scripts/output/vaultviz
  py tools/vaultviz.py sheet --vault 220 --styles vault --cols 4 --out scripts/output/vaultviz/dragon_sheet.png
  py tools/vaultviz.py sheet --vault 180 --styles global --cols 5

PNG output requires Pillow:
  py -m pip install pillow
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional

try:
    from PIL import Image, ImageDraw, ImageFont

    PIL_AVAILABLE = True
except Exception:
    Image = None  # type: ignore[assignment]
    ImageDraw = None  # type: ignore[assignment]
    ImageFont = None  # type: ignore[assignment]
    PIL_AVAILABLE = False


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_VAULT_TXT = REPO_ROOT / "lib" / "edit" / "vault.txt"
DEFAULT_STYLE_TXT = REPO_ROOT / "lib" / "edit" / "style.txt"
DEFAULT_STYLE_LEVELS_TXT = REPO_ROOT / "lib" / "edit" / "style-levels.txt"
DEFAULT_TILESET = REPO_ROOT / "lib" / "xtra" / "graf" / "16x16.png"
DEFAULT_GRAF_PRF = ""
DEFAULT_OUT_DIR = REPO_ROOT / "scripts" / "output" / "vaultviz"


_BASE_TERRAIN_SYMBOLS = {" ", ".", "#", "$", "%", "+", "s", ":", ",", ";", "<", ">", "0", "7"}

# Vault symbol to tile coordinate mapping (row, col) for pictogram rendering
# Coordinates were derived from the old graphics preference data.
VAULT_SYMBOL_TILES: dict[str, tuple[int, int]] = {
    # Monsters (from the old graphics preference data)
    "o": (7, 30),   # Orc champion (R:81)
    "O": (7, 31),   # Orc captain (R:91)
    "f": (9, 21),   # Cat warrior (R:154)
    "F": (9, 22),   # Cat assassin (R:175)
    "t": (9, 13),   # Mountain troll (R:74)
    "T": (9, 15),   # Troll guard (R:192)
    "W": (7, 13),   # Barrow wight (R:112)
    "z": (8, 1),    # Dejected human thrall (R:13)
    "Z": (8, 0),    # Orc thrallmaster (R:15)
    "@": (9, 8),    # Easterling spy (R:121)
    "H": (6, 24),   # Silent watcher (R:203)
    "c": (8, 18),   # Wolf/White wolf (R:52)
    "v": (7, 15),   # Lesser vampire (R:174)
    "r": (10, 4),   # Sulrauko (R:123) - representative raukar
    "M": (7, 2),    # Brood spider (R:44) - representative spider
    "d": (9, 30),   # Great cold-drake (R:202) - representative dragon
    "y": (9, 29),   # Young cold-drake (R:164)
    "Y": (9, 27),   # Young fire-drake (R:181)
    "a": (7, 26),   # Orc archer (R:51)
    "b": (7, 14),   # Twisted bat (R:104)

    # Unique monsters
    "A": (7, 19),   # Aldor, the Risen King (R:117)
    "R": (10, 16),  # Gothmog, high Captain of Balrogs (R:241)
    "U": (7, 0),    # Ungoliant, the Gloomweaver (R:242)
    "D": (10, 3),   # Glaurung, the Deceiver (R:243)
    "G": (10, 17),  # Gorthaur, Servant of Morgoth (R:244)
    "V": (10, 18),  # Morgoth, Lord of Darkness (R:251)
    "C": (8, 23),   # Carcharoth, the Jaws of Thirst (R:253)
    "N": (10, 19),  # Mandos the Doomsman (R:20)
    "P": (11, 8),   # Valar Projection - Nienna tile (R:6)

    # Additional undocumented unique monsters
    "K": (10, 2),   # Ancalagon the Black (R:233)
    "B": (10, 10),  # Duruin, Least of the Balrogs (R:126)
    "L": (10, 19),  # Aule the Smith (R:19)
    "q": (7, 21),   # Whispering shadow (R:101)
    "j": (7, 5),    # Shadow spider (R:132)
    "k": (8, 12),   # Lurking horror (R:113)
    "n": (6, 31),   # Nightthorn (R:72)
    "g": (7, 10),   # Wraith - wight/wraith (R:183)

    # Treasures - use small chests (will have 'T' overlay)
    "*": (1, 0),    # Basic treasure -> Small wooden chest (K:372)
    "&": (1, 2),    # Good treasure -> Small steel chest (K:373)
    "!": (1, 4),    # Great treasure -> Small jewelled chest (K:374)

    # Skeletons
    "S": (3, 30),   # Elf skeleton (K:42)
    "h": (3, 29),   # Human skeleton (K:41)
    "e": (3, 28),   # Orc skeleton (K:40)

    # Features
    "^": (0, 17),   # Trap - pit (F:17)
    "w": (0, 26),   # Web (F:26)

    # Generic monster levels (use orc progression as representatives)
    "1": (7, 27),   # Monster +1 -> Orc skirmisher (R:21)
    "2": (7, 29),   # Monster +2 -> Orc soldier (R:41)
    "3": (7, 28),   # Monster +3 -> Orc warrior (R:61)
    "4": (7, 30),   # Monster +4 -> Orc champion (R:81)
    "?": (7, 25),   # Monster/item +1 -> Orc scout (R:31)
}

# Chest tiles by vault type (for '~' symbol)
VAULT_CHEST_TILES: dict[int, tuple[int, int]] = {
    6: (1, 1),   # Type 6 (interesting room) -> Large wooden chest (K:375)
    7: (1, 3),   # Type 7 (lesser vault) -> Large steel chest (K:376)
    8: (1, 5),   # Type 8 (greater vault) -> Large jewelled chest (K:377)
    9: (1, 5),   # Type 9 (Morgoth's vault) -> Large jewelled chest
    10: (1, 5),  # Type 10 (Gates) -> Large jewelled chest
}

# Symbols that get a 'T' overlay to distinguish from actual chests
_TREASURE_SYMBOLS = {"*", "&", "!"}


@dataclass(frozen=True)
class Vault:
    serial: int
    name: str
    typ: Optional[int] = None
    depth: Optional[int] = None
    rarity: Optional[int] = None
    color: int = 0
    flags: tuple[str, ...] = ()
    style_tokens: tuple[tuple[str, int], ...] = ()
    lines: tuple[str, ...] = ()

    @property
    def height(self) -> int:
        return len(self.lines)

    @property
    def width(self) -> int:
        if not self.lines:
            return 0
        return len(self.lines[0])


@dataclass(frozen=True)
class Style:
    idx: int
    name: str
    wall: Optional[tuple[int, int]] = None
    vein: Optional[tuple[int, int]] = None
    floor_variants: tuple[tuple[int, int], ...] = ()
    door_variants: tuple[tuple[int, int], ...] = ()


@dataclass(frozen=True)
class StyleSystem:
    styles: dict[int, Style]
    default_vein_overlay: Optional[tuple[int, int]] = None
    overlay_key_rgb: Optional[tuple[int, int, int]] = None


def _read_lines_keep_trailing_spaces(path: Path) -> list[str]:
    # We must preserve trailing spaces for rectangular vaults (D: lines).
    lines: list[str] = []
    with path.open("r", encoding="utf-8", newline="") as file_handle:
        for raw in file_handle:
            line = raw.rstrip("\n")
            if line.endswith("\r"):
                line = line[:-1]
            lines.append(line)
    return lines


def parse_vault_txt(path: Path) -> list[Vault]:
    current_serial: Optional[int] = None
    current_name: Optional[str] = None
    current_typ: Optional[int] = None
    current_depth: Optional[int] = None
    current_rarity: Optional[int] = None
    current_color: int = 0
    current_flags: list[str] = []
    current_style_tokens: list[tuple[str, int]] = []
    current_lines: list[str] = []

    def flush_current() -> Optional[Vault]:
        nonlocal current_serial, current_name, current_typ, current_depth, current_rarity
        nonlocal current_color, current_flags, current_style_tokens, current_lines

        if current_serial is None or current_name is None:
            return None

        if current_lines:
            expected_width = len(current_lines[0])
            for row in current_lines:
                if len(row) != expected_width:
                    raise ValueError(
                        f"{path}: vault {current_serial} '{current_name}' is not rectangular "
                        f"(expected width {expected_width}, got {len(row)})"
                    )

        vault = Vault(
            serial=current_serial,
            name=current_name,
            typ=current_typ,
            depth=current_depth,
            rarity=current_rarity,
            color=current_color,
            flags=tuple(current_flags),
            style_tokens=tuple(current_style_tokens),
            lines=tuple(current_lines),
        )

        current_serial = None
        current_name = None
        current_typ = None
        current_depth = None
        current_rarity = None
        current_color = 0
        current_flags = []
        current_style_tokens = []
        current_lines = []
        return vault

    vaults: list[Vault] = []

    for line in _read_lines_keep_trailing_spaces(path):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("V:"):
            continue

        if line.startswith("N:"):
            maybe_vault = flush_current()
            if maybe_vault is not None:
                vaults.append(maybe_vault)

            parts = line.split(":", 2)
            if len(parts) < 3:
                raise ValueError(f"{path}: malformed N: line: {line!r}")
            current_serial = int(parts[1])
            current_name = parts[2]
            continue

        if current_serial is None:
            continue

        if line.startswith("X:"):
            # Sil's parser reads only type/depth/rarity; extra :rows:cols are ignored in-game.
            parts = line[2:].split(":")
            if len(parts) < 3:
                raise ValueError(f"{path}: malformed X: line: {line!r}")
            current_typ = int(parts[0])
            current_depth = int(parts[1])
            current_rarity = int(parts[2])
            continue

        if line.startswith("C:"):
            current_color = int(line[2:].strip())
            continue

        if line.startswith("S:"):
            tokens = line[2:].strip().split()
            for token in tokens:
                if ":" not in token:
                    raise ValueError(f"{path}: malformed S: token {token!r} in line {line!r}")
                style_token, weight_text = token.split(":", 1)
                weight = int(weight_text)
                if weight <= 0:
                    continue
                current_style_tokens.append((style_token, weight))
            continue

        if line.startswith("F:"):
            token_str = line[2:]
            token_str = token_str.replace("|", " ")
            for token in token_str.split():
                current_flags.append(token.strip())
            continue

        if line.startswith("D:"):
            current_lines.append(line[2:])
            continue

    maybe_vault = flush_current()
    if maybe_vault is not None:
        vaults.append(maybe_vault)

    return vaults


def parse_style_txt(path: Path) -> StyleSystem:
    styles: dict[int, Style] = {}
    current_idx: Optional[int] = None
    current_name: Optional[str] = None
    wall: Optional[tuple[int, int]] = None
    vein: Optional[tuple[int, int]] = None
    floor_variants: list[tuple[int, int]] = []
    door_variants: list[tuple[int, int]] = []

    default_vein_overlay: Optional[tuple[int, int]] = None
    overlay_key_rgb: Optional[tuple[int, int, int]] = None

    def flush_current() -> None:
        nonlocal current_idx, current_name, wall, vein, floor_variants, door_variants
        if current_idx is None or current_name is None:
            return
        styles[current_idx] = Style(
            idx=current_idx,
            name=current_name,
            wall=wall,
            vein=vein,
            floor_variants=tuple(floor_variants),
            door_variants=tuple(door_variants),
        )
        current_idx = None
        current_name = None
        wall = None
        vein = None
        floor_variants = []
        door_variants = []

    for raw in _read_lines_keep_trailing_spaces(path):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("V:"):
            continue

        # style.txt supports inline comments for non-layout lines
        line = stripped.split("#", 1)[0].strip()
        if not line:
            continue

        if line.startswith("E:"):
            # E:row:col
            parts = line.split(":")
            if len(parts) < 3:
                raise ValueError(f"{path}: malformed E: line: {raw!r}")
            default_vein_overlay = (int(parts[1]), int(parts[2]))
            continue

        if line.startswith("EK:"):
            # EK:R:G:B
            parts = line.split(":")
            if len(parts) < 4:
                raise ValueError(f"{path}: malformed EK: line: {raw!r}")
            overlay_key_rgb = (int(parts[1]), int(parts[2]), int(parts[3]))
            continue

        if line.startswith("N:"):
            flush_current()
            parts = line.split(":", 2)
            if len(parts) < 3:
                raise ValueError(f"{path}: malformed N: line: {raw!r}")
            current_idx = int(parts[1])
            current_name = parts[2].strip()
            continue

        if current_idx is None:
            continue

        if line.startswith("W:"):
            parts = line.split(":")
            if len(parts) < 3:
                raise ValueError(f"{path}: malformed W: line: {raw!r}")
            wall = (int(parts[1]), int(parts[2]))
            continue

        if line.startswith("Y:"):
            parts = line.split(":")
            if len(parts) < 3:
                raise ValueError(f"{path}: malformed Y: line: {raw!r}")
            vein = (int(parts[1]), int(parts[2]))
            continue

        if line.startswith("F:"):
            tokens = line[2:].replace(",", " ").split()
            for token in tokens:
                token = token.strip().strip(":").strip(",")
                if not token or ":" not in token:
                    continue
                row_text, col_text = token.split(":", 1)
                floor_variants.append((int(row_text), int(col_text)))
            continue

        if line.startswith("D:"):
            tokens = line[2:].replace(",", " ").split()
            for token in tokens:
                token = token.strip().strip(":").strip(",")
                if not token or ":" not in token:
                    continue
                row_text, col_text = token.split(":", 1)
                door_variants.append((int(row_text), int(col_text)))
            continue

    flush_current()
    return StyleSystem(styles=styles, default_vein_overlay=default_vein_overlay, overlay_key_rgb=overlay_key_rgb)


def parse_graf_feature_tiles(path: Path) -> dict[int, tuple[int, int]]:
    tiles: dict[int, tuple[int, int]] = {}
    for raw in _read_lines_keep_trailing_spaces(path):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        line = stripped.split("#", 1)[0].strip()
        if not line.startswith("F:"):
            continue
        parts = line.split(":")
        if len(parts) < 3:
            continue
        feature_id = int(parts[1])
        tile_spec = parts[2].strip()
        if "/" not in tile_spec:
            continue
        attr_hex, char_hex = tile_spec.split("/", 1)
        attr = int(attr_hex, 16)
        char = int(char_hex, 16)
        tiles[feature_id] = (attr - 0x80, char - 0x80)
    return tiles


def _slugify(text: str, max_len: int = 80) -> str:
    slug = text.strip().lower()
    slug = re.sub(r"[^a-z0-9]+", "_", slug)
    slug = slug.strip("_")
    if not slug:
        slug = "vault"
    return slug[:max_len]


def _dedupe_keep_order(items: Iterable[int]) -> list[int]:
    seen: set[int] = set()
    out: list[int] = []
    for item in items:
        if item in seen:
            continue
        seen.add(item)
        out.append(item)
    return out


def _parse_int_ranges(spec: str) -> list[int]:
    result: list[int] = []
    for part in re.split(r"[,\s]+", spec.strip()):
        if not part:
            continue
        range_match = re.fullmatch(r"(\d+)\s*-\s*(\d+)", part)
        if range_match:
            start = int(range_match.group(1))
            end = int(range_match.group(2))
            step = 1 if end >= start else -1
            result.extend(list(range(start, end + step, step)))
            continue
        if part.isdigit():
            result.append(int(part))
            continue
        raise ValueError(f"Not a numeric style selector: {part!r}")
    return result


def parse_style_levels_txt(path: Path) -> dict[int, list[tuple[int, int]]]:
    depth_to_weights: dict[int, list[tuple[int, int]]] = {}
    for raw in _read_lines_keep_trailing_spaces(path):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("V:"):
            continue
        line = stripped.split("#", 1)[0].strip()
        if not line.startswith("L:"):
            continue

        # Supports:
        # - L:<depth>: <sidx:weight>...
        # - L:<min>:<max>: <sidx:weight>...
        range_match = re.match(r"^L:(\d+):(\d+):\s*(.*)$", line)
        single_match = re.match(r"^L:(\d+):\s*(.*)$", line)
        if range_match:
            depth_min = int(range_match.group(1))
            depth_max = int(range_match.group(2))
            rest = range_match.group(3)
        elif single_match:
            depth_min = depth_max = int(single_match.group(1))
            rest = single_match.group(2)
        else:
            continue

        tokens = rest.replace(",", " ").split()
        pairs: list[tuple[int, int]] = []
        for token in tokens:
            if ":" not in token:
                continue
            sidx_text, weight_text = token.split(":", 1)
            if not sidx_text.isdigit():
                continue
            weight = int(weight_text)
            if weight <= 0:
                continue
            pairs.append((int(sidx_text), weight))

        for depth in range(depth_min, depth_max + 1):
            depth_to_weights[depth] = pairs

    return depth_to_weights


def _styles_from_vault_tokens(
    vault: Vault,
    *,
    all_style_ids: list[int],
    level_style_pool: Optional[list[int]],
) -> list[int]:
    resolved: list[int] = []
    for style_token, _weight in vault.style_tokens:
        if style_token.isdigit():
            resolved.append(int(style_token))
        elif style_token in {"*", "$"}:
            if level_style_pool:
                resolved.extend(level_style_pool)
            else:
                resolved.extend(all_style_ids)
        else:
            continue
    return _dedupe_keep_order(resolved)


def _select_styles(
    selectors: list[str],
    *,
    vault: Optional[Vault],
    styles: dict[int, Style],
    level_rules: dict[int, list[tuple[int, int]]],
    depth_hint: Optional[int],
) -> list[int]:
    all_style_ids = sorted(styles.keys())
    selected: list[int] = []

    resolved_depth = depth_hint
    if resolved_depth is None and vault is not None:
        resolved_depth = vault.depth

    level_style_pool: Optional[list[int]] = None
    if resolved_depth is not None and resolved_depth in level_rules:
        level_style_pool = [sidx for sidx, _w in level_rules[resolved_depth]]

    expanded_selectors: list[str] = []
    for selector in selectors:
        expanded_selectors.extend([s for s in selector.split(",") if s.strip()])

    if not expanded_selectors:
        expanded_selectors = ["auto"]

    for selector in expanded_selectors:
        selector = selector.strip()
        if not selector:
            continue
        selector_lower = selector.lower()

        if selector_lower == "global":
            selected.extend(all_style_ids)
            continue

        if selector_lower in {"all", "*"}:
            # If a specific vault is selected and it has S: style tokens, treat
            # "all" as "all styles that apply to this vault" instead of the
            # global set. This makes it easy to render a vault across its
            # applicable styles with the convenient "--styles all" flag.
            if vault is not None and vault.style_tokens:
                selected.extend(_styles_from_vault_tokens(vault, all_style_ids=all_style_ids, level_style_pool=level_style_pool))
            else:
                selected.extend(all_style_ids)
            continue

        if selector_lower == "level":
            if level_style_pool is None:
                raise ValueError("Style selector 'level' needs a resolvable depth (use --depth or a vault with X:depth)")
            selected.extend(level_style_pool)
            continue

        if selector_lower == "vault":
            if vault is None:
                raise ValueError("Style selector 'vault' requires a vault selection")
            selected.extend(_styles_from_vault_tokens(vault, all_style_ids=all_style_ids, level_style_pool=level_style_pool))
            continue

        if selector_lower == "auto":
            if vault is not None and vault.style_tokens:
                selected.extend(_styles_from_vault_tokens(vault, all_style_ids=all_style_ids, level_style_pool=level_style_pool))
            elif level_style_pool is not None:
                selected.extend(level_style_pool)
            else:
                selected.append(0)
            continue

        # Numeric selectors: "0-5 10"
        try:
            selected.extend(_parse_int_ranges(selector))
            continue
        except ValueError:
            pass

        # Name substring match
        name_matches = [sid for sid, style in styles.items() if selector_lower in style.name.lower()]
        if not name_matches:
            raise ValueError(f"No styles matched selector: {selector!r}")
        selected.extend(sorted(name_matches))

    selected = _dedupe_keep_order(selected)
    selected = [sid for sid in selected if sid in styles]
    return selected


def _find_vaults(vaults: list[Vault], selectors: list[str], regex: bool) -> list[Vault]:
    if not selectors:
        raise ValueError("At least one --vault is required")

    matches: list[Vault] = []
    for selector in selectors:
        selector = selector.strip()
        if not selector:
            continue
        if selector.isdigit():
            serial = int(selector)
            vault = next((v for v in vaults if v.serial == serial), None)
            if vault is None:
                raise ValueError(f"No vault with serial {serial}")
            matches.append(vault)
            continue

        if regex:
            pattern = re.compile(selector, re.IGNORECASE)
            found = [v for v in vaults if pattern.search(v.name)]
        else:
            needle = selector.lower()
            found = [v for v in vaults if needle in v.name.lower()]

        if not found:
            raise ValueError(f"No vault matched: {selector!r}")
        matches.extend(found)

    matches = sorted({v.serial: v for v in matches}.values(), key=lambda v: v.serial)
    return matches


def _require_pillow() -> None:
    if PIL_AVAILABLE:
        return
    raise RuntimeError("PNG rendering requires Pillow. Install it with: py -m pip install pillow (or: python -m pip install pillow)")


class TileAtlas:
    def __init__(self, tileset_path: Path, tile_size: int) -> None:
        _require_pillow()
        assert Image is not None
        self.tileset_path = tileset_path
        self.tile_size = tile_size
        self.sheet = Image.open(tileset_path).convert("RGBA")
        self._cache: dict[tuple[int, int, int], "Image.Image"] = {}

    def get(self, row: int, col: int, scale: int) -> "Image.Image":
        key = (row, col, scale)
        cached = self._cache.get(key)
        if cached is not None:
            return cached
        x0 = col * self.tile_size
        y0 = row * self.tile_size
        tile = self.sheet.crop((x0, y0, x0 + self.tile_size, y0 + self.tile_size))
        if scale != 1:
            tile = tile.resize((self.tile_size * scale, self.tile_size * scale), resample=Image.NEAREST)
        self._cache[key] = tile
        return tile


def _tile_with_color_key(tile: "Image.Image", key_rgb: tuple[int, int, int]) -> "Image.Image":
    # Convert "colorkey" transparency into alpha.
    assert Image is not None
    if tile.mode != "RGBA":
        tile = tile.convert("RGBA")
    r_key, g_key, b_key = key_rgb
    pixels = tile.load()
    if pixels is None:
        return tile
    width, height = tile.size
    out = tile.copy()
    out_px = out.load()
    assert out_px is not None
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if (r, g, b) == (r_key, g_key, b_key):
                out_px[x, y] = (r, g, b, 0)
            else:
                out_px[x, y] = (r, g, b, a)
    return out


def _load_font(font_path: Optional[Path], pixel_size: int) -> "ImageFont.ImageFont":
    assert ImageFont is not None
    if font_path is not None:
        return ImageFont.truetype(str(font_path), size=pixel_size)

    candidate_paths = [
        Path(r"C:\Windows\Fonts\consola.ttf"),
        Path(r"C:\Windows\Fonts\lucon.ttf"),
    ]
    for candidate in candidate_paths:
        if candidate.exists():
            try:
                return ImageFont.truetype(str(candidate), size=pixel_size)
            except Exception:
                continue

    # Portable fallback; not scalable, but always available.
    return ImageFont.load_default()


def _draw_text_centered(
    draw: "ImageDraw.ImageDraw",
    xy: tuple[int, int],
    text: str,
    font: "ImageFont.ImageFont",
    fill: tuple[int, int, int, int],
    outline: Optional[tuple[int, int, int, int]] = (0, 0, 0, 200),
) -> None:
    x, y = xy
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    tx = x - text_w // 2
    ty = y - text_h // 2
    if outline is not None:
        for ox, oy in [(-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, -1), (-1, 1), (1, 1)]:
            draw.text((tx + ox, ty + oy), text, font=font, fill=outline)
    draw.text((tx, ty), text, font=font, fill=fill)


def render_vault_png(
    vault: Vault,
    style: Style,
    *,
    atlas: TileAtlas,
    style_system: StyleSystem,
    graf_feature_tiles: dict[int, tuple[int, int]],
    scale: int,
    tile_size: int,
    show_symbols: str,
    secret_door: str,
    grid: bool,
    font_path: Optional[Path],
) -> "Image.Image":
    assert Image is not None
    assert ImageDraw is not None

    if vault.width <= 0 or vault.height <= 0:
        raise ValueError(f"Vault {vault.serial} '{vault.name}' has no D: layout lines")
    if style.wall is None:
        raise ValueError(f"Style {style.idx} '{style.name}' has no W: tile")
    if not style.floor_variants:
        raise ValueError(f"Style {style.idx} '{style.name}' has no F: tiles")
    if not style.door_variants:
        raise ValueError(f"Style {style.idx} '{style.name}' has no D: tiles")

    tile_px = tile_size * scale
    image = Image.new("RGBA", (vault.width * tile_px, vault.height * tile_px), (0, 0, 0, 0))

    wall_row, wall_col = style.wall
    floor_row, floor_col = style.floor_variants[0]
    door_row, door_col = style.door_variants[0]

    wall_tile = atlas.get(wall_row, wall_col, scale)
    floor_tile = atlas.get(floor_row, floor_col, scale)
    door_tile = atlas.get(door_row, door_col, scale)

    overlay_tile: Optional["Image.Image"] = None
    if style_system.default_vein_overlay is not None:
        ov_row, ov_col = style_system.default_vein_overlay
        overlay_tile = atlas.get(ov_row, ov_col, scale)
        if style_system.overlay_key_rgb is not None:
            overlay_tile = _tile_with_color_key(overlay_tile, style_system.overlay_key_rgb)
        else:
            key_rgb = overlay_tile.getpixel((0, 0))[:3]
            overlay_tile = _tile_with_color_key(overlay_tile, key_rgb)

    # Feature tiles from the old graphics preference data (fixed, not style-driven).
    FEAT_SUNLIGHT = 9
    FEAT_GLYPH = 3
    FEAT_CHASM = 2
    FEAT_RUBBLE = 49
    FEAT_UP_STAIR = 80
    FEAT_DOWN_STAIR = 81
    FEAT_FORGE = 69

    def feat_tile(feat_id: int) -> Optional["Image.Image"]:
        coord = graf_feature_tiles.get(feat_id)
        if coord is None:
            return None
        row, col = coord
        return atlas.get(row, col, scale)

    rubble_tile = feat_tile(FEAT_RUBBLE)
    sunlight_tile = feat_tile(FEAT_SUNLIGHT)
    glyph_tile = feat_tile(FEAT_GLYPH)
    chasm_tile = feat_tile(FEAT_CHASM)
    up_tile = feat_tile(FEAT_UP_STAIR)
    down_tile = feat_tile(FEAT_DOWN_STAIR)
    forge_tile = feat_tile(FEAT_FORGE)

    draw = ImageDraw.Draw(image)
    font = _load_font(font_path, max(10, int(tile_px * 0.75)))

    def should_draw_symbol(symbol: str) -> bool:
        if show_symbols == "none":
            return False
        if show_symbols == "all":
            return symbol != " "
        # markers
        if symbol == "s":
            return secret_door == "hidden"
        return symbol not in _BASE_TERRAIN_SYMBOLS

    def symbol_color(symbol: str) -> tuple[int, int, int, int]:
        if symbol in {"^"}:
            return (220, 120, 255, 255)
        if symbol in {"*", "&", "!", "~"}:
            return (255, 215, 100, 255)
        if symbol in {"1", "2", "3", "4", "?"}:
            return (255, 160, 160, 255)
        if symbol in {"<", ">"}:
            return (200, 220, 255, 255)
        if symbol in {"0"}:
            return (255, 200, 120, 255)
        if symbol in {"7"}:
            return (160, 220, 255, 255)
        return (240, 240, 240, 255)

    for y, row in enumerate(vault.lines):
        for x, symbol in enumerate(row):
            px = x * tile_px
            py = y * tile_px

            if symbol == " ":
                continue

            if symbol in {"#", "$"}:
                base = wall_tile
            elif symbol == ".":
                base = floor_tile
            elif symbol == "%":
                if style.vein is not None:
                    vein_row, vein_col = style.vein
                    base = atlas.get(vein_row, vein_col, scale)
                elif overlay_tile is not None:
                    base = wall_tile.copy()
                    base.alpha_composite(overlay_tile)
                else:
                    base = wall_tile
            elif symbol == "+":
                base = door_tile
            elif symbol == "s":
                base = door_tile if secret_door == "door" else wall_tile
            elif symbol == ":" and rubble_tile is not None:
                base = rubble_tile
            elif symbol == "," and sunlight_tile is not None:
                base = sunlight_tile
            elif symbol == ";" and glyph_tile is not None:
                base = glyph_tile
            elif symbol == "7" and chasm_tile is not None:
                base = chasm_tile
            elif symbol == "0" and forge_tile is not None:
                base = forge_tile
            elif symbol == "<" and up_tile is not None:
                base = up_tile
            elif symbol == ">" and down_tile is not None:
                base = down_tile
            else:
                base = floor_tile

            image.paste(base, (px, py), base)

            # Render symbol as pictogram tile if available
            symbol_tile_coord: Optional[tuple[int, int]] = None
            if symbol == "~":
                # Chest tile depends on vault type
                symbol_tile_coord = VAULT_CHEST_TILES.get(vault.typ or 6, (1, 1))
            elif symbol in VAULT_SYMBOL_TILES:
                symbol_tile_coord = VAULT_SYMBOL_TILES[symbol]

            if symbol_tile_coord is not None:
                sym_row, sym_col = symbol_tile_coord
                symbol_tile = atlas.get(sym_row, sym_col, scale)
                image.alpha_composite(symbol_tile, (px, py))

                # Add 't' overlay for treasure symbols to distinguish from actual chests
                if symbol in _TREASURE_SYMBOLS:
                    cx = px + tile_px // 2
                    cy = py + tile_px // 2
                    _draw_text_centered(
                        draw,
                        (cx, cy),
                        "t",
                        font=font,
                        fill=(255, 255, 255, 255),
                        outline=(0, 0, 0, 220),
                    )
            elif should_draw_symbol(symbol):
                # Fallback to text rendering for symbols without tiles
                cx = px + tile_px // 2
                cy = py + tile_px // 2
                _draw_text_centered(
                    draw,
                    (cx, cy),
                    symbol,
                    font=font,
                    fill=symbol_color(symbol),
                    outline=(0, 0, 0, 220),
                )

    if grid:
        grid_color = (0, 0, 0, 80)
        out_w, out_h = image.size
        for x in range(0, out_w + 1, tile_px):
            draw.line([(x, 0), (x, out_h)], fill=grid_color, width=1)
        for y in range(0, out_h + 1, tile_px):
            draw.line([(0, y), (out_w, y)], fill=grid_color, width=1)

    return image


def _print_vault_table(vaults: list[Vault]) -> None:
    print(f"{'ID':>4}  {'T':>2}  {'D':>2}  {'R':>3}  {'WxH':>9}  Name")
    for vault in vaults:
        dims = f"{vault.width}x{vault.height}" if vault.width and vault.height else "-"
        typ = "-" if vault.typ is None else str(vault.typ)
        depth = "-" if vault.depth is None else str(vault.depth)
        rarity = "-" if vault.rarity is None else str(vault.rarity)
        print(f"{vault.serial:>4}  {typ:>2}  {depth:>2}  {rarity:>3}  {dims:>9}  {vault.name}")


def cmd_list(args: argparse.Namespace) -> int:
    vaults = parse_vault_txt(Path(args.vault_txt))
    filtered = vaults
    if args.type is not None:
        filtered = [v for v in filtered if v.typ == args.type]
    if args.depth_min is not None:
        filtered = [v for v in filtered if v.depth is not None and v.depth >= args.depth_min]
    if args.depth_max is not None:
        filtered = [v for v in filtered if v.depth is not None and v.depth <= args.depth_max]
    if args.filter:
        pattern = re.compile(args.filter, re.IGNORECASE)
        filtered = [v for v in filtered if pattern.search(v.name)]
    _print_vault_table(filtered)
    return 0


def cmd_styles(args: argparse.Namespace) -> int:
    style_system = parse_style_txt(Path(args.style_txt))
    styles = style_system.styles
    ids = sorted(styles.keys())
    if args.filter:
        pattern = re.compile(args.filter, re.IGNORECASE)
        ids = [sid for sid in ids if pattern.search(styles[sid].name)]

    print(f"{'ID':>3}  Name")
    for sid in ids:
        print(f"{sid:>3}  {styles[sid].name}")
    return 0


def cmd_show(args: argparse.Namespace) -> int:
    vaults = parse_vault_txt(Path(args.vault_txt))
    selected = _find_vaults(vaults, args.vault, regex=args.regex)
    for idx, vault in enumerate(selected):
        if idx:
            print()
        header = f"{vault.serial}: {vault.name}"
        meta = f"type={vault.typ} depth={vault.depth} rarity={vault.rarity} size={vault.width}x{vault.height}"
        print(header)
        print(meta)
        for line in vault.lines:
            print(line)
    return 0


def cmd_render(args: argparse.Namespace) -> int:
    _require_pillow()
    vaults = parse_vault_txt(Path(args.vault_txt))
    selected_vaults = _find_vaults(vaults, args.vault, regex=args.regex)

    style_system = parse_style_txt(Path(args.style_txt))
    level_rules = parse_style_levels_txt(Path(args.style_levels_txt))
    styles = style_system.styles

    graf_feature_tiles = parse_graf_feature_tiles(Path(args.graf_prf)) if args.graf_prf else {}
    atlas = TileAtlas(Path(args.tileset), tile_size=args.tile_size)

    out_dir = Path(args.out_dir) if args.out_dir else DEFAULT_OUT_DIR
    out_dir.mkdir(parents=True, exist_ok=True)

    # Add a --force flag to allow re-rendering everything when requested.
    force = getattr(args, "force", False)

    for vault in selected_vaults:
        style_ids = _select_styles(
            args.styles or [],
            vault=vault,
            styles=styles,
            level_rules=level_rules,
            depth_hint=args.depth,
        )

        vault_slug = f"{vault.serial:03d}_{_slugify(vault.name)}"
        vault_dir = out_dir / vault_slug
        # If the vault directory exists and --force is not set, skip styles that already exist.
        if vault_dir.exists() and not force:
            existing_files = [p.name for p in vault_dir.iterdir() if p.is_file()]
            existing_style_prefixes = {f"style{int(p.split("_")[0][5:]):02d}_" for p in existing_files if p.startswith("style") and "_" in p}
            missing_style_ids = [sid for sid in style_ids if f"style{sid:02d}_" not in existing_style_prefixes]
            if not missing_style_ids:
                if not args.quiet:
                    print(f"Skipping {vault_slug} (all styles present)")
                continue
            style_ids = missing_style_ids
        else:
            vault_dir.mkdir(parents=True, exist_ok=True)

        for style_id in style_ids:
            style = styles[style_id]
            image = render_vault_png(
                vault,
                style,
                atlas=atlas,
                style_system=style_system,
                graf_feature_tiles=graf_feature_tiles,
                scale=args.scale,
                tile_size=args.tile_size,
                show_symbols=args.show_symbols,
                secret_door=args.secret_door,
                grid=args.grid,
                font_path=Path(args.font) if args.font else None,
            )

            style_slug = f"style{style.idx:02d}_{_slugify(style.name, max_len=32)}"
            out_path = vault_dir / f"{style_slug}.png"
            image.save(out_path)
            if not args.quiet:
                print(out_path)

    return 0


def cmd_sheet(args: argparse.Namespace) -> int:
    _require_pillow()
    assert Image is not None
    assert ImageDraw is not None

    vaults = parse_vault_txt(Path(args.vault_txt))
    selected_vaults = _find_vaults(vaults, args.vault, regex=args.regex)
    if len(selected_vaults) != 1:
        raise ValueError("sheet command requires selecting exactly one vault (use a single --vault)")
    vault = selected_vaults[0]

    style_system = parse_style_txt(Path(args.style_txt))
    level_rules = parse_style_levels_txt(Path(args.style_levels_txt))
    styles = style_system.styles

    graf_feature_tiles = parse_graf_feature_tiles(Path(args.graf_prf)) if args.graf_prf else {}
    atlas = TileAtlas(Path(args.tileset), tile_size=args.tile_size)

    style_ids = _select_styles(
        args.styles or [],
        vault=vault,
        styles=styles,
        level_rules=level_rules,
        depth_hint=args.depth,
    )

    rendered: list[tuple[int, "Image.Image"]] = []
    for style_id in style_ids:
        rendered.append(
            (
                style_id,
                render_vault_png(
                    vault,
                    styles[style_id],
                    atlas=atlas,
                    style_system=style_system,
                    graf_feature_tiles=graf_feature_tiles,
                    scale=args.scale,
                    tile_size=args.tile_size,
                    show_symbols=args.show_symbols,
                    secret_door=args.secret_door,
                    grid=args.grid,
                    font_path=Path(args.font) if args.font else None,
                ),
            )
        )

    cols = max(1, args.cols)
    rows = (len(rendered) + cols - 1) // cols

    pad = 12
    caption_h = int(args.tile_size * args.scale * 0.9) + 10
    cell_w = rendered[0][1].size[0]
    cell_h = rendered[0][1].size[1] + caption_h

    sheet_w = cols * cell_w + (cols + 1) * pad
    sheet_h = rows * cell_h + (rows + 1) * pad + caption_h

    sheet = Image.new("RGBA", (sheet_w, sheet_h), (20, 20, 20, 255))
    draw = ImageDraw.Draw(sheet)
    font = _load_font(Path(args.font) if args.font else None, max(12, int(caption_h * 0.6)))

    title = f"{vault.serial}: {vault.name}"
    draw.text((pad, pad), title, font=font, fill=(240, 240, 240, 255))

    for i, (style_id, img) in enumerate(rendered):
        r = i // cols
        c = i % cols
        x0 = pad + c * (cell_w + pad)
        y0 = pad + caption_h + r * (cell_h + pad)

        style = styles[style_id]
        label = f"{style.idx}: {style.name}"
        draw.text((x0, y0), label, font=font, fill=(240, 240, 240, 255))

        sheet.alpha_composite(img, (x0, y0 + caption_h))

    default_out = DEFAULT_OUT_DIR / f"{vault.serial:03d}_{_slugify(vault.name)}__sheet.png"
    out_path = Path(args.out) if args.out else default_out
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out_path)
    if not args.quiet:
        print(out_path)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="vaultviz", add_help=True)

    parser.add_argument("--vault-txt", default=str(DEFAULT_VAULT_TXT), help="Path to vault.txt")
    parser.add_argument("--style-txt", default=str(DEFAULT_STYLE_TXT), help="Path to style.txt")
    parser.add_argument("--style-levels-txt", default=str(DEFAULT_STYLE_LEVELS_TXT), help="Path to style-levels.txt")

    sub = parser.add_subparsers(dest="cmd", required=True)

    list_p = sub.add_parser("list", help="List vaults (filterable)")
    list_p.add_argument("--filter", help="Regex on vault name")
    list_p.add_argument("--type", type=int, help="Vault type (6,7,8,9,10)")
    list_p.add_argument("--depth-min", type=int)
    list_p.add_argument("--depth-max", type=int)
    list_p.set_defaults(func=cmd_list)

    styles_p = sub.add_parser("styles", help="List visual styles from style.txt")
    styles_p.add_argument("--filter", help="Regex on style name")
    styles_p.set_defaults(func=cmd_styles)

    show_p = sub.add_parser("show", help="Print the raw vault layout to stdout")
    show_p.add_argument("--vault", action="append", default=[], help="Vault serial or name substring")
    show_p.add_argument("--regex", action="store_true", help="Treat --vault as a regex pattern")
    show_p.set_defaults(func=cmd_show)

    render_p = sub.add_parser("render", help="Render vault(s) to PNG (one file per style)")
    render_p.add_argument("--vault", action="append", default=[], help="Vault serial or name substring")
    render_p.add_argument("--regex", action="store_true", help="Treat --vault as a regex pattern")
    render_p.add_argument(
        "--styles",
        action="append",
        default=[],
        help="Style selectors: auto|vault|level|all(vault-scoped)|global(all ids)|0-5,10|<name-substring> (repeatable)",
    )
    render_p.add_argument("--depth", type=int, help="Depth hint to resolve 'level' / '*' / '$' style selectors")
    render_p.add_argument("--tileset", default=str(DEFAULT_TILESET), help="Tileset PNG path (default: 16x16.png)")
    render_p.add_argument("--graf-prf", default=DEFAULT_GRAF_PRF, help="Optional legacy graphics mapping path")
    render_p.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR), help="Output directory")
    render_p.add_argument("--tile-size", type=int, default=16, help="Tileset tile size in pixels")
    render_p.add_argument("--scale", type=int, default=2, help="Nearest-neighbor scale factor")
    render_p.add_argument("--font", help="Optional TTF font path for symbol overlay")
    render_p.add_argument(
        "--show-symbols",
        choices=["none", "markers", "all"],
        default="markers",
        help="Overlay vault symbols over tiles",
    )
    render_p.add_argument(
        "--secret-door",
        choices=["hidden", "door"],
        default="hidden",
        help="How to draw secret doors ('s')",
    )
    render_p.add_argument("--grid", action="store_true", help="Draw a faint grid between tiles")
    render_p.add_argument("--quiet", action="store_true", help="Suppress per-file output")
    render_p.add_argument("--force", action="store_true", help="Re-render existing styles (overwrite)")
    render_p.set_defaults(func=cmd_render)

    sheet_p = sub.add_parser("sheet", help="Render a single vault across many styles into one PNG")
    sheet_p.add_argument("--vault", action="append", default=[], help="Vault serial or name substring")
    sheet_p.add_argument("--regex", action="store_true", help="Treat --vault as a regex pattern")
    sheet_p.add_argument(
        "--styles",
        action="append",
        default=[],
        help="Style selectors: auto|vault|level|all(vault-scoped)|global(all ids)|0-5,10|<name-substring> (repeatable)",
    )
    sheet_p.add_argument("--depth", type=int, help="Depth hint to resolve 'level' / '*' / '$' style selectors")
    sheet_p.add_argument("--tileset", default=str(DEFAULT_TILESET), help="Tileset PNG path (default: 16x16.png)")
    sheet_p.add_argument("--graf-prf", default=DEFAULT_GRAF_PRF, help="Optional legacy graphics mapping path")
    sheet_p.add_argument("--tile-size", type=int, default=16, help="Tileset tile size in pixels")
    sheet_p.add_argument("--scale", type=int, default=2, help="Nearest-neighbor scale factor")
    sheet_p.add_argument("--font", help="Optional TTF font path for labels and symbol overlay")
    sheet_p.add_argument("--cols", type=int, default=4, help="Sheet columns")
    sheet_p.add_argument("--out", help="Output PNG path")
    sheet_p.add_argument(
        "--show-symbols",
        choices=["none", "markers", "all"],
        default="markers",
        help="Overlay vault symbols over tiles",
    )
    sheet_p.add_argument(
        "--secret-door",
        choices=["hidden", "door"],
        default="hidden",
        help="How to draw secret doors ('s')",
    )
    sheet_p.add_argument("--grid", action="store_true", help="Draw a faint grid between tiles")
    sheet_p.add_argument("--quiet", action="store_true", help="Suppress output")
    sheet_p.set_defaults(func=cmd_sheet)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.func(args))
    except BrokenPipeError:
        return 0
    except Exception as exc:
        print(f"vaultviz: error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
