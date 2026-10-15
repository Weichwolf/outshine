"""Plot actual joint-transfer failure and factored native air reconstruction."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import SAMPLE
from cloud_air_transfer_reuse import bilinear, grid_indices, numeric
from matplotlib.backends.backend_pdf import PdfPages


def display(values):
    linear = np.clip(values[..., :3] + values[..., 3:] * 0.02, 0, 1)
    return np.where(
        linear <= 0.0031308, linear * 12.92, 1.055 * linear ** (1 / 2.4) - 0.055
    )


def comparison(pdf, actual, reconstructed, title, labels):
    figure, axes = plt.subplots(3, 1, figsize=(13, 13))
    for axis, values, label in zip(
        axes[:2], (actual, reconstructed), labels, strict=True
    ):
        axis.imshow(display(values), interpolation="nearest")
        axis.set_title(label)
        axis.axis("off")
    relative = (
        np.max(
            abs(reconstructed[..., :3] - actual[..., :3])
            / np.maximum(abs(actual[..., :3]), 1e-5),
            axis=-1,
        )
        * 100
    )
    plot = axes[2].imshow(
        relative, vmin=0, vmax=2, cmap="magma", interpolation="nearest"
    )
    axes[2].set_title(
        f"Actual RGB change (%), max{relative.max():.3g}; fixed0–2% scale"
    )
    axes[2].axis("off")
    figure.colorbar(plot, ax=axes[2], shrink=0.7, label="Maximum channel change (%)")
    figure.suptitle(title, fontsize=15)
    figure.text(
        0.5,
        0.012,
        "Same captured numerical transfer + constant background; shared display transform.\n1920×1080,10km query, homogeneous-air control; not an engine/Place frame.",
        ha="center",
        fontsize=9,
    )
    figure.tight_layout(rect=(0, 0.05, 1, 0.96))
    pdf.savefig(figure)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    if args.output.resolve().is_relative_to(root):
        parser.error("private outputs belong outside the checkout")
    config = json.loads((args.captures / "configuration.json").read_text())
    receipt = json.loads((args.captures / "receipt.json").read_text())
    width, height = config["width"], config["height"]
    joint = numeric(np.fromfile(args.captures / "joint.bin", SAMPLE)).reshape(
        height, width, 6
    )
    air = numeric(np.fromfile(args.captures / "air.bin", SAMPLE)).reshape(
        height, width, 6
    )
    indices, shape = grid_indices(width, height, 8)
    coarse_joint = bilinear(
        joint.reshape(-1, 6)[indices].reshape(*shape, 6), width, height, 8
    )
    rebuilt = numeric(
        np.fromfile(args.captures / "step-8/reconstructed.bin", SAMPLE)
    ).reshape(height, width, 6)
    args.output.parent.mkdir(exist_ok=True, parents=True)
    with PdfPages(args.output) as pdf:
        comparison(
            pdf,
            joint,
            coarse_joint,
            "Joint transfer loses actual eye-cloud detail",
            (
                "Direct joint cloud/air transfer",
                "Bilinear joint transfer,8px nodes — rejected",
            ),
        )
        comparison(
            pdf,
            air,
            rebuilt,
            "Share weighted solar visibility, retain local air response",
            (
                "Direct shadowed air; eye-cloud disabled only in this control",
                "Native factored air,8px nodes; eye-cloud still a separate requirement",
            ),
        )
        figure, axes = plt.subplots(1, 3, figsize=(15, 5))
        rows = receipt["analysis"]["steps"]
        for key, name in (
            ("joint_bilinear", "Joint L/T"),
            ("air_bilinear", "Air L/T"),
            ("normalized_native", "Factored air"),
        ):
            axes[0].plot(
                [r["step"] for r in rows],
                [r[key]["relative_RGB_max"] * 100 for r in rows],
                "o-",
                label=name,
            )
        axes[0].axhline(1, color="gray", ls=":", label="Unchanged1% RGB")
        axes[0].set_yscale("log")
        axes[0].set_xlabel("Raster spacing (pixels)")
        axes[0].set_ylabel("Worst change against full direct image (%)")
        axes[0].legend(fontsize=8)
        for name, color in (("producer", "#256898"), ("reconstruction", "#cf7335")):
            axes[1].plot(
                [r["step"] for r in rows],
                [
                    receipt["profiles"][str(r["step"])][name]["warm_median_ms"]
                    for r in rows
                ],
                "o-",
                label=name,
                color=color,
            )
        axes[1].set_xlabel("Raster spacing (pixels)")
        axes[1].set_ylabel("Actual dispatch warm median (ms)")
        axes[1].legend(fontsize=8)
        for row in rows:
            axes[2].scatter(
                [row["step"]] * len(row["independent"]),
                [x["reused_relative_RGB"] * 100 for x in row["independent"]],
                s=15,
            )
        axes[2].axhline(1, color="gray", ls=":")
        axes[2].set_xlabel("Raster spacing (pixels)")
        axes[2].set_ylabel("Independent RTE witness error (%)")
        for axis in axes:
            axis.grid(alpha=0.2)
        figure.suptitle("Component quality and GPU work; full runtime remains open")
        figure.text(
            0.5,
            0.015,
            "GPU intervals exclude CPU normalization, readback/upload and other renderer stages.\nNo arbitrary-depth, joint-cloud, history, Place, p99 or A18 acceptance.",
            ha="center",
            fontsize=9,
        )
        figure.tight_layout(rect=(0, 0.12, 1, 0.95))
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
