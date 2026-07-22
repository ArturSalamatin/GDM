"""
Визуализация барьера из неактивных ячеек (VAL-008).

Использование:
    python scripts/plot_barrier.py --results-dir results/val-008

Зависимости: numpy, matplotlib
"""
import argparse
import os

import numpy as np
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", required=True)
    args = parser.parse_args()

    data = np.genfromtxt(
        os.path.join(args.results_dir, "barrier.csv"),
        delimiter=",", skip_header=1)

    i = data[:, 0].astype(int)
    j = data[:, 1].astype(int)
    active = data[:, 2].astype(int)
    P_atm = data[:, 3]
    Sw = data[:, 4]

    nx = i.max() + 1
    ny = j.max() + 1

    active_map = active.reshape(ny, nx)
    P_map = P_atm.reshape(ny, nx)
    Sw_map = Sw.reshape(ny, nx)

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    ax = axes[0]
    im = ax.imshow(active_map, origin="lower", cmap="RdYlGn",
                   vmin=0, vmax=1, aspect="auto")
    ax.set_title("Active cells (1=active, 0=inactive)")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    ax = axes[1]
    im = ax.imshow(P_map, origin="lower", cmap="viridis", aspect="auto")
    ax.set_title("Pressure [atm]")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    ax = axes[2]
    im = ax.imshow(Sw_map, origin="lower", cmap="Blues",
                   vmin=0, vmax=1, aspect="auto")
    ax.set_title("Water saturation Sw")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    axes[2].plot(5, 2, "v", color="blue", markersize=10, label="INJ")
    axes[2].legend(loc="upper right")

    plt.suptitle("VAL-008: Barrier of inactive cells (T=30 days)", fontsize=14)
    plt.tight_layout()
    out = os.path.join(args.results_dir, "barrier.png")
    plt.savefig(out, dpi=150)
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
