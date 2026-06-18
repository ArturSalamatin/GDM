"""
Визуализация линий тока и поля скоростей.

Использование:
    python scripts/plot_streamlines.py build/results/streamlines_single_injector
    python scripts/plot_streamlines.py build/results/streamlines_two_well
"""
import sys
import json
import pathlib

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch


def load_data(result_dir: pathlib.Path):
    streamlines = pd.read_csv(result_dir / "streamlines.csv")
    velocity = pd.read_csv(result_dir / "velocity_field.csv")
    with open(result_dir / "metadata.json") as f:
        meta = json.load(f)
    return streamlines, velocity, meta


def plot(result_dir: pathlib.Path):
    sl, vel, meta = load_data(result_dir)
    nx, ny = meta["nx"], meta["ny"]
    lx, ly = meta["lx"], meta["ly"]

    fig, axes = plt.subplots(1, 2, figsize=(16, 7))

    # --- Left: streamlines colored by time ---
    ax = axes[0]
    t_max = sl["t"].max() if "t" in sl.columns else 1.0
    cmap = plt.cm.viridis
    for tid, group in sl.groupby("trajectory_id"):
        if "t" in group.columns and t_max > 0:
            t_norm = group["t"].values / t_max
            for k in range(len(group) - 1):
                ax.plot(group["x"].iloc[k:k+2], group["y"].iloc[k:k+2],
                        linewidth=0.9, color=cmap(t_norm[k]), alpha=0.8)
        else:
            ax.plot(group["x"], group["y"], linewidth=0.8, color="steelblue", alpha=0.7)
        if len(group) > 1:
            ax.annotate("",
                        xy=(group["x"].iloc[-1], group["y"].iloc[-1]),
                        xytext=(group["x"].iloc[-2], group["y"].iloc[-2]),
                        arrowprops=dict(arrowstyle="->", color="steelblue", lw=1.2))
    if "t" in sl.columns:
        sm = plt.cm.ScalarMappable(cmap=cmap, norm=plt.Normalize(0, t_max))
        plt.colorbar(sm, ax=ax, label="time, days")

    for w in meta["wells"]:
        marker = "v" if w["name"] in ("INJ",) else "^"
        color = "blue" if w["name"] in ("INJ",) else "red"
        ax.plot(w["x"], w["y"], marker=marker, markersize=12,
                color=color, zorder=5, label=w["name"])

    ax.set_xlim(0, lx)
    ax.set_ylim(0, ly)
    ax.set_aspect("equal")
    ax.set_xlabel("x, m")
    ax.set_ylabel("y, m")
    end_t = meta.get("end_time", meta.get("fixed_time", 0))
    mode = meta.get("mode", "snapshot")
    ax.set_title(f"Streamlines ({mode}, t = 0..{end_t:.0f} days)")
    ax.legend()
    ax.grid(True, alpha=0.3)

    # --- Right: velocity quiver ---
    ax = axes[1]
    X = vel["x"].values.reshape(ny, nx)
    Y = vel["y"].values.reshape(ny, nx)
    VX = vel["vx"].values.reshape(ny, nx)
    VY = vel["vy"].values.reshape(ny, nx)
    speed = np.sqrt(VX**2 + VY**2)

    skip = max(1, nx // 20)
    s = slice(None, None, skip)
    q = ax.quiver(X[s, s], Y[s, s], VX[s, s], VY[s, s],
                  speed[s, s], cmap="viridis", scale_units="xy", alpha=0.8)
    plt.colorbar(q, ax=ax, label="speed, m/day")

    for w in meta["wells"]:
        marker = "v" if w["name"] in ("INJ",) else "^"
        color = "blue" if w["name"] in ("INJ",) else "red"
        ax.plot(w["x"], w["y"], marker=marker, markersize=12,
                color=color, zorder=5, label=w["name"])

    ax.set_xlim(0, lx)
    ax.set_ylim(0, ly)
    ax.set_aspect("equal")
    ax.set_xlabel("x, m")
    ax.set_ylabel("y, m")
    ax.set_title("Velocity field (last snapshot)")
    ax.legend()
    ax.grid(True, alpha=0.3)

    fig.suptitle(meta["case"], fontsize=14)
    fig.tight_layout()

    out_path = result_dir / "streamlines.png"
    fig.savefig(out_path, dpi=150)
    print(f"Saved: {out_path}")
    plt.show()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    plot(pathlib.Path(sys.argv[1]))
