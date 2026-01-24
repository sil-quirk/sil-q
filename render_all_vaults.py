#!/usr/bin/env python3
"""Render all vaults to PNG."""
import subprocess
from pathlib import Path

# Get all vault IDs from vault.txt
vault_file = Path('lib/edit/vault.txt')
vault_ids = []

with open(vault_file, 'r', encoding='utf-8') as f:
    for line in f:
        if line.startswith('N:'):
            parts = line.split(':')
            if len(parts) >= 2:
                try:
                    vault_id = int(parts[1])
                    vault_ids.append(vault_id)
                except ValueError:
                    pass

print(f"Found {len(vault_ids)} vaults")

# Build command with all vault IDs
cmd = ['python', 'tools/vaultviz.py', 'render']
for vid in sorted(vault_ids):
    cmd.extend(['--vault', str(vid)])
cmd.extend(['--styles', 'all', '--scale', '2', '--out-dir', 'scripts/output/vaultviz_all'])

print(f"Rendering {len(vault_ids)} vaults to PNG...")
cmd.append('--force')
result = subprocess.run(cmd)
exit(result.returncode)
