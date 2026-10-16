"""PDF from actual production solar-disc raster captures and their lookup oracle."""

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from sky_disc_validation import HEIGHT, WIDTH, image

CASES = ("high-orbit", "orbit-grazing", "orbit-edge")


def pictures(output, pdf):
    fig, axes = plt.subplots(3, 2, figsize=(13, 12), constrained_layout=True)
    for row, case in enumerate(CASES):
        for col, state in enumerate(("before", "after")):
            rgb = image(output / f"{state}-f16" / f"{case}.rgba32")[..., :3]
            axes[row, col].imshow((1 - np.exp(-rgb / 5000)) ** (1 / 2.2))
            axes[row, col].set_title(f"{case}: {state}")
            axes[row, col].axis("off")
    fig.suptitle(
        "Actual production sky.frag, GPU-generated Earth Veil LUT\n1280×720,1° vertical view, fixed control SkyView; same camera/exposure\nDisplay (1-exp(-RGB/5000))^(1/2.2); isolated solar disc, no Place/frame claim"
    )
    pdf.savefig(fig)
    plt.close(fig)


def errors(output, pdf):
    rows = json.loads((output / "analysis.json").read_text())["cases"]
    labels = sorted({r["case"] for r in rows})
    fig, ax = plt.subplots(figsize=(13, 7), constrained_layout=True)
    x = np.arange(len(labels))
    for n, (storage, field, label) in enumerate(
        (
            ("f16", "previous_maximum_transfer_error", "previous RGBA16 sampler"),
            ("f16", "maximum_transfer_error", "physical entry RGBA16 sampler"),
            ("f32", "maximum_transfer_error", "same texels via RGBA32 sampler"),
        )
    ):
        values = [
            next(
                r[field] * 100
                for r in rows
                if r["case"] == case and r["storage"] == storage
            )
            for case in labels
        ]
        ax.bar(x + (n - 1) * 0.25, values, width=0.25, label=label)
    ax.axhline(
        0.1, color="black", linestyle="--", label="0.1% incident-radiance budget"
    )
    ax.set(
        xticks=x,
        xticklabels=labels,
        yscale="log",
        ylabel="maximum absolute transfer error (% incident solar radiance)",
    )
    ax.tick_params(axis="x", rotation=20)
    ax.legend()
    ax.grid(axis="y", alpha=0.2)
    fig.suptitle(
        "Independent spherical-entry/bilinear oracle at native pixels\nNo new LUT integration or format selection; Sun-edge roundoff band reported separately\nDark-channel relative RGB error remains a separate raw diagnostic"
    )
    pdf.savefig(fig)
    plt.close(fig)


def spectra(output, pdf):
    fig, axes = plt.subplots(3, 1, figsize=(13, 10), constrained_layout=True)
    colours = ("#b33d31", "#19824d", "#3269a1")
    for ax, case in zip(axes, CASES):
        p = np.fromfile(output / "after-f16" / f"{case}.parameters", "<f4")
        solid = np.pi * float(p[26]) ** 2
        background = np.array([0.125, 0.25, 0.5])
        for state, style in (("before", "--"), ("after", "-")):
            rgb = (
                image(output / f"{state}-f16" / f"{case}.rgba32")[:, WIDTH // 2, :3]
                - background
            ) * solid
            for channel, colour in enumerate(colours):
                ax.plot(
                    np.arange(HEIGHT),
                    rgb[:, channel],
                    color=colour,
                    linestyle=style,
                    label=f"{state} {'RGB'[channel]}",
                )
        ax.set(
            title=case,
            xlabel="native center-column pixel, top to bottom",
            ylabel="solar transfer",
        )
        ax.set_xlim(0, HEIGHT - 1)
        ax.set_ylim(bottom=0)
        ax.legend(ncol=6, fontsize=8)
        ax.grid(alpha=0.2)
    fig.suptitle(
        "Actual native solar spectra across the fixed disc\nSolid: corrected entry; dashed: previous mapping; same production RGBA16 LUT"
    )
    pdf.savefig(fig)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private PDF belongs outside the checkout")
    with PdfPages(args.output / "comparison.pdf") as pdf:
        for page in (pictures, errors, spectra):
            page(args.output, pdf)


if __name__ == "__main__":
    main()
