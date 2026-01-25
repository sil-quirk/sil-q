#!/usr/bin/env python3
from pathlib import Path
ids = [l.strip() for l in Path('scripts/output/new_vaults.txt').read_text(encoding='utf-8').splitlines() if l.strip()]
base = Path('scripts/output/vaultviz_new')
missing_dirs = []
missing_pngs = []
for id_ in ids:
    # find a directory starting with the 3-digit prefix
    slug = None
    for d in base.iterdir():
        if d.is_dir() and d.name.startswith(f"{int(id_):03d}_"):
            slug = d
            break
    if not slug:
        missing_dirs.append(id_)
        continue
    pngs = [p for p in slug.iterdir() if p.suffix.lower() == '.png']
    if not pngs:
        missing_pngs.append((id_, slug.name))

print('Missing directories:', len(missing_dirs))
print('\n'.join(missing_dirs))
print('Missing PNGs in directories:', len(missing_pngs))
for id_, name in missing_pngs:
    print(id_, name)

Path('scripts/output/new_vaults_render_check.txt').write_text(
    'Missing directories:\n' + '\n'.join(missing_dirs) + '\n\nMissing PNGs:\n' + '\n'.join(f'{i}\t{name}' for i,name in missing_pngs),
    encoding='utf-8'
)
print('Wrote scripts/output/new_vaults_render_check.txt')
