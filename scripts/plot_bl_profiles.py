"""Overlay plot: Sw(r) radial profiles for all grids + reference N=321 (VAL-002)."""
import csv
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
vdir = os.path.join(root, 'results', 'validation')

grids = [21, 41, 81, 161]
colors = ['#d62728', '#ff7f0e', '#2ca02c', '#1f77b4']

fig, ax = plt.subplots(figsize=(10, 6))

r_ref, sw_ref = [], []
with open(os.path.join(vdir, 'radial_bl_profile_N321_ref.csv')) as f:
    reader = csv.DictReader(f)
    for row in reader:
        r_ref.append(float(row['r']))
        sw_ref.append(float(row['Sw']))
ax.plot(r_ref, sw_ref, 'k-', linewidth=2, label='Reference N=321', alpha=0.7)

for N, color in zip(grids, colors):
    r, sw_gdm = [], []
    with open(os.path.join(vdir, f'radial_bl_profile_N{N}.csv')) as f:
        reader = csv.DictReader(f)
        for row in reader:
            r.append(float(row['r']))
            sw_gdm.append(float(row['Sw_GDM']))
    ax.plot(r, sw_gdm, '-', color=color, linewidth=1.5,
            label=f'GDM N={N}', alpha=0.8)

ax.set_xlabel('r (m)', fontsize=12)
ax.set_ylabel('Sw', fontsize=12)
ax.set_title('VAL-002: Radial Sw(r) profiles — grid refinement', fontsize=13)
ax.legend(fontsize=9, ncol=2)
ax.grid(True, alpha=0.3)
plt.tight_layout()
out = os.path.join(vdir, 'radial_bl_profiles_overlay.png')
plt.savefig(out, dpi=150)
print(f'Saved {out}')
