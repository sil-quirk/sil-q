#!/usr/bin/env python3
import re
import pandas as pd

def parse_houses(path):
    houses = []
    with open(path, 'r', encoding='utf-8') as f:
        current = None
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#') or line.startswith('V:'):
                continue

            if line.startswith('N:'):
                if current and current['num'] != 0:
                    houses.append(current)
                _, num, name = raw.split(':', 2)
                current = {
                    'num': int(num),
                    'name': name.strip(),
                    'S': [],         # [Str, Dex, Con, Gra]
                    'F': [],         # affinities / penalties
                    'U': [],         # uniques
                    'C_pairs': []    # abilities
                }

            elif line.startswith('S:'):
                current['S'] = list(map(int, line.split(':')[1:5]))

            elif line.startswith('F:'):
                flags = raw.split(':',1)[1].split('|')
                current['F'] = [f.strip() for f in flags if f.strip()]

            elif line.startswith('U:'):
                uniques = raw.split(':',1)[1].split('|')
                current['U'] = [u.strip() for u in uniques if u.strip()]

            elif line.startswith('C:'):
                content = raw.split('#',1)[0][2:].strip()
                parts = content.split(':')
                for i in range(0, len(parts)-1, 2):
                    current['C_pairs'].append((parts[i].strip(), parts[i+1].strip()))

        if current and current['num'] != 0:
            houses.append(current)
    return houses

def parse_races(path):
    races = []
    with open(path, 'r', encoding='utf-8') as f:
        current = None
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#') or line.startswith('V:'):
                continue

            if line.startswith('N:'):
                if current:
                    races.append(current)
                _, num, name = raw.split(':', 2)
                current = {
                    'num': int(num),
                    'name': name.strip(),
                    'S':   [],  # [Str, Dex, Con, Gra]
                    'F':   [],  # affinities / penalties
                    'C':   []   # member hero IDs
                }

            elif line.startswith('S:'):
                current['S'] = list(map(int, line.split(':')[1:5]))

            elif line.startswith('F:'):
                flags = raw.split(':',1)[1].split('|')
                current['F'] = [f.strip() for f in flags if f.strip()]

            elif line.startswith('C:'):
                ids = re.findall(r'\d+', raw)
                current['C'] = list(map(int, ids))

        if current:
            races.append(current)
    return races

def compute_scores(houses, races):
    # build hero→race lookup by membership
    hero2race = {}
    for r in races:
        for hid in r['C']:
            hero2race[hid] = r

    results = []
    for h in houses:
        r = hero2race.get(h['num'], None)

        # 1) Stats by type
        hs = h['S']
        rs = r['S'] if r else [0,0,0,0]
        Str = hs[0] + rs[0]
        Dex = hs[1] + rs[1]
        Con = hs[2] + rs[2]
        Gra = hs[3] + rs[3]
        stats_total = Str + Dex + Con + Gra

        # 2) Net affinities / penalties
        def net_aff(flags):
            return sum(1 for f in flags if f.endswith('_AFFINITY')) \
                 - sum(1 for f in flags if f.endswith('_PENALTY'))
        aff_total = net_aff(h['F']) + (net_aff(r['F']) if r else 0)

        # 3) Unique traits
        uniq_total = len(h['U']) * 2
        #    + FREE flags → +2 each
        free_count = h['F'].count('FREE') + (r['F'].count('FREE') if r else 0)
        uniq_total += free_count * 2

        # 4) Abilities
        abil_total = len(h['C_pairs']) * 0.5

        # 5) Grand total
        total = stats_total + aff_total + uniq_total + abil_total

        # 6) Dot counts
        special = set(h['F'] + (r['F'] if r else []))
        red_count = (
            (2 if 'KINSLAYER' in special else 0) +
            (1 if 'TREACHERY' in special else 0) +
            (1 if 'CURSE' in special else 0) +
            (1 if 'MOR_CURSE' in special else 0)
        )
        green_count = (1 if 'GIFTERU' in special else 0)
        dots = '🔴'*red_count + '🟢'*green_count

        # net_dots for tie-breaker
        net_dots = green_count - red_count

        results.append({
            'Hero':       h['name'],
            'Str':        Str,
            'Dex':        Dex,
            'Con':        Con,
            'Gra':        Gra,
            'Stats':      stats_total,
            'Affinities': aff_total,
            'Unique':     uniq_total,
            'Abilities':  abil_total,
            'Total':      total,
            'Dots':       dots,
            '_net':       net_dots
        })

    # sort by Total descending, then by net_dots descending
    return sorted(results,
                  key=lambda x: (x['Total'], x['_net']),
                  reverse=True)

def main():
    houses = parse_houses('house.txt')
    races  = parse_races('race.txt')
    scores = compute_scores(houses, races)

    df = pd.DataFrame(scores)
    # drop the internal '_net' column before printing
    df = df.drop(columns=['_net'])
    print(df.to_string(index=False))

    input("\nDone — press Enter to exit...")

if __name__ == '__main__':
    main()
