"""Plot the matched canopy work comparison without adding overlapping intervals."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from PIL import Image

MODES = ("legacy", "nine", "adaptive")
LABELS = ("Legacy", "Nine samples", "Adaptive four/nine")
COLOURS = ("tab:blue", "tab:orange", "tab:green")


def plot_timing(pdf, place, data):
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.subplots_adjust(bottom=0.18, top=0.91, hspace=0.3, wspace=0.23)
    fig.suptitle(
        f"{place} — matched native material workload\n1920×1080/60, 60 frames / 360°",
        fontsize=15,
    )
    for mode, label, colour in zip(MODES, LABELS, COLOURS):
        current = data["cases"][mode]
        axes[0, 0].plot(
            current["probe"]["diagnostic"]["frame_ms"], label=label, color=colour
        )
        for axis, category, title in (
            (axes[1, 0], "ground", "Ground draw intervals"),
            (axes[1, 1], "objects", "Object raster intervals"),
        ):
            values = current["probe"]["diagnostic"]["interval_series"][category]
            measured = [value for value in values if value is not None]
            median = np.median(measured) if measured else np.nan
            maximum = max(measured) if measured else np.nan
            at = MODES.index(mode)
            axis.bar(at - 0.16, median, 0.3, color=colour)
            axis.bar(at + 0.16, maximum, 0.3, color=colour, alpha=0.4)
            axis.set(title=title, ylabel="Individual interval [ms]")
            axis.set_xticks(range(3), LABELS, rotation=8)
    axes[0, 0].set(
        title="Instrumented submissions, all measured frames",
        xlabel="Measured frame index",
        ylabel="GPU submission [ms]",
    )
    axes[0, 0].legend()
    positions = np.arange(3)
    for offset, kind, alpha in ((-0.17, "plain", 1), (0.17, "probe", 0.4)):
        axes[0, 1].bar(
            positions + offset,
            [data["cases"][mode][kind]["gpu_max_ms"] for mode in MODES],
            0.32,
            color=COLOURS,
            alpha=alpha,
        )
    axes[0, 1].set(
        title="GPU submission maximum: ordinary / diagnostic", ylabel="Maximum [ms]"
    )
    axes[0, 1].set_xticks(positions, LABELS, rotation=8)
    for axis in axes.flat:
        axis.grid(axis="y", alpha=0.2)
    lines = ["Ordinary host-p99 / preload / turn [ms]; frame/load/turn gates (P/F):"]
    for mode, label in zip(MODES, LABELS):
        row = data["cases"][mode]["plain"]
        gates = "/".join(
            "P" if row[key] else "F" for key in ("frame_gate", "load_gate", "turn_gate")
        )
        lines.append(
            f"{label}: {row['host_p99_ms']:.4f} / {row['preload_ms']:.2f} / {row['turn_ms']:.4f}; {gates}."
        )
    lines.extend(
        (
            "Dark / pale bars: ordinary / diagnostic (top); median / maximum of per-frame largest individual intervals (bottom).",
            "Intervals overlap: do not add. Statistics queries perturb execution; no isolated speedup claim.",
            "All ordinary/probe image pairs exact. Earlier red controls remain red. No Metal/A18 acceptance.",
        )
    )
    fig.text(0.5, 0.015, "\n".join(lines), ha="center", fontsize=9)
    pdf.savefig(fig)
    plt.close(fig)


def plot_images(pdf, source, place):
    fig, axes = plt.subplots(1, 3, figsize=(18, 7))
    fig.subplots_adjust(bottom=0.12, top=0.87, wspace=0.02)
    fig.suptitle(f"{place} — actual ordinary native final frames", fontsize=15)
    for axis, mode, label in zip(axes, MODES, LABELS):
        with Image.open(source / f"{place}-{mode}-plain.png") as picture:
            assert picture.size == (1920, 1080)
            axis.imshow(picture)
        axis.set_title(label)
        axis.axis("off")
    fig.text(
        0.5,
        0.07,
        "All instrumented counterparts are byte-exact. Material fields change colour/roughness/relief; geometry and coverage stay fixed.\nThese are material counterchecks, not broad world-quality acceptance.",
        ha="center",
        fontsize=11,
    )
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--comparison", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(
        Path(__file__).resolve().parents[3]
    ) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("PDF output must be in system temp")
    data = json.loads(args.comparison.read_text())
    with PdfPages(output) as pdf:
        for place, result in data["places"].items():
            plot_timing(pdf, place, result)
            plot_images(pdf, Path(data["source"]), place)
    print(output)


if __name__ == "__main__":
    main()
