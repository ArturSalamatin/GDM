"""Log-log convergence plot for radial BL grid refinement study (VAL-002)."""
import csv
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
vdir = os.path.join(root, 'results', 'validation')

data = {'N': [], 'h': [], 'L2': [], 'p': []}
with open(os.path.join(vdir, 'radial_bl_convergence.csv')) as f:
    reader = csv.DictReader(f)
    for row in reader:
        data['N'].append(int(row['N']))
        data['h'].append(float(row['h']))
        data['L2'].append(float(row['L2']))
        data['p'].append(float(row['p']) if row['p'] else None)

h = np.array(data['h'])
L2 = np.array(data['L2'])

fig, ax = plt.subplots(figsize=(8, 6))
ax.loglog(h, L2, 'ko-', markersize=8, label='GDM (upstream)')

h_ref = np.array([h[-1], h[0]])
L2_ref = L2[-1] * (h_ref / h[-1]) ** 1.0
ax.loglog(h_ref, L2_ref, 'r--', alpha=0.5, label='slope = 1 (reference)')

for i in range(len(h)):
    label = f'N={data["N"][i]}'
    if data['p'][i] is not None:
        label += f', p={data["p"][i]:.2f}'
    ax.annotate(label, (h[i], L2[i]), textcoords="offset points",
                xytext=(10, 5), fontsize=9)

ax.set_xlabel('h (m)', fontsize=12)
ax.set_ylabel('L2 error', fontsize=12)
ax.set_title('VAL-002: Grid convergence — radial Buckley–Leverett', fontsize=13)
ax.legend(fontsize=11)
ax.grid(True, which='both', alpha=0.3)
plt.tight_layout()
out = os.path.join(vdir, 'radial_bl_convergence.png')
plt.savefig(out, dpi=150)
print(f'Saved {out}')
