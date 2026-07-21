"""Log-log convergence plot for BL grid refinement study (VAL-002)."""
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

data = {'Nx': [], 'hx': [], 'L2': [], 'p': []}
with open('results/validation/bl_convergence.csv') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data['Nx'].append(int(row['Nx']))
        data['hx'].append(float(row['hx']))
        data['L2'].append(float(row['L2']))
        data['p'].append(float(row['p']) if row['p'] else None)

hx = np.array(data['hx'])
L2 = np.array(data['L2'])

fig, ax = plt.subplots(figsize=(8, 6))
ax.loglog(hx, L2, 'ko-', markersize=8, label='GDM (upstream)')

# Reference slope: order 1
h_ref = np.array([hx[-1], hx[0]])
L2_ref = L2[-1] * (h_ref / hx[-1]) ** 1.0
ax.loglog(h_ref, L2_ref, 'r--', alpha=0.5, label='slope = 1 (reference)')

for i in range(len(hx)):
    label = f'Nx={data["Nx"][i]}'
    if data['p'][i] is not None:
        label += f', p={data["p"][i]:.2f}'
    ax.annotate(label, (hx[i], L2[i]), textcoords="offset points",
                xytext=(10, 5), fontsize=9)

ax.set_xlabel('h (м)', fontsize=12)
ax.set_ylabel('L2 error', fontsize=12)
ax.set_title('VAL-002: Grid convergence — 1D Buckley–Leverett', fontsize=13)
ax.legend(fontsize=11)
ax.grid(True, which='both', alpha=0.3)
plt.tight_layout()
plt.savefig('results/validation/bl_convergence.png', dpi=150)
print('Saved results/validation/bl_convergence.png')
