"""Actual source-field comparison and out-of-training camera error curves."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def fields_plot(arrays, output):
    size, query = 4, 3
    source = arrays[f"truth{size}"].reshape(3, size**2, 8)[:, :, query]
    normal = arrays[f"normal4_no_self_shadow{size}"].reshape(3, size**2, 8)[:, :, query]
    mean = arrays[f"normalized_mean_no_self_shadow{size}"].reshape(3, size**2, 8)[
        :, :, query
    ]
    joint = arrays[f"pooled12_{size}"].reshape(3, size**2, 8)[:, :, query]
    coverage = arrays[f"coverage{size}"].reshape(3, size**2)
    fig, axes = plt.subplots(3, 7, figsize=(16, 9), dpi=160, layout="constrained")
    titles = [
        "Coverage",
        "Normalized mean",
        "Four normal lobes",
        "Joint source",
        "Pooled joint modes",
        "Four minus source",
        "Modes minus source",
    ]
    for view in range(3):
        values = [
            coverage[view],
            mean[view],
            normal[view],
            source[view],
            joint[view],
            normal[view] - source[view],
            joint[view] - source[view],
        ]
        for column, value in enumerate(values):
            ax = axes[view, column]
            error = column >= 5
            image = ax.imshow(
                value.reshape(size, size),
                origin="lower",
                interpolation="nearest",
                cmap="coolwarm" if error else "viridis",
                vmin=-0.03 if error else 0,
                vmax=0.03 if error else 1,
            )
            ax.set(xticks=[], yticks=[])
            if view == 0:
                ax.set_title(titles[column], fontsize=9)
            if column == 0:
                ax.set_ylabel(f"New camera {view}")
            if view == 2 and column in (0, 6):
                fig.colorbar(image, ax=ax, orientation="horizontal", fraction=0.07)
    fig.suptitle(
        "Same 32-needle source, three additional cameras, 5° light-camera angle\nActual 4×4 pooled fields · 12 shared angular modes · own camera fields still source-baked · no whole-tree admission",
        fontsize=13,
    )
    fig.savefig(output)
    plt.close(fig)


def error_plot(report, output):
    levels = report["levels"]
    methods = [
        (
            "Four normal lobes, no self-shadow",
            lambda r: r["baseline"]["normal4_no_self_shadow"],
            "o-",
        ),
        (
            "Single-source angular modes (up to8)",
            lambda r: r["original32_modes"]["8"],
            "s-",
        ),
        ("Pooled angular modes (up to8)", lambda r: r["pooled_modes"]["8"], "^-"),
        ("Pooled angular modes (up to12)", lambda r: r["pooled_modes"]["12"], "d-"),
        (
            "64-light interpolation, no reduction",
            lambda r: r["baseline"]["angular"],
            "x--",
        ),
    ]
    fig, axes = plt.subplots(1, 2, figsize=(16, 9), dpi=160, layout="constrained")
    for label, method, style in methods:
        values = [method(row)["six_independent_lights"] for row in levels]
        axes[0].plot(
            [r["size"] for r in levels],
            [v["max_abs"] for v in values],
            style,
            label=label,
        )
        axes[1].plot(
            [r["size"] for r in levels],
            [100 * v["coverage_normalized_rmse"] for v in values],
            style,
            label=label,
        )
    for ax in axes:
        ax.set(
            xscale="log",
            xticks=[1, 2, 4, 8, 16],
            xlabel="Square footprint width (pixels)",
        )
        ax.set_xticklabels(["1", "2", "4", "8", "16"])
        ax.grid(alpha=0.3)
        ax.legend(fontsize=10)
    axes[0].set_ylabel("Maximum unit-diffuse response error")
    axes[1].set_ylabel("RMSE / RMS coverage (%)")
    fig.suptitle(
        "Shared light modes do not generalize equally at every footprint\nThree additional cameras · same physical32source · six independent evaluation lights · finite quadrature",
        fontsize=14,
    )
    fig.savefig(output)
    plt.close(fig)


def run(source, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private figures must be under/tmp")
    report = json.loads((source / "report.json").read_text())
    with np.load(source / "fields.npz") as saved:
        arrays = {key: saved[key] for key in saved.files}
    output.parent.mkdir(parents=True, exist_ok=True)
    fields_plot(arrays, output)
    error_plot(report, output.with_name(output.stem + "-errors.png"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.output)
