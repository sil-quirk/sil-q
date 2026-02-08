#!/usr/bin/env python3
"""Analyze ability descriptions for word-wrap line counts at 38-char width."""

import sys
import os

WRAP_WIDTH = 38
MAX_LINES = 20
SEPARATOR_LINES = 2  # blank lines between D and E in mode 0

def word_wrap(text, width):
    """Simple word-wrap: split into words, fill lines up to width chars."""
    if not text.strip():
        return []
    words = text.split()
    if not words:
        return []
    lines = []
    current_line = words[0]
    for word in words[1:]:
        if len(current_line) + 1 + len(word) <= width:
            current_line += " " + word
        else:
            lines.append(current_line)
            current_line = word
    lines.append(current_line)
    return lines

def parse_abilities(filepath):
    """Parse ability.txt and extract N, D, E fields."""
    abilities = []
    current = None

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n\r")
            # Strip comments from non-D/E lines
            if line.startswith("N:"):
                if current is not None:
                    abilities.append(current)
                parts = line.split(":", 2)
                num = parts[1].strip()
                name = parts[2].strip() if len(parts) > 2 else "?"
                current = {"num": num, "name": name, "D": "", "E": ""}
            elif line.startswith("D:") and current is not None:
                text = line[2:]
                if current["D"]:
                    current["D"] += " " + text
                else:
                    current["D"] = text
            elif line.startswith("E:") and current is not None:
                text = line[2:]
                if current["E"]:
                    current["E"] += " " + text
                else:
                    current["E"] = text

    if current is not None:
        abilities.append(current)

    return abilities

def main():
    filepath = r"c:\Users\efrem\Documents\GitHub\Sil-More\lib\edit\ability.txt"
    abilities = parse_abilities(filepath)

    results = []
    for ab in abilities:
        d_lines = word_wrap(ab["D"], WRAP_WIDTH)
        e_lines = word_wrap(ab["E"], WRAP_WIDTH)
        d_count = len(d_lines)
        e_count = len(e_lines)
        # Mode 0 total: D lines + 2 blank separator + E lines
        total = d_count + SEPARATOR_LINES + e_count
        results.append({
            "num": ab["num"],
            "name": ab["name"],
            "d_count": d_count,
            "e_count": e_count,
            "total": total,
            "d_text": ab["D"],
            "e_text": ab["E"],
            "d_lines": d_lines,
            "e_lines": e_lines,
        })

    # Sort by total lines descending
    results.sort(key=lambda r: r["total"], reverse=True)

    # Count flagged
    flagged = [r for r in results if r["total"] > MAX_LINES]

    print(f"{'='*80}")
    print(f"ABILITY DESCRIPTION LINE ANALYSIS (wrap width={WRAP_WIDTH}, max={MAX_LINES})")
    print(f"{'='*80}")
    print(f"Total abilities: {len(results)}")
    print(f"Flagged (>{MAX_LINES} lines): {len(flagged)}")
    print(f"{'='*80}\n")

    for r in results:
        over = " *** OVER LIMIT ***" if r["total"] > MAX_LINES else ""
        print(f"#{r['num']:>3s} {r['name']:<30s}  D:{r['d_count']:>2d}  E:{r['e_count']:>2d}  Total:{r['total']:>3d}{over}")

        # Show wrapped lines for context
        print(f"  D text ({r['d_count']} lines):")
        for i, ln in enumerate(r["d_lines"]):
            marker = f"  | {ln}"
            print(marker)

        print(f"  E text ({r['e_count']} lines):")
        for i, ln in enumerate(r["e_lines"]):
            marker = f"  | {ln}"
            print(marker)
        print()

    if flagged:
        print(f"\n{'='*80}")
        print(f"SUMMARY: {len(flagged)} abilities exceed {MAX_LINES} display lines:")
        print(f"{'='*80}")
        for r in flagged:
            print(f"  #{r['num']:>3s} {r['name']:<30s}  Total: {r['total']} lines (D:{r['d_count']} + 2 + E:{r['e_count']})")

if __name__ == "__main__":
    main()
