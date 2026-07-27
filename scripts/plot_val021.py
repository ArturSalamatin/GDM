import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv("results/val-021/two_reservoirs.csv")
Nx, Ny = df['i'].max() + 1, df['j'].max() + 1

Sw = df.pivot(index='j', columns='i', values='Sw').values
P  = df.pivot(index='j', columns='i', values='P_atm').values
active = df.pivot(index='j', columns='i', values='active').values

wells = {
    'INJ_L':  (1, 2),  'PROD_L': (8, 2),
    'INJ_R':  (12, 2), 'PROD_R': (19, 2),
}

fig, axes = plt.subplots(1, 2, figsize=(14, 4))

for ax, field, title, cmap in [
    (axes[0], Sw, 'Sw', 'RdYlBu_r'),
    (axes[1], P,  'P (атм)', 'viridis'),
]:
    masked = np.ma.masked_where(active == 0, field)
    im = ax.pcolormesh(masked, cmap=cmap, edgecolors='gray', linewidth=0.3)
    barrier = np.ma.masked_where(active == 1, np.ones_like(field))
    ax.pcolormesh(barrier, cmap='Greys', vmin=0, vmax=2, alpha=0.5)
    fig.colorbar(im, ax=ax, shrink=0.8)
    ax.set_title(title)
    ax.set_xlabel('i')
    ax.set_ylabel('j')
    ax.set_aspect('equal')
    for name, (wi, wj) in wells.items():
        color = 'blue' if 'INJ' in name else 'red'
        ax.plot(wi + 0.5, wj + 0.5, 'o', color=color, markersize=8)
        ax.annotate(name, (wi + 0.5, wj + 0.5), fontsize=7,
                    ha='center', va='bottom', color=color)

fig.suptitle('VAL-021: два независимых резервуара с барьером')
plt.tight_layout()
plt.savefig('results/val-021/two_reservoirs.png', dpi=150)
plt.show()
