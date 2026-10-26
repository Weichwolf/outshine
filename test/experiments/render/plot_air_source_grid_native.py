"""Plot actual source-grid GPU captures and their independent RTE reference."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from air_source_grid_native import NATIVE_SAMPLE, ROOT
from air_source_grid_reference import SIGMA, relative_errors
from matplotlib.backends.backend_pdf import PdfPages


def display(rgb):
    rgb = np.clip(rgb, 0, 1)
    return np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * rgb ** (1 / 2.4) - 0.055)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    batch = args.captures / "earth"
    cases = json.loads((batch / "cases.json").read_text())
    case = next(
        index
        for index, row in enumerate(cases)
        if row["center_km"] == 0.45
        and row["width_km"] == 0.2
        and row["cloud_optical_depth"] == 2
    )
    rows = slice(case * 258, (case + 1) * 258, 2)
    query = np.fromfile(batch / "queries-dense.bin", "<f4")[rows]
    reference = np.load(batch / "reference-dense.npy")[rows]
    captures = [
        (name, np.fromfile(batch / name / "samples.bin", NATIVE_SAMPLE)[rows])
        for name in ("quadratic22", "log22", "log256")
    ]
    colors = ("#c34d28", "#927631", "#176da2")
    args.output.parent.mkdir(exist_ok=True, parents=True)
    with PdfPages(args.output) as pdf:
        figure, axes = plt.subplots(1, 3, figsize=(13, 5))
        axes[0].plot(
            query, reference[:, 1] * 1000, color="black", label="Independent FP64"
        )
        strips = [display(reference * 32 + np.exp(-query[:, None] * SIGMA) * 0.02)]
        for (name, captured), color in zip(captures, colors, strict=True):
            axes[0].plot(
                query, captured["radiance"][:, 1] * 1000, color=color, label=name
            )
            axes[1].plot(
                query,
                relative_errors(captured["radiance"], reference) * 100,
                color=color,
                label=name,
            )
            strips.append(
                display(captured["radiance"] * 32 + captured["through"] * 0.02)
            )
        axes[0].set_ylabel("Green radiance / lux (10⁻³ sr⁻¹)")
        axes[1].set_ylabel("Maximum RGB error (%)")
        axes[1].axhline(1, color="grey", ls=":", label="Unchanged 1% limit")
        for axis in axes[:2]:
            axis.set_xlabel("Camera query distance (km)")
            axis.grid(alpha=0.2)
            axis.legend(fontsize=8)
        axes[2].imshow(
            np.asarray(strips),
            aspect="auto",
            interpolation="nearest",
            extent=(query[0], query[-1], 3.5, -0.5),
        )
        axes[2].set_yticks(range(4), ["FP64", "quadratic22", "log22", "log256"])
        axes[2].set_xlabel("Camera query distance (km)")
        axes[2].set_title("Captured transfer, shared display exposure", fontsize=10)
        figure.suptitle("Direct cloud shadows before the air-source fit", fontsize=16)
        figure.text(
            0.5,
            0.02,
            "Full Earth physical extent; identical prescribed clear/Psi inputs and queries.\n"
            "Strip preview adds the same constant background to actual GPU L/T. No full-world or runtime-profile claim.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.1, 1, 0.92))
        pdf.savefig(figure)
        plt.close(figure)

        receipt = json.loads((args.captures / "receipt.json").read_text())
        figure, axes = plt.subplots(1, 2, figsize=(11, 5))
        labels, errors, work = [], [], []
        for name in ("quadratic22", "log22", "log256"):
            selected = [
                next(
                    row
                    for row in batch["reports"]
                    if row["profile"] == name and not row["sparse"]
                )
                for batch in receipt["batches"].values()
            ]
            labels.append(name)
            errors.append(max(row["max_relative"] for row in selected) * 100)
            work.append(max(row["max_sources"] for row in selected))
        axes[0].bar(labels, errors, color=colors)
        axes[0].set_yscale("log")
        axes[0].axhline(1, color="grey", ls=":")
        axes[0].set_ylabel("Worst native RGB error (%)")
        axes[1].bar(labels, work, color=colors)
        axes[1].set_ylabel("Peak logical source positions per tested ray")
        for axis in axes:
            axis.grid(axis="y", alpha=0.2)
        figure.suptitle("Quality and source work are separate evidence", fontsize=16)
        figure.text(
            0.5,
            0.025,
            "135 shadow controls + 3 null-cloud controls; null-cloud and sparse-query outputs are bitwise identical.\n"
            "Source positions are algorithmic work, not hardware instruction counts or GPU time.\n"
            "The 256-cell profile is experimental; clear production defaults remain at 22 quadratic cells.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.16, 1, 0.91))
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
