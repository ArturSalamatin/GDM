"""
Визуализация VAL-022: 3 слоя (k=0 inactive, k=1 active, k=2 active).
Перфорации заданы во всех 3 слоях — k=0 отфильтрован RemovePerfsAtInactiveCells.

Использование:
    python scripts/plot_val022.py --results-dir results/val-022

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

    layers = {}
    for k in range(3):
        path = os.path.join(args.results_dir, f"layer_{k}.csv")
        layers[k] = np.genfromtxt(path, delimiter=",", skip_header=1)

    nx = int(layers[0][:, 0].max()) + 1
    ny = int(layers[0][:, 1].max()) + 1

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    titles = ["k=0 (inactive)", "k=1 (active)", "k=2 (active)"]

    for col, k in enumerate(range(3)):
        Sw = layers[k][:, 2].reshape(ny, nx)
        P = layers[k][:, 3].reshape(ny, nx)

        im = axes[0, col].imshow(P, origin="lower", cmap="viridis", aspect="auto")
        axes[0, col].set_title(f"P [atm] — {titles[col]}")
        plt.colorbar(im, ax=axes[0, col])

        im = axes[1, col].imshow(Sw, origin="lower", cmap="Blues", aspect="auto",
                                 vmin=0, vmax=1)
        axes[1, col].set_title(f"Sw — {titles[col]}")
        plt.colorbar(im, ax=axes[1, col])

    fig.suptitle("VAL-022: Inactive top layer — perf in all layers filtered", fontsize=14)
    plt.tight_layout()

    out_path = os.path.join(args.results_dir, "val022_comparison.png")
    plt.savefig(out_path, dpi=150)
    print(f"Saved: {out_path}")
    plt.show()


if __name__ == "__main__":
    main()
