from simulate_drops import *
import random
from collections import Counter

# Build item table
items = build_item_table_v2(r'../lib/edit/artefact.txt', r'../lib/edit/special.txt')

print('COMPARATIVE ANALYSIS: C Game vs Python Simulation')
print('='*80)
print()

# Simulate 100 drops at depth 19
random.seed(42)
depth19_drops = []
for i in range(100):
    quality = 'normal' if i < 80 else ('good' if i < 90 else 'great')
    item = simulate_drop(items, 19, quality)
    if item:
        depth19_drops.append(item)

# Count by type
type_counts = {'artefact': 0, 'special': 0, 'normal': 0}
for item in depth19_drops:
    type_counts[item.group_type] += 1

print(f'PYTHON SIMULATION - Depth 19 (100 drops):')
print(f'  Total successful: {len(depth19_drops)}')
print(f'  Artefacts: {type_counts["artefact"]} ({100*type_counts["artefact"]/len(depth19_drops):.1f}%)')
print(f'  Specials:  {type_counts["special"]} ({100*type_counts["special"]/len(depth19_drops):.1f}%)')
print(f'  Normal:    {type_counts["normal"]} ({100*type_counts["normal"]/len(depth19_drops):.1f}%)')
print()

# Difficulty distribution
diffs = [item.difficulty for item in depth19_drops]
print(f'  Difficulty range: {min(diffs)} - {max(diffs)}')
print(f'  Average difficulty: {sum(diffs)/len(diffs):.1f}')
print()

# Rarity distribution
rarities = [item.rarity for item in depth19_drops]
avg_rarity = sum(rarities)/len(rarities)
print(f'  Average rarity: {avg_rarity:.2f}')
print(f'  Rarity 1 (common): {rarities.count(1)} ({100*rarities.count(1)/len(rarities):.1f}%)')
print(f'  Rarity 2-5: {sum(1 for r in rarities if 2 <= r <= 5)} ({100*sum(1 for r in rarities if 2 <= r <= 5)/len(rarities):.1f}%)')
print(f'  Rarity 6+: {sum(1 for r in rarities if r >= 6)} ({100*sum(1 for r in rarities if r >= 6)/len(rarities):.1f}%)')
print()

# Top items
top_items = Counter([item.group_name for item in depth19_drops]).most_common(10)
print('  Most common items:')
for name, count in top_items:
    print(f'    {name[:40]:40} x{count}')
