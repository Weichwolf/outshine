"""Plot the CPU-only equal-input local-light comparison as a private PDF (#448)."""

import argparse
import json
from pathlib import Path

import matplotlib
import numpy as np
from local_light_inputs import ROOT

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private plots belong outside the repository")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    report = json.loads((args.input / "street-log/comparison.json").read_text())
    capture = np.load(args.input / "street-log/captures.npz")
    width, height = report["sample_grid_per_depth"]
    images = [
        capture[k].reshape(4, height, width, 3) for k in ("reference", "radiance")
    ]
    with PdfPages(args.output) as pdf:
        figure, axes = plt.subplots(4, 3, figsize=(12, 9), constrained_layout=True)
        for row, depth in enumerate(report["depths_m"]):
            for column, image in enumerate(images):
                mapped = np.maximum(image[row], 0)
                axes[row, column].imshow(mapped / (1 + mapped), interpolation="nearest")
            difference = np.max(np.abs(images[0][row] - images[1][row]), axis=-1)
            axes[row, 2].imshow(difference, vmin=0, vmax=1e-8, cmap="magma")
            axes[row, 0].set_ylabel(f"view depth {depth:g} m")
            for axis in axes[row]:
                axis.set_xticks([])
                axis.set_yticks([])
            axes[row, 2].set_xlabel(f"max |RGB difference| = {difference.max():.3g}")
        for axis, title in zip(
            axes[0], ("Full light sum", "Logarithmic clusters", "Absolute error")
        ):
            axis.set_title(title)
        figure.suptitle(
            "CPU point-light control: 2,539 lamps, 96×54 receivers per depth\n"
            "Procedural lamps on retained Vienna axes; not native frames or actual road surfaces"
        )
        pdf.savefig(figure)
        plt.close(figure)
        names = [
            "street-brute",
            "street-tiles",
            "street-linear",
            "street-log",
            "street-log32",
        ]
        rows = [
            json.loads((args.input / n / "comparison.json").read_text()) for n in names
        ]
        figure, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
        metrics = [
            ("light_tests_selected", "Evaluated light/receiver pairs"),
            (
                "logical_gpu_payload_bytes",
                "Logical GPU buffers [bytes], no driver reserve",
            ),
            ("build_with_tracemalloc_ms", "Python build with tracing [ms]"),
            ("query_ms", "Python grouped radiance evaluation [ms]"),
        ]
        for axis, (key, title) in zip(axes.ravel(), metrics):
            axis.bar(np.arange(len(rows)), [r[key] for r in rows])
            axis.set_xticks(
                np.arange(len(rows)), [n.removeprefix("street-") for n in names]
            )
            axis.set_title(title)
            if key == "light_tests_selected":
                axis.set_yscale("log")
            else:
                axis.ticklabel_format(axis="y", style="sci", scilimits=(0, 4))
        figure.suptitle(
            "Same 20,736 point queries and source lights; all results equal the full sum\n"
            "Two concurrent single-BLAS-thread processes under host load; no GPU time or frame budget proof"
        )
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
