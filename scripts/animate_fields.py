"""
Анимация полей давления и насыщенности из результатов GDM.

Использование:
    python scripts/animate_fields.py --results-dir results/single_injector

Зависимости: numpy, matplotlib
Для MP4: ffmpeg в PATH
"""

import argparse
import json
import glob
import os

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation


def load_metadata(results_dir):
    with open(os.path.join(results_dir, "metadata.json")) as f:
        return json.load(f)


def load_snapshots(results_dir, meta):
    nx, ny = meta["Nx"], meta["Ny"]
    pattern = os.path.join(results_dir, "snapshot_*.csv")
    files = sorted(glob.glob(pattern))

    snapshots = []
    for fpath in files:
        data = np.genfromtxt(fpath, delimiter=",", skip_header=1)
        sw = data[:, 2].reshape(ny, nx)
        p_atm = data[:, 3].reshape(ny, nx)
        snapshots.append((sw, p_atm))
    return snapshots


def load_mass_balance(results_dir):
    fpath = os.path.join(results_dir, "mass_balance.csv")
    if not os.path.exists(fpath):
        return None
    return np.genfromtxt(fpath, delimiter=",", names=True)


def animate_fields(results_dir, output=None, fps=4, dpi=120):
    meta = load_metadata(results_dir)
    snapshots = load_snapshots(results_dir, meta)
    times = meta["save_times"]

    nx, ny = meta["Nx"], meta["Ny"]
    lx, ly = meta["Lx"], meta["Ly"]
    hx, hy = lx / nx, ly / ny

    x = np.linspace(hx / 2, lx - hx / 2, nx)
    y = np.linspace(hy / 2, ly - hy / 2, ny)

    p_all = np.array([s[1] for s in snapshots])
    p_min, p_max = p_all.min(), p_all.max()
    if p_min == p_max:
        p_min -= 1
        p_max += 1

    fig, (ax_sw, ax_p) = plt.subplots(1, 2, figsize=(12, 5))
    fig.suptitle(f"t = {times[0]:.0f} дней", fontsize=14)

    im_sw = ax_sw.imshow(
        snapshots[0][0], origin="lower", cmap="Blues",
        vmin=0, vmax=1, extent=[0, lx, 0, ly], aspect="equal")
    ax_sw.set_title("$S_w$")
    ax_sw.set_xlabel("x, м")
    ax_sw.set_ylabel("y, м")
    fig.colorbar(im_sw, ax=ax_sw, shrink=0.8)

    im_p = ax_p.imshow(
        snapshots[0][1], origin="lower", cmap="RdYlBu_r",
        vmin=p_min, vmax=p_max, extent=[0, lx, 0, ly], aspect="equal")
    ax_p.set_title("P, атм")
    ax_p.set_xlabel("x, м")
    ax_p.set_ylabel("y, м")
    fig.colorbar(im_p, ax=ax_p, shrink=0.8)

    if "wells" in meta:
        for w in meta["wells"]:
            marker = "v" if w["type"] == "injector" else "^"
            color = "b" if w["type"] == "injector" else "r"
            for ax in (ax_sw, ax_p):
                ax.plot(w["x"], w["y"], marker,
                        color=color, markersize=10, markeredgecolor="k", zorder=5)
    elif "well_x" in meta and "well_y" in meta:
        for ax in (ax_sw, ax_p):
            ax.plot(meta["well_x"], meta["well_y"], "v",
                    color="b", markersize=10, markeredgecolor="k", zorder=5)

    fig.tight_layout(rect=[0, 0, 1, 0.95])

    def update(frame):
        sw, p = snapshots[frame]
        im_sw.set_data(sw)
        im_p.set_data(p)
        fig.suptitle(f"t = {times[frame]:.0f} дней", fontsize=14)
        return im_sw, im_p

    anim = animation.FuncAnimation(
        fig, update, frames=len(snapshots), interval=1000 // fps, blit=False)

    if output is None:
        output = os.path.join(results_dir, "animation")

    try:
        fname = output + ".mp4"
        anim.save(fname, writer="ffmpeg", fps=fps, dpi=dpi)
        print(f"Saved: {fname}")
    except Exception:
        fname = output + ".gif"
        anim.save(fname, writer="pillow", fps=fps, dpi=dpi)
        print(f"Saved: {fname}")

    plt.close(fig)


def plot_mass_balance(results_dir, output=None, dpi=120):
    bal = load_mass_balance(results_dir)
    if bal is None:
        print("No mass_balance.csv found, skipping balance plot.")
        return

    t = bal["t"]

    fig, axes = plt.subplots(2, 2, figsize=(12, 8))

    ax = axes[0, 0]
    ax.plot(t, bal["oil_mass"] / 1e6, "k-o", markersize=3, label="Oil")
    ax.plot(t, bal["water_mass"] / 1e6, "b-s", markersize=3, label="Water")
    ax.set_ylabel("Масса, 10⁶ кг")
    ax.set_title("Масса фаз в пласте")
    ax.legend()
    ax.grid(True, alpha=0.3)

    ax = axes[0, 1]
    ax.plot(t, bal["oil_residual"], "k-o", markersize=3, label="Oil residual")
    ax.plot(t, bal["water_residual"], "b-s", markersize=3, label="Water residual")
    ax.set_ylabel("Невязка, кг")
    ax.set_title("Невязка баланса масс")
    ax.legend()
    ax.grid(True, alpha=0.3)

    ax = axes[1, 0]
    ax.plot(t, bal["accumOil"] / 1e3, "k-o", markersize=3, label="ΔM oil")
    ax.plot(t, bal["accumOilOutFlux"] / 1e3, "r--", label="Oil outflux")
    ax.plot(t, bal["accumOilDebet"] / 1e3, "g:", label="Oil debet")
    ax.set_xlabel("t, дней")
    ax.set_ylabel("10³ кг")
    ax.set_title("Нефтяной баланс (компоненты)")
    ax.legend()
    ax.grid(True, alpha=0.3)

    ax = axes[1, 1]
    ax.plot(t, bal["accumWater"] / 1e3, "b-o", markersize=3, label="ΔM water")
    ax.plot(t, bal["accumWaterOutFlux"] / 1e3, "r--", label="Water outflux")
    ax.plot(t, bal["accumWaterDebet"] / 1e3, "g:", label="Water debet")
    ax.set_xlabel("t, дней")
    ax.set_ylabel("10³ кг")
    ax.set_title("Водный баланс (компоненты)")
    ax.legend()
    ax.grid(True, alpha=0.3)

    fig.tight_layout()

    if output is None:
        output = os.path.join(results_dir, "mass_balance.png")
    fig.savefig(output, dpi=dpi, bbox_inches="tight")
    print(f"Saved: {output}")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Визуализация результатов GDM: анимация полей + баланс масс")
    parser.add_argument("--results-dir", required=True,
                        help="Папка с snapshot_*.csv и metadata.json")
    parser.add_argument("--fps", type=int, default=4)
    parser.add_argument("--dpi", type=int, default=120)
    args = parser.parse_args()

    animate_fields(args.results_dir, fps=args.fps, dpi=args.dpi)
    plot_mass_balance(args.results_dir, dpi=args.dpi)


if __name__ == "__main__":
    main()
