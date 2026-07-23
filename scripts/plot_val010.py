"""
Визуализация VAL-010: full-grid vs corner-off.

Использование:
    python scripts/plot_val010.py --results-dir results/val-010

Зависимости: numpy, matplotlib
"""
import argparse
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", required=True)
    args = parser.parse_args()

    ref = np.genfromtxt(os.path.join(args.results_dir, "reference_full.csv"),
                        delimiter=",", skip_header=1)
    test = np.genfromtxt(os.path.join(args.results_dir, "corner_off.csv"),
                         delimiter=",", skip_header=1)
    diff = np.genfromtxt(os.path.join(args.results_dir, "difference.csv"),
                         delimiter=",", skip_header=1)

    nx = int(ref[:, 0].max()) + 1
    ny = int(ref[:, 1].max()) + 1

    P_ref = ref[:, 3].reshape(ny, nx)
    Sw_ref = ref[:, 4].reshape(ny, nx)
    P_test = test[:, 3].reshape(ny, nx)
    Sw_test = test[:, 4].reshape(ny, nx)
    active = diff[:, 2].reshape(ny, nx)
    dP = diff[:, 3].reshape(ny, nx)
    dSw = diff[:, 4].reshape(ny, nx)

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))

    im = axes[0, 0].imshow(P_ref, origin="lower", cmap="viridis", aspect="auto")
    axes[0, 0].set_title("P [atm] — full grid")
    plt.colorbar(im, ax=axes[0, 0])

    im = axes[0, 1].imshow(P_test, origin="lower", cmap="viridis", aspect="auto")
    axes[0, 1].set_title("P [atm] — corner (0,0) off")
    plt.colorbar(im, ax=axes[0, 1])
    axes[0, 1].add_patch(Rectangle((-0.5, -0.5), 1, 1,
                                    fill=True, color="black"))

    im = axes[0, 2].imshow(dP, origin="lower", cmap="RdBu_r", aspect="auto")
    axes[0, 2].set_title("ΔP [atm]")
    plt.colorbar(im, ax=axes[0, 2], format="%.2e")

    im = axes[1, 0].imshow(Sw_ref, origin="lower", cmap="Blues", aspect="auto")
    axes[1, 0].set_title("Sw — full grid")
    plt.colorbar(im, ax=axes[1, 0])

    im = axes[1, 1].imshow(Sw_test, origin="lower", cmap="Blues", aspect="auto")
    axes[1, 1].set_title("Sw — corner (0,0) off")
    plt.colorbar(im, ax=axes[1, 1])
    axes[1, 1].add_patch(Rectangle((-0.5, -0.5), 1, 1,
                                    fill=True, color="black"))

    im = axes[1, 2].imshow(dSw, origin="lower", cmap="RdBu_r", aspect="auto")
    axes[1, 2].set_title("ΔSw")
    plt.colorbar(im, ax=axes[1, 2], format="%.2e")

    fig.suptitle("VAL-010: Corner cell (0,0) inactive vs full grid", fontsize=14)
    plt.tight_layout()

    out_path = os.path.join(args.results_dir, "val010_comparison.png")
    plt.savefig(out_path, dpi=150)
    print(f"Saved: {out_path}")
    plt.show()


if __name__ == "__main__":
    main()
