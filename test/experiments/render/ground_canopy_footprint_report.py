"""Render the measured native canopy footprint comparison as a PDF."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selected", type=Path, required=True)
    parser.add_argument("--alternative", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    reports = [
        json.loads((folder / "measurement.json").read_text())
        for folder in (args.selected, args.alternative)
    ]
    data = [
        np.load(folder / "reference.npz")
        for folder in (args.selected, args.alternative)
    ]
    with PdfPages(args.output) as pdf:
        fig, axes = plt.subplots(2, 2, figsize=(12, 9), layout="constrained")
        fig.suptitle(
            "Native canopy colour — projected footprint and movement\n"
            "Actual GPU output versus independent 64×64 area integration",
            fontsize=14,
        )
        for report, arrays in zip(reports, data):
            samples = report["quadrature_samples"]
            groups = report["groups"]
            widths = sorted({group["width"] for group in groups})
            errors = [
                max(
                    float(
                        arrays["error"][
                            group["start"] : group["start"] + group["count"]
                        ].max()
                    )
                    for group in groups
                    if group["width"] == width
                )
                * 255
                for width in widths
            ]
            axes[0, 0].plot(widths, errors, "o-", label=f"{samples} samples")
            axes[0, 1].plot(np.sort(arrays["error"] * 255), label=f"{samples} samples")
            axes[1, 0].plot(arrays["motion_error"] * 255, label=f"{samples} samples")
        axes[0, 0].axhline(1, color="red", linestyle="--", label="declared 1 LSB limit")
        axes[0, 0].set(
            xlabel="Pixel width / material wavelength",
            ylabel="Worst neutral display error [LSB]",
            title="All rotations, aspects and shears",
        )
        axes[0, 1].set(
            xlabel="Sorted reference sample",
            ylabel="Display error [LSB]",
            title="All 1792 area/motion samples",
        )
        axes[1, 0].set(
            xlabel="Movement step",
            ylabel="Error in consecutive display differences [LSB]",
            title="Fixed footprint, 256 world-space positions",
        )
        axes[1, 1].axis("off")
        text = [
            "4864 GPU probes per candidate; 192 footprint groups.",
            "Axes exchanged/reversed without changing the area.",
            f"9-sample symmetry residual: {reports[0]['symmetry_max_linear']:.3g} linear.",
            f"32→64 reference convergence: {reports[0]['convergence_display_lsb']:.4f}LSB.",
            f"Unfiltered point: {reports[0]['negative_controls']['unfiltered_point_max_display_lsb']:.2f}LSB error.",
            f"Uniform mean: {reports[0]['negative_controls']['uniform_mean_max_display_lsb']:.2f}LSB error.",
            "",
            "Neutral albedo display transform only.",
            "No lighting/tone/device or whole-frame acceptance.",
            "The rejected 4-sample candidate keeps its failed status.",
        ]
        axes[1, 1].text(0, 0.95, "\n".join(text), va="top", fontsize=11)
        for axis in axes.flat[:3]:
            axis.grid(alpha=0.2)
            axis.legend()
        pdf.savefig(fig)
        plt.close(fig)
    for arrays in data:
        arrays.close()
    print(args.output)


if __name__ == "__main__":
    main()
