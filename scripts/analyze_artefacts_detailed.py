#!/usr/bin/env python3
import csv
from collections import defaultdict, Counter
from statistics import mean, median
import os
import re

# Parse artefact.txt for N, name, I: and W: lines
path = 'lib/edit/artefact.txt'
entries = []
current = {'N':None, 'name':None, 'I':None, 'W':None}
with open(path, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.rstrip('\n')
        if line.startswith('N:'):
            if current['N'] is not None:
                entries.append(current)
            parts = line.split(':',2)
            idx = parts[1]
            name = parts[2] if len(parts)>2 else ''
            current = {'N':int(idx), 'name':name, 'I':None, 'W':None}
        elif line.startswith('I:') and current['N'] is not None:
            # I: tval : sval : pval
            iparts = line[2:].split(':')
            try:
                tval = int(iparts[0])
            except Exception:
                tval = None
            sval = iparts[1] if len(iparts) > 1 else None
            current['I'] = {'tval':tval, 'sval':sval, 'raw':line[2:]}
        elif line.startswith('W:') and current['N'] is not None:
            wparts = line[2:].split(':')
            if len(wparts) >= 2:
                try:
                    depth = int(wparts[0])
                except ValueError:
                    depth = None
                try:
                    rarity = int(wparts[1])
                except ValueError:
                    rarity = None
            else:
                depth = None
                rarity = None
            current['W'] = {'depth':depth, 'rarity':rarity, 'raw':line[2:]}
    if current['N'] is not None:
        entries.append(current)

# Prepare output folder
out_dir = 'scripts/output'
os.makedirs(out_dir, exist_ok=True)

# Collect statistics
total = len(entries)
w_entries = [e for e in entries if e['W'] and e['W']['depth'] is not None]
entries_with_I = [e for e in entries if e['I'] and e['I']['tval'] is not None]

# Global metrics
depths = [e['W']['depth'] for e in w_entries]
rarities = [e['W']['rarity'] for e in w_entries]

global_stats = {
    'total_entries': total,
    'entries_with_W': len(w_entries),
    'entries_with_I': len(entries_with_I),
    'depth_min': min(depths) if depths else None,
    'depth_max': max(depths) if depths else None,
    'depth_mean': mean(depths) if depths else None,
    'depth_median': median(depths) if depths else None,
    'rarity_min': min(rarities) if rarities else None,
    'rarity_max': max(rarities) if rarities else None,
    'rarity_mean': mean(rarities) if rarities else None,
    'rarity_median': median(rarities) if rarities else None,
}

# tval groups
group_by_tval = defaultdict(list)
for e in w_entries:
    tval = e['I']['tval'] if e.get('I') else None
    group_by_tval[tval].append(e)

# Compute per-tval stats
per_tval = {}
for tval, items in sorted(group_by_tval.items(), key=lambda x: (x[0] is None, x[0])):
    ds = [it['W']['depth'] for it in items if it['W']['depth'] is not None]
    rs = [it['W']['rarity'] for it in items if it['W']['rarity'] is not None]
    per_tval[tval] = {
        'count': len(items),
        'depth_min': min(ds) if ds else None,
        'depth_max': max(ds) if ds else None,
        'depth_mean': mean(ds) if ds else None,
        'depth_median': median(ds) if ds else None,
        'rarity_min': min(rs) if rs else None,
        'rarity_max': max(rs) if rs else None,
        'rarity_mean': mean(rs) if rs else None,
        'rarity_median': median(rs) if rs else None,
    }

# Depth -> list of rarities
depth_rarity = defaultdict(list)
for e in w_entries:
    depth_rarity[e['W']['depth']].append(e['W']['rarity'])

# Save CSV full
csv_path = os.path.join(out_dir, 'artefacts_full.csv')
with open(csv_path, 'w', encoding='utf-8', newline='') as fh:
    writer = csv.writer(fh)
    writer.writerow(['idx', 'name', 'tval', 'sval', 'depth', 'rarity', 'raw_W'])
    for e in entries:
        tval = e['I']['tval'] if e.get('I') else ''
        sval = e['I']['sval'] if e.get('I') else ''
        depth = e['W']['depth'] if e.get('W') else ''
        rarity = e['W']['rarity'] if e.get('W') else ''
        raw = e['W']['raw'] if e.get('W') else ''
        writer.writerow([e['N'], e['name'], tval, sval, depth, rarity, raw])

# Save summary
summary_path = os.path.join(out_dir, 'artefacts_summary.txt')
with open(summary_path, 'w', encoding='utf-8') as fh:
    fh.write('GLOBAL STATS:\n')
    for k, v in global_stats.items():
        fh.write(f'{k}: {v}\n')
    fh.write('\nPER TVAL STATS (tval: count, depth_mean, rarity_mean)\n')
    for tval, stats in per_tval.items():
        fh.write(f'{tval}: {stats["count"]}, depth_mean={stats["depth_mean"]}, rarity_mean={stats["rarity_mean"]}\n')

print('Wrote:', csv_path, summary_path)
print('\nGlobal stats:')
for k, v in global_stats.items():
    print(k, v)

print('\nPer tval counts:')
for tval, stats in per_tval.items():
    print(tval, stats['count'])

# Also export a small aggregated CSV for tval-level
agg_csv = os.path.join(out_dir, 'artefacts_by_tval.csv')
with open(agg_csv, 'w', encoding='utf-8', newline='') as fh:
    writer = csv.writer(fh)
    writer.writerow(['tval', 'count', 'depth_min', 'depth_max', 'depth_mean', 'depth_median', 'rarity_min', 'rarity_max', 'rarity_mean', 'rarity_median'])
    for tval, stats in per_tval.items():
        writer.writerow([tval, stats['count'], stats['depth_min'], stats['depth_max'], stats['depth_mean'], stats['depth_median'], stats['rarity_min'], stats['rarity_max'], stats['rarity_mean'], stats['rarity_median']])
print('Saved aggregated CSV:', agg_csv)
