#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Update the '# Smithing difficulty: N' comments in artefact.txt to match
the values calculated by calc_artefact_difficulty.py.

Usage:
    python tools/update_artefact_difficulties.py            # update in-place
    python tools/update_artefact_difficulties.py --dry-run  # show diffs only
    python tools/update_artefact_difficulties.py --check    # exit 1 if any differ
"""

import os
import re
import sys
import argparse

# ---------------------------------------------------------------------------
# Make the scripts/ directory importable so we can reuse calc_artefact_difficulty
# ---------------------------------------------------------------------------
_repo_root = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))
_scripts_dir = os.path.join(_repo_root, 'scripts')
if _scripts_dir not in sys.path:
    sys.path.insert(0, _scripts_dir)

import calc_artefact_difficulty as cad

# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------

def _find_file(candidates):
    for p in candidates:
        if os.path.isfile(p):
            return p
    return None


def _lib_paths(filename):
    return [
        os.path.join(_repo_root, 'lib', 'edit', filename),
        os.path.join(_scripts_dir, '..', 'lib', 'edit', filename),
    ]


# ---------------------------------------------------------------------------
# Core logic
# ---------------------------------------------------------------------------

def compute_difficulties():
    """Return a dict mapping artefact idx -> calculated difficulty."""
    artefact_file = _find_file(_lib_paths('artefact.txt'))
    if not artefact_file:
        raise FileNotFoundError("Cannot find lib/edit/artefact.txt")

    ability_file = _find_file(_lib_paths('ability.txt'))
    if ability_file:
        cad.parse_ability_file(ability_file)

    object_file = _find_file(_lib_paths('object.txt'))
    objects = cad.parse_object_file(object_file) if object_file else []
    if objects:
        cad.populate_objects_dict(objects)

    artefacts, _sval_order = cad.parse_artefact_file(artefact_file)

    result = {}
    for art in artefacts:
        art['type'] = 'artefact'
        art['rarity_schedule'] = [(art['depth'], art['rarity'])]
        result[art['idx']] = cad.calculate_difficulty(art)
    return result


def update_file(artefact_path, difficulties, *, dry_run=False):
    """
    Scan artefact_path for '# Smithing difficulty: N' lines, pair each with the
    next 'N:<idx>:' line found after it, and update the value to match
    difficulties[idx].

    Returns list of (line_number_1based, old_line, new_line) tuples for every
    change (or would-be change in dry_run mode).
    """
    with open(artefact_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    DIFF_RE = re.compile(r'^(# Smithing difficulty:)\s*(\d+)\s*$')
    N_RE    = re.compile(r'^N:(\d+):')

    changes = []
    pending_diff_idx = None   # index into `lines` of the most recent difficulty comment

    for i, line in enumerate(lines):
        m_n = N_RE.match(line)
        if m_n and pending_diff_idx is not None:
            art_idx = int(m_n.group(1))
            if art_idx in difficulties:
                new_val = difficulties[art_idx]
                old_line = lines[pending_diff_idx]
                md = DIFF_RE.match(old_line)
                old_val = int(md.group(2))
                if old_val != new_val:
                    new_line = f"{md.group(1)} {new_val}\n"
                    changes.append((pending_diff_idx + 1, old_line.rstrip('\n'), new_line.rstrip('\n')))
                    lines[pending_diff_idx] = new_line
            pending_diff_idx = None
            continue

        m_d = DIFF_RE.match(line)
        if m_d:
            pending_diff_idx = i
            # Don't reset on another difficulty comment before an N: line;
            # just overwrite (shouldn't happen but be safe).

    if not dry_run and changes:
        with open(artefact_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)

    return changes


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Update # Smithing difficulty comments in artefact.txt"
    )
    parser.add_argument('--dry-run', '-n', action='store_true',
                        help='Print what would change without writing the file')
    parser.add_argument('--check', action='store_true',
                        help='Like --dry-run but exit with code 1 if anything differs')
    parser.add_argument('--artefact-file', metavar='PATH',
                        help='Override path to artefact.txt')
    args = parser.parse_args(argv)

    dry = args.dry_run or args.check

    artefact_path = args.artefact_file or _find_file(_lib_paths('artefact.txt'))
    if not artefact_path:
        print("ERROR: Cannot find lib/edit/artefact.txt", file=sys.stderr)
        return 2

    print("Computing difficulties...")
    difficulties = compute_difficulties()
    print(f"  Computed {len(difficulties)} artefact difficulties.")

    changes = update_file(artefact_path, difficulties, dry_run=dry)

    if not changes:
        print("All Smithing difficulty comments are already up to date.")
        return 0

    action = "Would update" if dry else "Updated"
    print(f"\n{action} {len(changes)} line(s) in {artefact_path}:\n")
    for lineno, old, new in changes:
        # Find the artefact name by scanning forward from the difficulty line
        print(f"  Line {lineno:4d}: {old!r}  ->  {new!r}")

    if not dry:
        print(f"\nDone. {len(changes)} difficulty comment(s) updated.")
    
    if args.check and changes:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
