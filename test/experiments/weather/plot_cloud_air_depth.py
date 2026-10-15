"""Plot actual native depth curves, failures and source costs as a PDF (#353)."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import RAY, SAMPLE
from matplotlib.backends.backend_pdf import PdfPages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.captures
    rays = np.fromfile(root / "rays.bin", RAY)
    queries = np.fromfile(root / "queries.bin", "<f4")
    report = json.loads((root / "analysis.json").read_text())
    receipt = json.loads((root / "receipt.json").read_text())
    with PdfPages(args.output) as pdf:
        fig, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
        for row, state in enumerate(("advected", "stationary")):
            ray = rays[3]
            a, b = int(ray["begin"]), int(ray["end"])
            q = queries[a:b]
            ref = np.load(root / f"{state}-reference.npy")
            axes[row, 0].plot(
                q, ref[a:b, 0], color="black", label="independent RTE red"
            )
            for name in ("legacy", "linear-512", "linear-1024", "growth-128"):
                actual = np.fromfile(root / f"{name}-{state}.bin", SAMPLE)["radiance"]
                axes[row, 0].plot(q, actual[a:b, 0], label=name)
                e = np.max(
                    abs(actual[a:b] - ref[a:b, :3])
                    / np.maximum(abs(ref[a:b, :3]), 1e-5),
                    axis=-1,
                )
                axes[row, 1].plot(q, e * 100, label=name)
            axes[row, 0].set(
                title=state + ": eye20m, horizontal west",
                xlim=(0, 210),
                xlabel="eye distance km",
                ylabel="radiance per lux",
            )
            axes[row, 1].set(
                title="RGB error against independent RTE",
                xlim=(0, 210),
                xlabel="eye distance km",
                ylabel="max relative RGB %",
            )
            axes[row, 1].axhline(1, color="black", linestyle="--")
            for ax in axes[row]:
                ax.legend(fontsize=8)
                ax.grid(alpha=0.2)
        fig.suptitle(
            "Actual finite-depth transfer; physical request10000km, full atmosphere/ground clipping"
        )
        pdf.savefig(fig)
        plt.close(fig)
        fig, axes = plt.subplots(1, 3, figsize=(12, 5), constrained_layout=True)
        profiles = list(report["states"]["stationary"])
        for state in ("advected", "stationary"):
            rows = report["states"][state]
            axes[0].plot(
                profiles,
                [rows[p]["relative_RGB_max"] * 100 for p in profiles],
                marker="o",
                label=state,
            )
            axes[1].plot(
                profiles,
                [rows[p]["raster_source_positions"] / 1e6 for p in profiles],
                marker="o",
                label=state,
            )
            axes[2].plot(
                profiles,
                [
                    receipt["profiles"][f"raster/{p}-{state}"]["warm_median_ms"]
                    for p in profiles
                ],
                marker="o",
                label=state,
            )
        axes[0].axhline(1, color="black", linestyle="--")
        for ax, label in zip(
            axes,
            (
                "all dense queries: max RGB %",
                "coarse raster: million source positions",
                "coarse raster: GPU dispatch ms",
            ),
        ):
            ax.set(ylabel=label)
            ax.tick_params(axis="x", rotation=35)
            ax.grid(alpha=0.2)
            ax.legend()
        fig.suptitle(
            "Measured components only: no depth-volume stores, reconstruction or full-frame budget"
        )
        pdf.savefig(fig)
        plt.close(fig)
        fig, ax = plt.subplots(figsize=(10, 5), constrained_layout=True)
        x = np.array([0, 0, 1, 1, 1], float)
        from cloud_air_transfer_reuse import cubic

        image = np.broadcast_to(x[None, :, None], (4, 5, 1))
        values = cubic(image, 33, 25, 8)[12, :, 0]
        ax.plot(np.arange(33), values, marker=".", label="existing signed cubic")
        ax.axhline(0, color="black", linestyle="--")
        ax.axhline(1, color="black", linestyle="--")
        ax.set(
            title="Hard unit edge rejects a general physical visibility-range claim",
            xlabel="pixel",
            ylabel="reconstructed visibility",
        )
        ax.grid(alpha=0.2)
        ax.legend()
        pdf.savefig(fig)
        plt.close(fig)


if __name__ == "__main__":
    main()
