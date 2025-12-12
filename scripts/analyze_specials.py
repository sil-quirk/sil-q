#!/usr/bin/env python3
import csv
import os
from collections import defaultdict, Counter
from statistics import mean, median

path = 'lib/edit/special.txt'
entries = []
current = {'N':None, 'name':None, 'W':None, 'T':[]}
with open(path, 'r', encoding='utf-8') as f:
    for line in f:
        line = line.rstrip('\n')
        if line.startswith('N:'):
            if current['N'] is not None:
                entries.append(current)
            parts = line.split(':',2)
            idx = parts[1]
            name = parts[2] if len(parts)>2 else ''
            current = {'N':int(idx), 'name':name, 'W':None, 'T':[]}
        elif line.startswith('W:') and current['N'] is not None:
            # W: depth : rarity : max_depth : cost
            parts = line[2:].split(':')
            depth = int(parts[0]) if parts[0].isdigit() else None
            rarity = int(parts[1]) if len(parts)>1 and parts[1].isdigit() else None
            max_depth = int(parts[2]) if len(parts)>2 and parts[2].isdigit() else None
            cost = int(parts[3]) if len(parts)>3 and parts[3].isdigit() else None
            current['W'] = {'depth':depth, 'rarity':rarity, 'max_depth':max_depth, 'cost':cost, 'raw':line[2:]}
        elif line.startswith('T:') and current['N'] is not None:
            # T: tval : min_sval : max_sval
            parts = line[2:].split(':')
            try:
                tval = int(parts[0])
            except Exception:
                tval = None
            min_sval = parts[1] if len(parts) > 1 else None
            max_sval = parts[2] if len(parts) > 2 else None
            current['T'].append({'tval':tval, 'min_sval':min_sval, 'max_sval':max_sval, 'raw':line[2:]})
    if current['N'] is not None:
        entries.append(current)

# prepare out dir
out = 'scripts/output'
os.makedirs(out, exist_ok=True)

# write full CSV
csv_full = os.path.join(out, 'specials_full.csv')
with open(csv_full, 'w', encoding='utf-8', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['idx', 'name', 'tvals', 'depth', 'rarity', 'max_depth', 'cost', 'raw_W'])
    for e in entries:
        tvals = ';'.join(str(t['tval']) for t in e['T']) if e['T'] else ''
        depth = e['W']['depth'] if e['W'] else ''
        rarity = e['W']['rarity'] if e['W'] else ''
        maxd = e['W']['max_depth'] if e['W'] else ''
        cost = e['W']['cost'] if e['W'] else ''
        raw = e['W']['raw'] if e['W'] else ''
        w.writerow([e['N'], e['name'], tvals, depth, rarity, maxd, cost, raw])

# filter entries that have W depth
w_entries = [e for e in entries if e.get('W') and e['W'].get('depth') is not None]

depths = [e['W']['depth'] for e in w_entries]
rarities = [e['W']['rarity'] for e in w_entries]

summary = {
    'total_entries': len(entries),
    'entries_with_W': len(w_entries),
    'depth_min': min(depths) if depths else None,
    'depth_max': max(depths) if depths else None,
    'depth_mean': mean(depths) if depths else None,
    'depth_median': median(depths) if depths else None,
    'rarity_min': min(rarities) if rarities else None,
    'rarity_max': max(rarities) if rarities else None,
    'rarity_mean': mean(rarities) if rarities else None,
    'rarity_median': median(rarities) if rarities else None,
}

# group by tval (each special may map to multiple tvals)
by_tval = defaultdict(list)
for e in w_entries:
    tvals = [t['tval'] for t in e['T'] if t.get('tval') is not None]
    if not tvals:
        by_tval[None].append(e)
    else:
        for t in tvals:
            by_tval[t].append(e)

# per tval csv
csv_tval = os.path.join(out, 'specials_by_tval.csv')
with open(csv_tval, 'w', encoding='utf-8', newline='') as fh:
    w2 = csv.writer(fh)
    w2.writerow(['tval', 'count', 'depth_min', 'depth_max', 'depth_mean', 'depth_median', 'rarity_min', 'rarity_max', 'rarity_mean', 'rarity_median'])
    for tval, items in sorted(by_tval.items(), key=lambda x: (x[0] is None, x[0])):
        ds = [it['W']['depth'] for it in items if it['W']]
        rs = [it['W']['rarity'] for it in items if it['W']]
        w2.writerow([tval, len(items), min(ds) if ds else '', max(ds) if ds else '', round(mean(ds),2) if ds else '', median(ds) if ds else '', min(rs) if rs else '', max(rs) if rs else '', round(mean(rs),2) if rs else '', median(rs) if rs else ''])

# save summary text
summary_file = os.path.join(out, 'specials_summary.txt')
with open(summary_file, 'w', encoding='utf-8') as fh:
    for k,v in summary.items():
        fh.write(f"{k}: {v}\n")
    fh.write('\nCounts by depth:\n')
    for d,c in Counter(depths).most_common():
        fh.write(f"{d}: {c}\n")
    fh.write('\nCounts by rarity:\n')
    for r,c in Counter(rarities).most_common():
        fh.write(f"{r}: {c}\n")

print('Wrote:', csv_full, csv_tval, summary_file)
print('Global summary:')
for k,v in summary.items():
    print(k, v)

print('\nTop depth counts:')
for d,c in Counter(depths).most_common(15):
    print(d, c)

print('\nTop rarity counts:')
for r,c in Counter(rarities).most_common(15):
    print(r, c)
