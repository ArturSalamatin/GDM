"""Overlay plot: Sw(x) profiles for all grids + analytical (VAL-002)."""
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

grids = [25, 50, 100, 200]
colors = ['#d62728', '#ff7f0e', '#2ca02c', '#1f77b4']

fig, ax = plt.subplots(figsize=(10, 6))

for Nx, color in zip(grids, colors):
    x, sw_gdm, sw_ana = [], [], []
    with open(f'results/validation/bl_profile_Nx{Nx}.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            x.append(float(row['x']))
            sw_gdm.append(float(row['Sw_GDM']))
            sw_ana.append(float(row['Sw_analytical']))
    ax.plot(x, sw_gdm, '-', color=color, linewidth=1.5,
            label=f'GDM Nx={Nx}', alpha=0.8)

# Analytical from finest grid
x_ana, sw_ana = [], []
with open('results/validation/bl_profile_Nx200.csv') as f:
    reader = csv.DictReader(f)
    for row in reader:
        x_ana.append(float(row['x']))
        sw_ana.append(float(row['Sw_analytical']))
ax.plot(x_ana, sw_ana, 'k--', linewidth=2, label='Analytical (BL)')

ax.set_xlabel('x (м)', fontsize=12)
ax.set_ylabel('Sw', fontsize=12)
ax.set_title('VAL-002: Sw profiles — grid refinement', fontsize=13)
ax.legend(fontsize=10)
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig('results/validation/bl_profiles_overlay.png', dpi=150)
print('Saved results/validation/bl_profiles_overlay.png')
