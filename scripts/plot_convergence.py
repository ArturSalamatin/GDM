"""
Визуализация сеточной сходимости: сравнение полей на разных сетках
+ графики сходимости интегральных величин.

Использование:
    python scripts/plot_convergence.py --results-dir results/convergence
"""

import argparse
import json
import glob
import os

import numpy as np
import matplotlib.pyplot as plt


def load_convergence_csv(results_dir):
    return np.genfromtxt(
        os.path.join(results_dir, "convergence.csv"),
        delimiter=",", names=True)


def load_final_snapshot(case_dir, nx, ny):
    files = sorted(glob.glob(os.path.join(case_dir, "snapshot_*.csv")))
    if not files:
        return None, None
    data = np.genfromtxt(files[-1], delimiter=",", skip_header=1)
    sw = data[:, 2].reshape(ny, nx)
    p_atm = data[:, 3].reshape(ny, nx)
    return sw, p_atm


def plot_fields_comparison(results_dir, dpi=120):
    conv = load_convergence_csv(results_dir)
    grids = conv["N"].astype(int)

    fig, axes = plt.subplots(2, len(grids), figsize=(5 * len(grids), 8))

    for col, n in enumerate(grids):
        case_dir = os.path.join(
            os.path.dirname(results_dir),
            f"single_injector_{n}x{n}")
        with open(os.path.join(case_dir, "metadata.json")) as f:
            meta = json.load(f)
        lx, ly = meta["Lx"], meta["Ly"]

        sw, p = load_final_snapshot(case_dir, n, n)
        if sw is None:
            continue

        ax = axes[0, col]
        im = ax.imshow(sw, origin="lower", cmap="Blues",
                       vmin=0, vmax=1, extent=[0, lx, 0, ly], aspect="equal")
        ax.set_title(f"$S_w$, {n}×{n}")
        if col == 0:
            ax.set_ylabel("y, м")
        fig.colorbar(im, ax=ax, shrink=0.8)

        p_all = conv["P_probe_atm"]
        ax = axes[1, col]
        im = ax.imshow(p, origin="lower", cmap="RdYlBu_r",
                       extent=[0, lx, 0, ly], aspect="equal")
        ax.set_title(f"P, атм, {n}×{n}")
        ax.set_xlabel("x, м")
        if col == 0:
            ax.set_ylabel("y, м")
        fig.colorbar(im, ax=ax, shrink=0.8)

    fig.suptitle("Финальное состояние (t=200 дней) на разных сетках", fontsize=14)
    fig.tight_layout(rect=[0, 0, 1, 0.95])

    out = os.path.join(results_dir, "fields_comparison.png")
    fig.savefig(out, dpi=dpi, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close(fig)


def plot_convergence_curves(results_dir, dpi=120):
    conv = load_convergence_csv(results_dir)
    h = conv["hx"]
    N = conv["N"].astype(int)

    fig, axes = plt.subplots(1, 3, figsize=(14, 4.5))

    ax = axes[0]
    ax.plot(h, conv["oil_mass"] / 1e6, "ko-", markersize=6)
    ax.set_xlabel("hx, м")
    ax.set_ylabel("Масса нефти, 10⁶ кг")
    ax.set_title("Oil mass(h)")
    ax.invert_xaxis()
    ax.grid(True, alpha=0.3)
    for i, n in enumerate(N):
        ax.annotate(f"{n}×{n}", (h[i], conv["oil_mass"][i] / 1e6),
                    textcoords="offset points", xytext=(5, 5), fontsize=8)

    ax = axes[1]
    ax.plot(h, conv["Sw_probe"], "bs-", markersize=6)
    ax.set_xlabel("hx, м")
    ax.set_ylabel("$S_w$ (probe)")
    ax.set_title("$S_w$ at probe point")
    ax.invert_xaxis()
    ax.grid(True, alpha=0.3)

    ax = axes[2]
    ax.plot(h, conv["P_probe_atm"], "r^-", markersize=6)
    ax.set_xlabel("hx, м")
    ax.set_ylabel("P, атм (probe)")
    ax.set_title("P at probe point")
    ax.invert_xaxis()
    ax.grid(True, alpha=0.3)

    fig.suptitle("Сеточная сходимость (t=200 дней)", fontsize=14)
    fig.tight_layout(rect=[0, 0, 1, 0.93])

    out = os.path.join(results_dir, "convergence_curves.png")
    fig.savefig(out, dpi=dpi, bbox_inches="tight")
    print(f"Saved: {out}")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Визуализация сеточной сходимости GDM")
    parser.add_argument("--results-dir", default="results/convergence")
    parser.add_argument("--dpi", type=int, default=120)
    args = parser.parse_args()

    plot_fields_comparison(args.results_dir, dpi=args.dpi)
    plot_convergence_curves(args.results_dir, dpi=args.dpi)


if __name__ == "__main__":
    main()
