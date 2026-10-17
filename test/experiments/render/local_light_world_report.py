"""Plot actual world-cell native images, measured cost and rebasing counterexamples."""

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

DEPTHS = [25, 250, 500, 5000]


def records(path):
    return [
        json.loads(line)
        for line in path.read_text().splitlines()
        if line.startswith("{")
    ]


def medians(rows, method):
    return [
        np.median(
            [
                r["gpu_ms"]
                for r in rows
                if r.get("method") == method and r["plane_m"] == d
            ]
        )
        for d in DEPTHS
    ]


def images(pdf, root):
    figure, axes = plt.subplots(4, 3, figsize=(13, 10), constrained_layout=True)
    folder = root / "dense-cache1"
    for row, depth in enumerate(DEPTHS):
        for column, method in enumerate(["brute", "tiles", "log"]):
            rgb = capture(folder / f"{method}-{row}.rgba32")[::4, ::4, :3]
            axes[row, column].imshow(rgb / (1 + rgb))
            axes[row, column].set_xticks([])
            axes[row, column].set_yticks([])
        axes[row, 0].set_ylabel(f"{depth} m view depth")
    for axis, title in zip(
        axes[0], ["Complete 8192-light sum", "World hash /16 m", "World hash /32 m"]
    ):
        axis.set_title(title)
    figure.suptitle(
        "Actual native direct lighting; cached camera-relative positions\n"
        "1920×1080 inputs, every fourth pixel shown; RGB/(1+RGB), black environment.\n"
        "Same production BRDF, original world IDs and Sun atlas owner. Receiver planes, no full-world acceptance."
    )
    pdf.savefig(figure)
    plt.close(figure)


def costs(pdf, root):
    figure, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    for axis, case in zip(axes[0], ["dense", "motion"]):
        for cached in [0, 1]:
            rows = records(root / f"{case}-timing{cached}.log")
            for method, width in [("tiles", 16), ("log", 32)]:
                label = f"{width} m / {'cached' if cached else 'per fragment'}"
                axis.plot(DEPTHS, medians(rows, method), marker="o", label=label)
        axis.set_xscale("log")
        axis.set_xlabel("View depth [m]")
        axis.set_ylabel("GPU submission median [ms]")
        axis.set_title(f"8192 lights / {case}")
        axis.legend(fontsize=8)
    rows = json.loads((root / "cpu-final/results.json").read_text())
    for method in ["hash", "sorted"]:
        selected = [r for r in rows if r["scene"] == "dense" and r["method"] == method]
        axes[1, 0].plot(
            [r["cell_width_m"] for r in selected],
            [r["payload_bytes"] / 1024**2 for r in selected],
            marker="o",
            label=method,
        )
    axes[1, 0].set_xlabel("World cell width [m]")
    axes[1, 0].set_ylabel("Index + cells + pose cache [MiB]; Light64 separate")
    axes[1, 0].legend()
    lines = [
        "Five repetitions per distributed depth; three for red cases.",
        "Counter/validation off. GPU interval includes optional prepass",
        "and draw; excludes upload/readback and CPU/index creation.",
        "Shared desktop + two nice compilers; own Python reference",
        "checks also overlap the measurement window. No fixed clocks.",
        "Static: 2 pose dispatches /60 draws; moving: 47 /60.",
        "Light records: 524,288 B; pose cache: 98,304 B per variant.",
        "World index survives these camera changes; actual light edits,",
        "bounded active-world residency and in-flight slots unmeasured.",
    ]
    for case in ["overlap", "overflow"]:
        row = records(root / f"{case}-timing1.log")
        lines.append(f"RED {case}/250 m: {medians(row, 'log')[1]:.2f} ms cached.")
    axes[1, 1].axis("off")
    axes[1, 1].text(0, 1, "\n".join(lines), va="top", fontsize=9, linespacing=1.5)
    figure.suptitle(
        "RTX 3060 /1920×1080: work saved, persistent bytes and remaining red cases"
    )
    pdf.savefig(figure)
    plt.close(figure)


def counterexamples(pdf, root):
    figure, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    correct = capture(root / "motion-cache1/log-1.rgba32")
    stale = capture(root / "stale-cache/log-1.rgba32")
    for axis, image, title in zip(
        axes[0],
        [correct, stale],
        ["Updated pose cache", "Wrong: reuse old camera origin"],
    ):
        rgb = image[::3, ::3, :3]
        axis.imshow(rgb / (1 + rgb))
        axis.set_title(title)
        axis.set_xticks([])
        axis.set_yticks([])
    rows = json.loads((root / "coordinates/results.json").read_text())
    axes[1, 0].plot(
        [r["max_error_to_direct_fp32_m"] * 1000 for r in rows], label="Observed error"
    )
    axes[1, 0].plot(
        [r["max_bound_m"] * 1000 for r in rows], label="Predeclared FP64-derived bound"
    )
    axes[1, 0].set_xlabel("Coordinate case (32 total, 24,576 axes each)")
    axes[1, 0].set_ylabel("Maximum difference to direct FP32 [mm]")
    axes[1, 0].legend(fontsize=8)
    bad = json.loads((root / "stale-cache-comparison.json").read_text())
    rgb = sum(r["rgb_failures"] for r in bad["records"])
    support = sum(r["support_mismatches"] for r in bad["records"])
    axes[1, 1].axis("off")
    axes[1, 1].text(
        0,
        1,
        f"Wrong pose cache: {rgb:,} RGB failures; {support:,} support mismatches.\n"
        "Both actual negative controls must fail, with unchanged tolerances.\n"
        "A separate wrong-camera-cell GPU kernel exceeds the same bound.\n\n"
        "128 m emitter coordinate cells, int32 cell + FP32 local position.\n"
        "Coordinate cases include Earth-size translation and 30/60 Hz steps\n"
        "across cell boundaries; maximum observed discrepancy: 0.1221 mm.\n"
        "All 12 moving/Earth raster files are byte-identical.\n\n"
        "This does not prove general FP32 culling, dynamic-light updates,\n"
        "transparent composition, a complete frame, Metal execution or A18.\n"
        "The current product still has the 16-light limit; renderer port #328.\n"
        "Default LLVM22 analysis remains red on 14 shared Check.h findings.",
        va="top",
        fontsize=9,
        linespacing=1.5,
    )
    figure.suptitle("Coordinate precision and actual stale-cache counterexample")
    pdf.savefig(figure)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external PDF required")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with PdfPages(args.output) as pdf:
        images(pdf, args.input)
        costs(pdf, args.input)
        counterexamples(pdf, args.input)


if __name__ == "__main__":
    main()
