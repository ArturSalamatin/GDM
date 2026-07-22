"""
Визуализация VAL-009: 2D-эталон vs 3D (один активный слой).

Использование:
    python scripts/plot_val009.py --results-dir results/val-009

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

    ref = np.genfromtxt(os.path.join(args.results_dir, "reference_2d.csv"),
                        delimiter=",", skip_header=1)
    l3d = np.genfromtxt(os.path.join(args.results_dir, "layer2_3d.csv"),
                        delimiter=",", skip_header=1)
    diff = np.genfromtxt(os.path.join(args.results_dir, "difference.csv"),
                         delimiter=",", skip_header=1)

    nx = int(ref[:, 0].max()) + 1
    ny = int(ref[:, 1].max()) + 1

    P_2d = ref[:, 2].reshape(ny, nx)
    Sw_2d = ref[:, 3].reshape(ny, nx)
    P_3d = l3d[:, 2].reshape(ny, nx)
    Sw_3d = l3d[:, 3].reshape(ny, nx)
    dP = diff[:, 2].reshape(ny, nx)
    dSw = diff[:, 3].reshape(ny, nx)

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))

    im = axes[0, 0].imshow(P_2d, origin="lower", cmap="viridis", aspect="auto")
    axes[0, 0].set_title("P [atm] — 2D reference")
    plt.colorbar(im, ax=axes[0, 0])

    im = axes[0, 1].imshow(P_3d, origin="lower", cmap="viridis", aspect="auto")
    axes[0, 1].set_title("P [atm] — 3D layer k=2")
    plt.colorbar(im, ax=axes[0, 1])

    im = axes[0, 2].imshow(dP, origin="lower", cmap="RdBu_r", aspect="auto")
    axes[0, 2].set_title("ΔP [atm] (3D − 2D)")
    plt.colorbar(im, ax=axes[0, 2], format="%.2e")

    im = axes[1, 0].imshow(Sw_2d, origin="lower", cmap="Blues", aspect="auto")
    axes[1, 0].set_title("Sw — 2D reference")
    plt.colorbar(im, ax=axes[1, 0])

    im = axes[1, 1].imshow(Sw_3d, origin="lower", cmap="Blues", aspect="auto")
    axes[1, 1].set_title("Sw — 3D layer k=2")
    plt.colorbar(im, ax=axes[1, 1])

    im = axes[1, 2].imshow(dSw, origin="lower", cmap="RdBu_r", aspect="auto")
    axes[1, 2].set_title("ΔSw (3D − 2D)")
    plt.colorbar(im, ax=axes[1, 2], format="%.2e")

    fig.suptitle("VAL-009: Single active layer (k=2) vs 2D reference", fontsize=14)
    plt.tight_layout()

    out_path = os.path.join(args.results_dir, "val009_comparison.png")
    plt.savefig(out_path, dpi=150)
    print(f"Saved: {out_path}")
    plt.show()


if __name__ == "__main__":
    main()
