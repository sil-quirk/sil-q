#!/usr/bin/env python3
"""Render all vaults to PNG and collect first render of each into 'all' folder."""
import subprocess
import shutil
import re
import sys
from pathlib import Path

# Get all vault IDs and names from vault.txt
vault_file = Path('lib/edit/vault.txt')
vaults = {}  # {id: name}

with open(vault_file, 'r', encoding='utf-8') as f:
    for line in f:
        if line.startswith('N:'):
            parts = line.split(':')
            if len(parts) >= 3:
                try:
                    vault_id = int(parts[1])
                    vault_name = parts[2].strip()
                    vaults[vault_id] = vault_name
                except ValueError:
                    pass

print(f"Found {len(vaults)} vaults")

# Build command with all vault IDs
cmd = [sys.executable, 'tools/vaultviz.py', 'render']
for vid in sorted(vaults.keys()):
    cmd.extend(['--vault', str(vid)])
cmd.extend(['--styles', 'all', '--scale', '2', '--out-dir', 'scripts/output/vaultviz_all'])

print(f"Rendering {len(vaults)} vaults to PNG...")
result = subprocess.run(cmd)
if result.returncode != 0:
    print("Rendering failed")
    exit(result.returncode)

# Copy first render of each vault to 'all' folder with descriptive name
output_dir = Path('scripts/output/vaultviz_all')
all_dir = output_dir / 'all'
all_dir.mkdir(parents=True, exist_ok=True)

print(f"Collecting first render of each vault into {all_dir}...")

# Find all vault directories (they're formatted as {id}_{name_with_underscores})
for vault_dir in sorted(output_dir.iterdir()):
    if vault_dir.is_dir() and vault_dir.name != 'all':
        pngs = list(vault_dir.glob('*.png'))
        
        if pngs:
            # Get first PNG
            first_png = sorted(pngs)[0]
            # Use directory name as output name (already properly formatted)
            out_name = f'{vault_dir.name}.png'
            out_path = all_dir / out_name
            
            # Copy the file
            shutil.copy2(first_png, out_path)
            print(f"  {vault_dir.name}")

print("Done!")
