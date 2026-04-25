#!/usr/bin/env python3
"""Convert WAV audio under lib/xtra to sibling OGG files.

By default this scans the repository's ``lib/xtra`` tree, skips the
reference/source pack material under ``lib/xtra/sound/packs``, and writes
``.ogg`` files next to each ``.wav``.

Examples:
    python tools/convert_xtra_wav_to_ogg.py
    python tools/convert_xtra_wav_to_ogg.py --dry-run
    python tools/convert_xtra_wav_to_ogg.py --delete-source --overwrite
    python tools/convert_xtra_wav_to_ogg.py --include-packs
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterator


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = REPO_ROOT / "lib" / "xtra"
PACKS_ROOT = REPO_ROOT / "lib" / "xtra" / "sound" / "packs"
DEFAULT_QUALITY = "5"
OGG_CODEC = "libvorbis"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert .wav files under lib/xtra to sibling .ogg files."
    )
    parser.add_argument(
        "root",
        nargs="?",
        default=str(DEFAULT_ROOT),
        help="Root directory to scan (defaults to repo lib/xtra).",
    )
    parser.add_argument(
        "--include-packs",
        action="store_true",
        help="Also process lib/xtra/sound/packs source/reference material.",
    )
    parser.add_argument(
        "--ffmpeg",
        default="ffmpeg",
        help="ffmpeg executable or full path (defaults to ffmpeg on PATH).",
    )
    parser.add_argument(
        "--quality",
        default=DEFAULT_QUALITY,
        help="Vorbis quality scale passed to ffmpeg's -q:a option (default: 5).",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing .ogg files.",
    )
    parser.add_argument(
        "--delete-source",
        action="store_true",
        help="Delete the source .wav after a successful conversion.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the files that would be converted without invoking ffmpeg.",
    )
    return parser.parse_args()


def resolve_root(raw_root: str) -> Path:
    root = Path(raw_root).expanduser()
    if not root.is_absolute():
        root = REPO_ROOT / root
    return root.resolve()


def is_under(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def iter_wav_files(root: Path, include_packs: bool) -> Iterator[Path]:
    for current_root, dirnames, filenames in os.walk(root):
        current_path = Path(current_root)
        if not include_packs and current_path == PACKS_ROOT.parent:
            dirnames[:] = [
                name for name in dirnames if current_path / name != PACKS_ROOT
            ]

        for filename in filenames:
            candidate = current_path / filename
            if candidate.suffix.lower() != ".wav":
                continue
            if not include_packs and is_under(candidate, PACKS_ROOT):
                continue
            yield candidate


def locate_ffmpeg(command: str) -> str | None:
    found = shutil.which(command)
    if found:
        return found

    candidate = Path(command)
    if candidate.is_file():
        return str(candidate)

    return None


def relpath_or_abs(path: Path) -> str:
    if is_under(path, REPO_ROOT):
        return str(path.relative_to(REPO_ROOT))
    return str(path)


def convert_file(
    source: Path,
    ffmpeg_path: str,
    quality: str,
    overwrite: bool,
) -> Path:
    destination = source.with_suffix(".ogg")

    if destination.exists() and not overwrite:
        return destination

    destination.parent.mkdir(parents=True, exist_ok=True)
    temp_destination = destination.with_name(f"{destination.stem}.tmp.ogg")

    if temp_destination.exists():
        temp_destination.unlink()

    command = [
        ffmpeg_path,
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-i",
        str(source),
        "-vn",
        "-map_metadata",
        "0",
        "-c:a",
        OGG_CODEC,
        "-q:a",
        quality,
        str(temp_destination),
    ]

    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        if temp_destination.exists():
            temp_destination.unlink()
        stderr = result.stderr.strip()
        message = stderr if stderr else "no error output"
        raise RuntimeError(f"ffmpeg failed: {message}")

    temp_destination.replace(destination)
    return destination


def main() -> int:
    args = parse_args()
    root = resolve_root(args.root)

    if not root.exists():
        print(f"[error] Missing root directory: {root}", file=sys.stderr)
        return 1

    ffmpeg_path = None
    if not args.dry_run:
        ffmpeg_path = locate_ffmpeg(args.ffmpeg)
        if not ffmpeg_path:
            print(
                f"[error] Could not find ffmpeg executable '{args.ffmpeg}'. "
                "Use --ffmpeg with a full path or install ffmpeg.",
                file=sys.stderr,
            )
            return 1

    converted = 0
    skipped = 0
    would_convert = 0
    failed = 0

    for source in iter_wav_files(root, args.include_packs):
        destination = source.with_suffix(".ogg")
        rel_source = relpath_or_abs(source)
        rel_destination = relpath_or_abs(destination)

        if destination.exists() and not args.overwrite:
            skipped += 1
            print(f"[skip] {rel_source} -> existing {rel_destination}")
            continue

        if args.dry_run:
            would_convert += 1
            print(f"[dry-run] {rel_source} -> {rel_destination}")
            continue

        try:
            convert_file(source, ffmpeg_path, args.quality, args.overwrite)
        except Exception as exc:
            failed += 1
            print(f"[error] {rel_source}: {exc}", file=sys.stderr)
            continue

        converted += 1
        print(f"[ok] {rel_source} -> {rel_destination}")

        if args.delete_source:
            try:
                source.unlink()
            except Exception as exc:
                failed += 1
                print(f"[error] {rel_source}: could not delete source: {exc}", file=sys.stderr)
            else:
                print(f"[removed] {rel_source}")

    if args.dry_run:
        print(f"Would convert {would_convert} file(s); skipped {skipped} existing output(s).")
    else:
        print(
            f"Converted {converted} file(s); skipped {skipped} existing output(s); "
            f"failed {failed}."
        )

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
