"""
VAL-004: Five-spot grid convergence visualization.
Log-log convergence plot + Sw field comparison.

Usage:
    python scripts/plot_five_spot_convergence.py
"""
import csv
import numpy as np
import matplotlib.pyplot as plt


def load_convergence():
    data = {'N': [], 'h': [], 'L2': [], 'p': []}
    with open('results/validation/five_spot_convergence.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data['N'].append(int(row['N']))
            data['h'].append(float(row['h']))
            data['L2'].append(float(row['L2']))
            data['p'].append(float(row['p']) if row['p'] else None)
    return data


def load_sw_field(N):
    sw = np.zeros((N, N))
    with open(f'results/validation/five_spot_Sw_N{N}.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            i, j = int(row['i']), int(row['j'])
            sw[j, i] = float(row['Sw'])
    return sw


def plot_convergence(data):
    h = np.array(data['h'])
    L2 = np.array(data['L2'])

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.loglog(h, L2, 'ko-', markersize=8, label='GDM (upstream)')

    h_ref = np.array([h[-1], h[0]])
    L2_ref_1 = L2[-1] * (h_ref / h[-1]) ** 1.0
    L2_ref_05 = L2[-1] * (h_ref / h[-1]) ** 0.5
    ax.loglog(h_ref, L2_ref_1, 'r--', alpha=0.5, label='slope = 1')
    ax.loglog(h_ref, L2_ref_05, 'b--', alpha=0.5, label='slope = 0.5')

    for i in range(len(h)):
        label = f'N={data["N"][i]}'
        if data['p'][i] is not None:
            label += f', p={data["p"][i]:.2f}'
        ax.annotate(label, (h[i], L2[i]), textcoords="offset points",
                    xytext=(10, 5), fontsize=9)

    ax.set_xlabel('h (m)', fontsize=12)
    ax.set_ylabel('L2 error', fontsize=12)
    ax.set_title('VAL-004: Grid convergence — five-spot (self-convergence)',
                 fontsize=13)
    ax.legend(fontsize=11)
    ax.grid(True, which='both', alpha=0.3)
    plt.tight_layout()
    plt.savefig('results/validation/five_spot_convergence.png', dpi=150)
    plt.show()


def plot_fields():
    grids = [21, 41, 81, 161]
    fig, axes = plt.subplots(1, 4, figsize=(20, 5))

    for ax, N in zip(axes, grids):
        sw = load_sw_field(N)
        im = ax.imshow(sw, origin='lower', cmap='Blues',
                       vmin=0, vmax=1, extent=[0, 500, 0, 500],
                       aspect='equal')
        ax.set_title(f'$S_w$, {N}x{N}', fontsize=12)
        ax.set_xlabel('x, m')
    axes[0].set_ylabel('y, m')
    fig.colorbar(im, ax=axes, shrink=0.8, label='$S_w$')

    fig.suptitle('VAL-004: Sw field at t=500 days', fontsize=14)
    fig.tight_layout(rect=[0, 0, 0.92, 0.95])
    plt.savefig('results/validation/five_spot_fields.png', dpi=150)
    plt.show()


if __name__ == '__main__':
    data = load_convergence()
    plot_convergence(data)
    plot_fields()
