#!/usr/bin/env python3
import importlib.util
from pathlib import Path

# Load parse_vault_txt from tools/vaultviz.py without requiring package import
def parse_vaults_simple(path: Path):
    entries = []
    with path.open('r', encoding='utf-8', newline='') as fh:
        for raw in fh:
            line = raw.rstrip('\n')
            if not line:
                continue
            if line.startswith('N:'):
                parts = line.split(':', 2)
                if len(parts) >= 3:
                    sid = int(parts[1])
                    name = parts[2]
                    entries.append((sid, name))
    return entries

parse_vault_txt = None  # not used when parsing simple


def main():
    cur = parse_vaults_simple(Path('lib/edit/vault.txt'))
    orig = parse_vaults_simple(Path('lib/edit/vault - silq.txt'))
    orig_serials = {s for s,n in orig}
    orig_names = {n for s,n in orig}
    new = [(s,n) for s,n in cur if s not in orig_serials and n not in orig_names]
    print(f"Found {len(new)} new vault(s)")
    for s,n in new:
        print(f"{s}\t{n}")
    out = Path('scripts/output')
    out.mkdir(parents=True, exist_ok=True)
    with open(out / 'new_vaults.txt', 'w', encoding='utf-8') as f:
        for s,n in new:
            f.write(str(s) + '\n')
    print('Wrote scripts/output/new_vaults.txt')

if __name__ == '__main__':
    main()
