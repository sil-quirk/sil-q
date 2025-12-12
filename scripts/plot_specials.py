#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import os

in_csv = 'scripts/output/specials_full.csv'
out_dir = 'scripts/output/plots_specials'
os.makedirs(out_dir, exist_ok=True)

df = pd.read_csv(in_csv)
# drop rows without depth
df = df[df['depth'].notna()]

# Depth histogram
plt.figure(figsize=(8,4))
plt.hist(df['depth'].astype(int), bins=range(int(df['depth'].min()), int(df['depth'].max())+2), color='tab:blue', edgecolor='k')
plt.title('Specials Depth Distribution')
plt.xlabel('Depth')
plt.ylabel('Count')
plt.grid(axis='y', alpha=0.25)
plt.tight_layout()
plt.savefig(os.path.join(out_dir, 'special_depth_hist.png'))
plt.close()

# Rarity histogram
plt.figure(figsize=(8,4))
plt.hist(df['rarity'].astype(int), bins=range(int(df['rarity'].min()), int(df['rarity'].max())+2), color='tab:orange', edgecolor='k')
plt.title('Specials Rarity Distribution')
plt.xlabel('Rarity')
plt.ylabel('Count')
plt.grid(axis='y', alpha=0.25)
plt.tight_layout()
plt.savefig(os.path.join(out_dir, 'special_rarity_hist.png'))
plt.close()

# Scatter depth vs rarity colored by tval
plt.figure(figsize=(8,6))
labels = df['tvals'].astype(str)
unique = labels.unique()
colors = plt.cm.get_cmap('tab20', len(unique))
color_map = {val: colors(i) for i, val in enumerate(unique)}

for val in unique:
    sub = df[labels == val]
    plt.scatter(sub['depth'].astype(int), sub['rarity'].astype(int), label=f'tvals {val}', color=color_map[val], alpha=0.9, edgecolors='k', linewidths=0.2, s=60)

plt.title('Special depth vs rarity (by tvals)')
plt.xlabel('Depth')
plt.ylabel('Rarity')
plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize='small', ncol=1)
plt.grid(alpha=0.25)
plt.tight_layout()
plt.savefig(os.path.join(out_dir, 'special_depth_vs_rarity_scatter.png'), dpi=150)
plt.close()

print('Plots saved to', out_dir)
