"""Plot actual native Earth-air controls; no runtime or complete-frame claim."""

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from cloud_air_reference import RAY, SAMPLE
from cloud_air_transfer_reuse import numeric
from linear_medium_reference import STATES
from matplotlib.backends.backend_pdf import PdfPages

COLORS = {
    "profiled": "#187354",
    "products": "#af7300",
    "legacy": "#2979b0",
    "frozen": "#b22d46",
}


def errors_page(output, pdf):
    rays = np.fromfile(output / "rays.bin", RAY)
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    for ax, (state, cover, _) in zip(axes.flat, STATES):
        ref = np.load(output / f"{state}-reference.npy")
        for name, color in COLORS.items():
            data = numeric(np.fromfile(output / f"{name}-{state}.bin", SAMPLE))
            error = np.max(
                abs(data[:, :3] - ref[:, :3]) / np.maximum(ref[:, :3], 1e-5), axis=1
            )
            maxima = [error[int(r["begin"]) : int(r["end"])].max() * 100 for r in rays]
            ax.plot(maxima, label=name, color=color)
        ax.axhline(1, color="black", linestyle="--", label="1% limit")
        ax.set(
            xlabel="declared ray index (24/25: orbital descent)",
            ylabel="maximum RGB error (%)",
            yscale="log",
            title=f"{state}, cover={cover}",
        )
        ax.legend(fontsize=8)
        ax.grid(alpha=0.2)
    fig.suptitle(
        "Native Earth profiles vs independent variable-extinction RTE\nConstant prescribed Sun/Psi LUTs; actual Weather Sun-T6 and eye cells"
    )
    pdf.savefig(fig)
    plt.close(fig)


def depth_page(output, pdf):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    for column, index in enumerate((9, 24)):
        ray = rays[index]
        a, b = int(ray["begin"]), int(ray["end"])
        q = queries[a:b]
        ref = np.load(output / "advected-reference.npy")[a:b]
        axes[0, column].plot(
            q, ref[:, 0], color="black", linewidth=2, label="independent RTE"
        )
        for name, color in COLORS.items():
            values = numeric(np.fromfile(output / f"{name}-advected.bin", SAMPLE))[a:b]
            axes[0, column].plot(q, values[:, 0], color=color, label=name, alpha=0.8)
        clear_ref = np.load(output / "clear-reference.npy")[a:b]
        max_clear_error = 2e-6
        for name in ("profiled", "products", "legacy"):
            values = numeric(np.fromfile(output / f"{name}-clear.bin", SAMPLE))[a:b]
            clear_error = np.max(abs(values[:, 3:] - clear_ref[:, 3:]), axis=1)
            max_clear_error = max(max_clear_error, float(clear_error.max()))
            axes[1, column].plot(
                q,
                clear_error,
                color=COLORS[name],
                label=name,
            )
        axes[1, column].axhline(
            2e-6, color="black", linestyle="--", label="2e-6 clear limit"
        )
        axes[1, column].set_yscale("symlog", linthresh=1e-8)
        axes[1, column].set_ylim(0, max_clear_error * 1.2)
        for row in range(2):
            axes[row, column].set(
                xscale="symlog", xlabel="eye distance (km)", title=f"ray {index}"
            )
            axes[row, column].legend(fontsize=8)
            axes[row, column].set_xlim(left=0)
            axes[row, column].grid(alpha=0.2)
        axes[0, column].set_ylabel("red radiance per lux")
        axes[1, column].set_ylabel("max absolute clear T error")
    fig.suptitle(
        "Actual native depth curves and null-cloud counterexample\nSame complete physical span; query-independent source partition"
    )
    pdf.savefig(fig)
    plt.close(fig)


def native_page(output, pdf):
    shape = json.loads((output / "raster/grid.json").read_text())["shape"]
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
    for ax, (state, _, _) in zip(axes.flat, STATES):
        data = np.fromfile(output / "raster" / f"profiled-{state}.bin", SAMPLE)
        rgb = data["radiance"].reshape(*shape, 3)
        ax.imshow((1 - np.exp(-12 * rgb)) ** (1 / 2.2))
        ax.set_title(state)
        ax.axis("off")
    fig.suptitle(
        "Actual native full-physical-span component samples\n1920×1080 view, step8 grid only; display (1-exp(-12 L))^(1/2.2)\nNo surface/sky-boundary radiance, reconstruction, or independent full-image acceptance"
    )
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private report belongs outside the checkout")
    with PdfPages(args.output / "comparison.pdf") as pdf:
        for page in (errors_page, depth_page, native_page):
            page(args.output, pdf)


if __name__ == "__main__":
    main()
