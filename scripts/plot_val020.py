"""
Визуализация VAL-020: shut-in + restart с закрытой перфорацией (2 слоя).

Использование:
    python scripts/plot_val020.py --results-dir results/val-020

Зависимости: numpy, matplotlib
"""
import argparse
import json
import os
import numpy as np
import matplotlib.pyplot as plt


def load_snapshot(results_dir, step, layer):
    path = os.path.join(results_dir, f"snapshot_{step:03d}_layer_{layer}.csv")
    return np.genfromtxt(path, delimiter=",", skip_header=1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", required=True)
    args = parser.parse_args()

    with open(os.path.join(args.results_dir, "metadata.json")) as f:
        meta = json.load(f)

    nx, ny = meta["Nx"], meta["Ny"]
    times = meta["save_times"]

    targets = [100.0, 150.0, times[-1]]
    indices = []
    for t in targets:
        idx = min(range(len(times)), key=lambda i: abs(times[i] - t))
        indices.append(idx)

    phase_labels = [
        f"Фаза 1 (t={times[indices[0]]:.0f}д)",
        f"Shut-in (t={times[indices[1]]:.0f}д)",
        f"Restart (t={times[indices[2]]:.0f}д)",
    ]

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    layer_titles = ["k=0 (закрыт при t=150)", "k=1 (работает весь период)"]

    for row, k in enumerate(range(2)):
        for col, (step_idx, label) in enumerate(zip(indices, phase_labels)):
            data = load_snapshot(args.results_dir, step_idx, k)
            Sw = data[:, 2].reshape(ny, nx)

            im = axes[row, col].imshow(
                Sw, origin="lower", cmap="Blues", aspect="auto", vmin=0, vmax=1)
            axes[row, col].set_title(f"Sw — {layer_titles[row]}\n{label}")
            plt.colorbar(im, ax=axes[row, col])

    fig.suptitle("VAL-020: shut-in + restart с закрытой перфорацией", fontsize=14)
    fig.tight_layout()
    fig.savefig(os.path.join(args.results_dir, "sw_phases.png"), dpi=150)
    print(f"Сохранено: {os.path.join(args.results_dir, 'sw_phases.png')}")

    bal = np.genfromtxt(
        os.path.join(args.results_dir, "mass_balance.csv"),
        delimiter=",", skip_header=1)
    t = bal[:, 0]
    oil_res = bal[:, 6]
    water_res = bal[:, 10]

    fig2, ax = plt.subplots(figsize=(10, 5))
    ax.plot(t, oil_res, "o-", label="Oil residual", markersize=3)
    ax.plot(t, water_res, "s-", label="Water residual", markersize=3)
    ax.axvline(100, color="gray", linestyle="--", alpha=0.5, label="shut-in start")
    ax.axvline(150, color="red", linestyle="--", alpha=0.5, label="restart (k=0 closed)")
    ax.set_xlabel("Время [дни]")
    ax.set_ylabel("Невязка баланса масс")
    ax.legend()
    ax.set_title("VAL-020: баланс масс по фазам")
    fig2.tight_layout()
    fig2.savefig(os.path.join(args.results_dir, "mass_balance.png"), dpi=150)
    print(f"Сохранено: {os.path.join(args.results_dir, 'mass_balance.png')}")

    data_shutin = load_snapshot(args.results_dir, indices[1], 0)
    data_restart = load_snapshot(args.results_dir, indices[2], 0)
    dSw = (data_restart[:, 2] - data_shutin[:, 2]).reshape(ny, nx)

    fig3, ax3 = plt.subplots(figsize=(7, 6))
    vmax = max(np.max(np.abs(dSw)), 0.001)
    im3 = ax3.imshow(dSw, origin="lower", cmap="RdBu_r", aspect="auto",
                     vmin=-vmax, vmax=vmax)
    ax3.set_title("ΔSw в k=0 (restart − shut-in)\nОжидание: ≈ 0 (перфорация закрыта)")
    plt.colorbar(im3, ax=ax3, label="ΔSw")
    fig3.tight_layout()
    fig3.savefig(os.path.join(args.results_dir, "delta_sw_k0.png"), dpi=150)
    print(f"Сохранено: {os.path.join(args.results_dir, 'delta_sw_k0.png')}")

    plt.show()


if __name__ == "__main__":
    main()
