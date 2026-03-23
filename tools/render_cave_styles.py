#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_VAULT = "180"
DEFAULT_OUT_DIR = REPO_ROOT / "scripts" / "output" / "vaultviz"
DEFAULT_PYTHON = REPO_ROOT / "src" / ".venv" / "Scripts" / "python.exe"


def _slugify(text: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "_", text.strip().lower())
    slug = slug.strip("_")
    return slug or "vault"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Render a cave vault across every style into one comparison sheet."
    )
    parser.add_argument(
        "--vault",
        default=DEFAULT_VAULT,
        help='Vault serial or name to render (default: 180, "Spider cave")',
    )
    parser.add_argument("--cols", type=int, default=5, help="Sheet columns")
    parser.add_argument("--scale", type=int, default=2, help="Nearest-neighbor scale factor")
    parser.add_argument("--out", help="Output PNG path")
    parser.add_argument("--quiet", action="store_true", help="Suppress the wrapped command output")
    args = parser.parse_args()

    python_exe = DEFAULT_PYTHON if DEFAULT_PYTHON.exists() else Path(sys.executable)
    if args.out:
        out_path = Path(args.out)
    elif str(args.vault) == DEFAULT_VAULT:
        out_path = DEFAULT_OUT_DIR / "spider_cave__all_styles.png"
    else:
        out_path = DEFAULT_OUT_DIR / f"{_slugify(str(args.vault))}__all_styles.png"

    cmd = [
        str(python_exe),
        str(REPO_ROOT / "tools" / "vaultviz.py"),
        "sheet",
        "--vault",
        str(args.vault),
        "--styles",
        "global",
        "--cols",
        str(args.cols),
        "--scale",
        str(args.scale),
        "--out",
        str(out_path),
    ]
    if args.quiet:
        cmd.append("--quiet")

    print("Running: " + " ".join(cmd), flush=True)
    result = subprocess.run(cmd, cwd=REPO_ROOT)
    return int(result.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
