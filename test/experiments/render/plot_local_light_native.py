"""Show actual native light-list captures, counterexamples and per-depth GPU cost."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np
from check_local_light_native import capture
from local_light_inputs import ROOT

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


def image_page(pdf, folder, title):
    figure, axes = plt.subplots(4, 4, figsize=(14, 10), constrained_layout=True)
    for plane, depth in enumerate([25, 250, 500, 5000]):
        reference = capture(folder / f"brute-{plane}.rgba32")[::4, ::4, :3]
        actual = reference
        for column, method in enumerate(["brute", "tiles", "log"]):
            actual = capture(folder / f"{method}-{plane}.rgba32")[::4, ::4, :3]
            axes[plane, column].imshow(actual / (1 + actual))
        ratio = np.max(
            np.abs(actual - reference) / np.maximum(1e-7, np.abs(reference) * 1e-5),
            axis=-1,
        )
        axes[plane, 3].imshow(ratio, vmin=0, vmax=1, cmap="magma")
        axes[plane, 0].set_ylabel(f"{depth} m view depth")
        for axis in axes[plane]:
            axis.set_xticks([])
            axis.set_yticks([])
    for axis, label in zip(
        axes[0], ["Full sum", "Tiles64", "Log64", "Log error / allowed FP32 tolerance"]
    ):
        axis.set_title(label)
    figure.suptitle(
        title
        + "\nActual 1920×1080 source captures; every fourth pixel shown. RGB/(1+RGB), black environment."
    )
    pdf.savefig(figure)
    plt.close(figure)


def timing_page(pdf, root):
    figure, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    for axis, case in zip(axes.ravel(), ["street", "dense", "overlap", "overflow"]):
        rows = [
            json.loads(line)
            for line in (root / f"{case}-timing.log").read_text().splitlines()
            if line.startswith("{")
        ]
        for index, method in enumerate(["brute", "tiles", "log"]):
            values = [
                np.median(
                    [
                        r["gpu_ms"]
                        for r in rows
                        if r.get("method") == method and r.get("plane_m") == depth
                    ]
                )
                for depth in [25, 250, 500, 5000]
            ]
            axis.bar(
                np.arange(4) + (index - 1) * 0.25, values, width=0.25, label=method
            )
        axis.set_xticks(np.arange(4), ["25", "250", "500", "5000"])
        axis.set_xlabel("View depth [m]")
        axis.set_ylabel("Render-only GPU time [ms], logarithmic")
        axis.set_yscale("log")
        axis.set_title(case)
        axis.legend()
    figure.suptitle(
        "RTX 3060: actual GPU timestamps, five alternating repetitions per depth\nCounter and validation disabled; upload/readback excluded. No full-frame or Place/device acceptance."
    )
    pdf.savefig(figure)
    plt.close(figure)


def negative_page(pdf, root):
    figure, axes = plt.subplots(2, 3, figsize=(12, 7), constrained_layout=True)
    for column, variant in enumerate(["listed", "first16", "wrong-owner"]):
        folder = root / f"small-{variant}"
        for row, plane in enumerate([0, 3]):
            pixels = capture(
                folder / f"{'brute' if column == 0 else 'log'}-{plane}.rgba32"
            )
            rgb = pixels[::4, ::4, :3] if row == 0 else pixels[500:580, 920:1000, :3]
            axes[row, column].imshow(rgb / (1 + rgb), interpolation="nearest")
            axes[row, column].set_xticks([])
            axes[row, column].set_yticks([])
        axes[0, column].set_title(
            [
                "All 17 world lights",
                "Wrong: only world IDs <16",
                "Wrong: list ordinal owns Sun shadow",
            ][column]
        )
    axes[0, 0].set_ylabel("25 m, full camera (¼ sampling)")
    axes[1, 0].set_ylabel("5000 m, central 80×80 native pixels")
    figure.suptitle(
        "Actual native negative controls — both must fail the unchanged comparison\nThe 17th light visibly illuminates the far receiver; Sun ownership uses the original world index."
    )
    pdf.savefig(figure)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private plots belong outside the repository")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with PdfPages(args.output) as pdf:
        image_page(
            pdf,
            args.input / "dense-actual",
            "8,192 mixed lights: actual direct BRDF and exact support-count control",
        )
        image_page(
            pdf,
            args.input / "overlap-actual",
            "1,024 overlapping mixed lights: retained contributions; floating-point accumulation differs",
        )
        timing_page(pdf, args.input)
        negative_page(pdf, args.input)


if __name__ == "__main__":
    main()
