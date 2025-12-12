#!/usr/bin/env python3
from collections import defaultdict, Counter
import re
from statistics import mean, median

path = 'lib/edit/artefact.txt'
entries = []
current = {'N':None, 'name':None, 'W':None}
with open(path, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.rstrip('\n')
        if line.startswith('N:'):
            if current['N'] is not None:
                entries.append(current)
            parts = line.split(':',2)
            idx = parts[1]
            name = parts[2] if len(parts)>2 else ''
            current = {'N':int(idx), 'name':name, 'W':None}
        elif line.startswith('W:') and current['N'] is not None:
            wparts = line[2:].split(':')
            if len(wparts) >= 2:
                try:
                    depth = int(wparts[0])
                    rarity = int(wparts[1])
                except ValueError:
                    depth = None
                    rarity = None
            else:
                depth = None
                rarity = None
            current['W'] = {'depth':depth, 'rarity':rarity, 'raw':line[2:]}
    if current['N'] is not None:
        entries.append(current)

w_entries = [e for e in entries if e['W'] and e['W']['depth'] is not None]
all_depths = [e['W']['depth'] for e in w_entries]
all_rarities = [e['W']['rarity'] for e in w_entries]

depth_counts = Counter(all_depths)
rarity_counts = Counter(all_rarities)

summary = {
    'total_artefacts': len(entries),
    'artefacts_with_W': len(w_entries),
    'depth_min': min(all_depths) if all_depths else None,
    'depth_max': max(all_depths) if all_depths else None,
    'depth_mean': mean(all_depths) if all_depths else None,
    'depth_median': median(all_depths) if all_depths else None,
    'rarity_min': min(all_rarities) if all_rarities else None,
    'rarity_max': max(all_rarities) if all_rarities else None,
    'rarity_mean': mean(all_rarities) if all_rarities else None,
    'rarity_median': median(all_rarities) if all_rarities else None,
}

depth_rarity = defaultdict(list)
for e in w_entries:
    d = e['W']['depth']
    r = e['W']['rarity']
    depth_rarity[d].append(r)

by_depth_sorted = sorted(w_entries, key=lambda x: x['W']['depth'])
min_depth_examples = by_depth_sorted[:10]
max_depth_examples = by_depth_sorted[-10:]

by_rarity_sorted = sorted(w_entries, key=lambda x: x['W']['rarity'])
min_rarity_examples = by_rarity_sorted[:10]
max_rarity_examples = by_rarity_sorted[-10:]

print('TOTAL_ENTRIES', summary['total_artefacts'])
print('WITH_W', summary['artefacts_with_W'])
print('DEPTH min/max/mean/median', summary['depth_min'], summary['depth_max'], round(summary['depth_mean'],2), summary['depth_median'])
print('RARITY min/max/mean/median', summary['rarity_min'], summary['rarity_max'], round(summary['rarity_mean'],2), summary['rarity_median'])

print('\nMost common depths (by count, top 20):')
for d, c in depth_counts.most_common(20):
    print(d, c)

print('\nMost common rarities (by count, top 20):')
for r, c in rarity_counts.most_common(20):
    print(r, c)

print('\nDepth -> median rarity (all depths):')
for d in sorted(depth_rarity.keys()):
    vals = depth_rarity[d]
    print(d, 'count', len(vals), 'median', median(vals) if vals else None)

print('\nExamples (lowest depth):')
for e in min_depth_examples:
    print(e['N'], e['name'], e['W'])
print('\nExamples (highest depth):')
for e in max_depth_examples:
    print(e['N'], e['name'], e['W'])

print('\nExamples (lowest rarity):')
for e in min_rarity_examples:
    print(e['N'], e['name'], e['W'])
print('\nExamples (highest rarity):')
for e in max_rarity_examples:
    print(e['N'], e['name'], e['W'])

# Save CSV summary
out = 'artefacts_W_summary.csv'
with open(out, 'w', encoding='utf-8') as fh:
    fh.write('idx,name,depth,rarity\n')
    for e in w_entries:
        fh.write(f"{e['N']},{e['name'].replace(',', '')},{e['W']['depth']},{e['W']['rarity']}\n")
print('\nSaved', out)
