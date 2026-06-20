"""
Анимация полей давления и насыщенности для 3D-задач (несколько пластов).

Использование:
    python scripts/animate_3d_fields.py --results-dir build/results/3d_smoke

Читает snapshot_NNN_layer_K.csv, metadata.json, mass_balance.csv.
Каждый кадр — сетка 2×Nz: верхний ряд S_w, нижний P по каждому пласту.

Зависимости: numpy, matplotlib
Для MP4: ffmpeg в PATH
"""

import argparse
import json
import glob
import os
import re

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation


def load_metadata(results_dir):
    with open(os.path.join(results_dir, "metadata.json")) as f:
        return json.load(f)


def load_snapshots_3d(results_dir, meta):
    nx, ny = meta["Nx"], meta["Ny"]
    nz = meta.get("Nz", 1)

    pattern = os.path.join(results_dir, "snapshot_*_layer_*.csv")
    files = sorted(glob.glob(pattern))

    by_step = {}
    for fpath in files:
        m = re.search(r"snapshot_(\d+)_layer_(\d+)\.csv", fpath)
        if not m:
            continue
        step, layer = int(m.group(1)), int(m.group(2))
        data = np.genfromtxt(fpath, delimiter=",", skip_header=1)
        sw = data[:, 2].reshape(ny, nx)
        p_atm = data[:, 3].reshape(ny, nx)
        by_step.setdefault(step, {})[layer] = (sw, p_atm)

    snapshots = []
    for step in sorted(by_step.keys()):
        layers = by_step[step]
        sw_layers = [layers[k][0] for k in range(nz)]
        p_layers = [layers[k][1] for k in range(nz)]
        snapshots.append((sw_layers, p_layers))

    return snapshots


def load_mass_balance(results_dir):
    fpath = os.path.join(results_dir, "mass_balance.csv")
    if not os.path.exists(fpath):
        return None
    return np.genfromtxt(fpath, delimiter=",", names=True)


def animate_fields_3d(results_dir, output=None, fps=4, dpi=100):
    meta = load_metadata(results_dir)
    snapshots = load_snapshots_3d(results_dir, meta)
    times = meta["save_times"]

    nx, ny = meta["Nx"], meta["Ny"]
    nz = meta.get("Nz", 1)
    lx, ly = meta["Lx"], meta["Ly"]

    p_all = np.array([p for _, p_layers in snapshots for p in p_layers])
    p_min, p_max = p_all.min(), p_all.max()
    if p_min == p_max:
        p_min -= 1
        p_max += 1

    # GridSpec: nz столбцов для данных + 1 узкий столбец для colorbar
    ncols = nz + 1
    width_ratios = [1] * nz + [0.05]
    fig_w = max(7.0, 3.5 * nz + 0.8)
    fig = plt.figure(figsize=(fig_w, 7))
    right_margin = 0.88 if nz <= 2 else 0.95
    gs = fig.add_gridspec(2, ncols, width_ratios=width_ratios,
                          wspace=0.35, hspace=0.30,
                          left=0.08, right=right_margin, top=0.92, bottom=0.06)
    title_text = fig.suptitle(f"t = {times[0]:.0f} дней", fontsize=14)

    axes_sw = [fig.add_subplot(gs[0, k]) for k in range(nz)]
    axes_p = [fig.add_subplot(gs[1, k]) for k in range(nz)]
    cax_sw = fig.add_subplot(gs[0, nz])
    cax_p = fig.add_subplot(gs[1, nz])

    ims_sw = []
    ims_p = []

    for k in range(nz):
        im_sw = axes_sw[k].imshow(
            snapshots[0][0][k], origin="lower", cmap="Blues",
            vmin=0, vmax=1, extent=[0, lx, 0, ly], aspect="equal")
        axes_sw[k].set_title(f"$S_w$, пласт {k}", fontsize=10)
        if k == 0:
            axes_sw[k].set_ylabel("y, м")
        else:
            axes_sw[k].set_yticklabels([])
        axes_sw[k].set_xticklabels([])
        ims_sw.append(im_sw)

        im_p = axes_p[k].imshow(
            snapshots[0][1][k], origin="lower", cmap="RdYlBu_r",
            vmin=p_min, vmax=p_max, extent=[0, lx, 0, ly], aspect="equal")
        axes_p[k].set_title(f"P, пласт {k}", fontsize=10)
        axes_p[k].set_xlabel("x, м")
        if k == 0:
            axes_p[k].set_ylabel("y, м")
        else:
            axes_p[k].set_yticklabels([])
        ims_p.append(im_p)

    fig.colorbar(ims_sw[0], cax=cax_sw, label="$S_w$")
    fig.colorbar(ims_p[0], cax=cax_p, label="P, атм")

    if "wells" in meta:
        for w in meta["wells"]:
            marker = "v" if w["type"] == "injector" else "^"
            color = "b" if w["type"] == "injector" else "r"
            for k in range(nz):
                for ax in (axes_sw[k], axes_p[k]):
                    ax.plot(w["x"], w["y"], marker,
                            color=color, markersize=7,
                            markeredgecolor="k", zorder=5)

    def update(frame):
        sw_layers, p_layers = snapshots[frame]
        for k in range(nz):
            ims_sw[k].set_data(sw_layers[k])
            ims_p[k].set_data(p_layers[k])
        title_text.set_text(f"t = {times[frame]:.0f} дней")
        return ims_sw + ims_p

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
        description="Визуализация 3D-результатов GDM: анимация по слоям + баланс масс")
    parser.add_argument("--results-dir", required=True,
                        help="Папка с snapshot_NNN_layer_K.csv и metadata.json")
    parser.add_argument("--fps", type=int, default=4)
    parser.add_argument("--dpi", type=int, default=100)
    args = parser.parse_args()

    animate_fields_3d(args.results_dir, fps=args.fps, dpi=args.dpi)
    plot_mass_balance(args.results_dir, dpi=args.dpi)


if __name__ == "__main__":
    main()
