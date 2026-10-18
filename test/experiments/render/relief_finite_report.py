"""PDF evidence for rejected finite-corner GGX residual approximations."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from relief_shading import ANGLES

COLOURS = ("tab:orange", "tab:blue")


def curves(source, name):
    data = []
    for tiles in (1, 4):
        with np.load(source / f"{name}-{tiles}.npz") as raw:
            data.append(raw["response"])
            reference = raw["reference"]
    return reference, data


def selection_page(pdf, report):
    figure, axes = plt.subplots(1, 2, figsize=(11.7, 8.3), constrained_layout=True)
    for index, ax in enumerate(axes):
        methods = [c["methods"][index] for c in report["cases"]]
        x = np.array([m["curve_max_peak_error"] for m in methods]).ravel()
        y = np.array([m["curve_rms_peak_error"] for m in methods]).ravel()
        failed = (x > 0.02) | (y > 0.005)
        ax.scatter(
            np.maximum(x, 1e-8),
            np.maximum(y, 1e-8),
            c=np.where(failed, "crimson", "seagreen"),
            s=24,
            alpha=0.65,
        )
        ax.axvline(0.02, color="crimson", linestyle="--")
        ax.axhline(0.005, color="crimson", linestyle="--")
        ax.set(
            xscale="log",
            yscale="log",
            xlabel="Maximum / own reference peak",
            ylabel="RMS / own reference peak",
            title=f"{(1, 4)[index]} finite tile(s) per corner: {failed.sum()}/288 failures",
        )
        ax.grid(alpha=0.2)
    figure.suptitle(
        "#422: exact GGX kernel mass does not select a full BRDF filter\nSame 24 footprints, 288 curves, 65 angles; fixed limits 2% maximum / 0.5% RMS",
        fontsize=14,
    )
    pdf.savefig(figure)
    plt.close(figure)


def counterexample_page(pdf, source):
    figure, axes = plt.subplots(2, 3, figsize=(11.7, 8.3), constrained_layout=True)
    selections = (
        ("sheared-corner-0.003", 3, "Glossy, transverse, view 50°"),
        ("sheared-corner-0.003", 8, "Matte, longitudinal, view 0°"),
        ("sheared-corner-0.0002", 3, "Small glossy corner, view 50°"),
    )
    for col, (name, curve, title) in enumerate(selections):
        reference, models = curves(source, name)
        axes[0, col].plot(ANGLES, reference[curve], "k", label="Independent reference")
        peak = np.max(np.abs(reference[curve]))
        for index, actual in enumerate(models):
            label = f"{(1, 4)[index]} finite tile(s)"
            axes[0, col].plot(ANGLES, actual[curve], color=COLOURS[index], label=label)
            axes[1, col].plot(
                ANGLES,
                (actual[curve] - reference[curve]) / peak * 100,
                color=COLOURS[index],
            )
        axes[0, col].set(title=f"{title}\n{name}", ylabel="Linear specular response")
        axes[0, col].legend(fontsize=8)
        axes[1, col].axhspan(-2, 2, color="seagreen", alpha=0.12)
        axes[1, col].set(
            xlabel="Light angle (degrees)", ylabel="Signed error / peak (%)"
        )
        for ax in axes[:, col]:
            ax.grid(alpha=0.2)
    figure.suptitle(
        "Retained counterexamples: weak glossy and authored matte responses",
        fontsize=14,
    )
    pdf.savefig(figure)
    plt.close(figure)


def response_page(pdf, report, source):
    blocks = [curves(source, case["name"]) for case in report["cases"]]
    reference = np.concatenate([b[0] for b in blocks])
    scale = np.max(np.abs(reference), axis=1)[:, None]
    figure, axes = plt.subplots(1, 3, figsize=(11.7, 8.3), constrained_layout=True)
    for index, (ax, response) in enumerate(
        zip(
            axes,
            (
                reference,
                np.concatenate([b[1][0] for b in blocks]),
                np.concatenate([b[1][1] for b in blocks]),
            ),
            strict=True,
        )
    ):
        ax.imshow(
            np.clip(response / scale, 0, 1) ** (1 / 2.2),
            origin="upper",
            aspect="auto",
            cmap="magma",
            extent=(-80, 80, 288, 0),
            vmin=0,
            vmax=1,
        )
        ax.set(
            title=("Reference", "1 finite tile", "4 finite tiles")[index],
            xlabel="Light angle (degrees)",
        )
        ax.set_yticks(
            np.arange(24) * 12 + 6,
            [c["name"] for c in report["cases"]] if index == 0 else [],
        )
        ax.tick_params(axis="y", labelsize=8)
    figure.suptitle(
        "CPU specular-response bands, all 288 curves\nEach row uses its own reference peak; gamma 2.2. These are component plots, not native lit images.",
        fontsize=13,
    )
    pdf.savefig(figure)
    plt.close(figure)


def cost_page(pdf, report):
    figure, ax = plt.subplots(figsize=(11.7, 8.3), constrained_layout=True)
    ax.axis("off")
    rows = []
    for key, label in (
        ("finite_tiles", "Finite domains / footprint"),
        ("edges", "Analytic edges / footprint"),
        ("retained_array_bytes", "Retained numeric arrays (bytes)"),
        ("residual_brdf_evaluations", "Residual BRDF evaluations / 780 lights"),
        ("analytic_edge_evaluations", "Analytic edge evaluations / 780 lights"),
    ):
        cells = []
        for index in range(2):
            values = [c["methods"][index]["work"][key] for c in report["cases"]]
            cells.append(f"{np.mean(values):.2f} / {max(values)}")
        rows.append([label, *cells])
    table = ax.table(
        cellText=rows,
        colLabels=(
            "Work: mean / maximum over 24 cases",
            "1 finite tile",
            "4 finite tiles",
        ),
        bbox=(0, 0.38, 1, 0.38),
        cellLoc="left",
        colWidths=(0.58, 0.2, 0.2),
    )
    table.auto_set_font_size(False)
    table.set_fontsize(11)
    ax.set_title("Work rejected before native implementation", fontsize=16)
    ax.text(
        0.02,
        0.14,
        "780 = 12 curves × 65 light directions per footprint. Each edge includes sqrt + atan2.\n"
        "Kernel transforms also require solve, Cholesky and inverse per light in this CPU model.\n"
        "Clipping work grows with footprint coverage; no bounded runtime statistics are claimed.\n"
        "Array bytes exclude Python/GEOS objects, allocator/driver reserve and temporary copies.\n"
        "No native timing, whole-frame/load/Place, Metal/A18 or displaced visibility acceptance.",
        transform=ax.transAxes,
        fontsize=11,
    )
    pdf.savefig(figure)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = json.loads((args.source / "report.json").read_text())
    with PdfPages(args.output) as pdf:
        selection_page(pdf, report)
        counterexample_page(pdf, args.source)
        response_page(pdf, report, args.source)
        cost_page(pdf, report)


if __name__ == "__main__":
    main()
