"""Plot native joint-medium diagnostic samples, independent RTE and costs."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import RAY, SAMPLE
from linear_medium_reference import PROFILES
from matplotlib.backends.backend_pdf import PdfPages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.captures
    receipt = json.loads((root / "receipt.json").read_text())
    analysis = receipt["analysis"]
    shape = json.loads((root / "raster/grid.json").read_text())["shape"]
    with PdfPages(args.output) as pdf:
        fig, axes = plt.subplots(2, 3, figsize=(12, 7), constrained_layout=True)
        for row, state in enumerate(("advected", "overcast")):
            for col, name in enumerate(("joint", "unshaded", "legacy")):
                values = np.fromfile(root / "raster" / f"{name}-{state}.bin", SAMPLE)
                eye = values["radiance"] + values["through"] * 0.02
                image = 1 - np.exp(-2 * eye)
                axes[row, col].imshow(image.reshape(*shape, 3), interpolation="nearest")
                axes[row, col].set(title=f"{state}: {name}")
                axes[row, col].axis("off")
        fig.suptitle(
            "Actual coarse final-transfer samples; constant0.02 background, exposure2; no Place/frame claim"
        )
        pdf.savefig(fig)
        plt.close(fig)
        rays = np.fromfile(root / "rays.bin", RAY)
        queries = np.fromfile(root / "queries.bin", "<f4")
        fig, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
        for row, index in enumerate((3, 26)):
            ray = rays[index]
            first, last = int(ray["begin"]), int(ray["end"])
            q = queries[first:last]
            ref = np.load(root / "advected-reference.npy")[first:last]
            axes[row, 0].plot(q, ref[:, 0], color="black", label="independent RTE red")
            for name in ("joint", "growth-96", "coarse", "unshaded", "legacy"):
                actual = np.fromfile(root / f"{name}-advected.bin", SAMPLE)["radiance"][
                    first:last
                ]
                axes[row, 0].plot(q, actual[:, 0], label=name)
                error = np.max(
                    abs(actual - ref[:, :3]) / np.maximum(abs(ref[:, :3]), 1e-5),
                    axis=-1,
                )
                axes[row, 1].plot(q, error * 100, label=name)
            for ax in axes[row]:
                ax.set(xlim=(0, 210 if index == 3 else 6), xlabel="eye distance km")
                ax.grid(alpha=0.2)
                ax.legend(fontsize=8)
            axes[row, 0].set(
                ylabel="radiance per lux", title=f"ray{index}, actual cloud/air overlap"
            )
            axes[row, 1].set(
                ylabel="max relative RGB %", yscale="symlog", linthresh=0.1
            )
            axes[row, 1].axhline(1, color="black", linestyle="--")
        pdf.savefig(fig)
        plt.close(fig)
        fig, axes = plt.subplots(1, 2, figsize=(12, 5), constrained_layout=True)
        profiles = list(PROFILES)
        for state in analysis["states"]:
            if state == "clear":
                continue
            rows = analysis["states"][state]
            axes[0].plot(
                profiles,
                [rows[p]["relative_RGB_max"] * 100 for p in profiles],
                marker="o",
                label=state,
            )
            axes[1].plot(
                profiles,
                [
                    receipt["profiles"][f"raster/{p}-{state}"]["warm_median_ms"]
                    for p in profiles
                ],
                marker="o",
                label=state,
            )
        axes[0].set(
            yscale="symlog", linthresh=0.1, ylabel="all dense queries: max RGB %"
        )
        axes[0].axhline(1, color="black", linestyle="--")
        axes[1].set(ylabel="coarse complete-transfer dispatch ms")
        for ax in axes:
            ax.legend()
            ax.grid(alpha=0.2)
            ax.tick_params(axis="x", rotation=25)
        fig.suptitle(
            "Same physical cohort: correctness and source-component GPU time remain separate"
        )
        pdf.savefig(fig)
        plt.close(fig)


if __name__ == "__main__":
    main()
