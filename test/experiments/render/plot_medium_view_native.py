"""Show actual native medium-view raster captures against independent transport."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages

ROOT = Path(__file__).resolve().parents[3]
WIDTH, HEIGHT, OPAQUE_FRAMES, GEOMETRY_FRAMES = 43, 31, 75, 36
SELECTION = (
    (2, "Perspective, full viewport"),
    (14, "Perspective, jitter and inset"),
    (34, "Ortho, full viewport"),
    (62, "Ortho, rotated, jitter and inset"),
    (64, "Outside atmosphere, inward"),
    (67, "Night eye, planet shadow edge"),
    (70, "Ortho, top entry and planet clip"),
    (72, "Zero solar illuminance"),
    (73, "Vacuum"),
    (74, "Zero camera distance"),
)


def display(rgb):
    rgb = np.clip(rgb, 0, 1)
    return np.where(rgb <= 0.0031308, 12.92 * rgb, 1.055 * rgb ** (1 / 2.4) - 0.055)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    actual = np.fromfile(str(args.samples) + ".opaque", "<f4").reshape(
        OPAQUE_FRAMES, HEIGHT, WIDTH, 4
    )
    reference = np.fromfile(str(args.samples) + ".opaque.reference", "<f4").reshape(
        actual.shape
    )
    geometry = np.fromfile(str(args.samples) + ".geometry", "<f4").reshape(
        GEOMETRY_FRAMES, HEIGHT, WIDTH, 3, 4
    )
    if not all(np.isfinite(values).all() for values in (actual, reference, geometry)):
        raise ValueError("nonfinite native capture")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with PdfPages(args.output) as pdf:
        figure, axes = plt.subplots(
            len(SELECTION), 3, figsize=(11, 15), layout="constrained"
        )
        for row, (index, label) in enumerate(SELECTION):
            for col, pixels in enumerate(
                (
                    display(actual[index, :, :, :3]),
                    display(reference[index, :, :, :3]),
                    np.clip(
                        np.abs(actual[index, :, :, :3] - reference[index, :, :, :3])
                        * 1e5,
                        0,
                        1,
                    ),
                )
            ):
                axes[row, col].imshow(pixels, interpolation="nearest")
                axes[row, col].set_axis_off()
            if row > 0:
                axes[row, 0].set_title(label, fontsize=9, loc="left")
        for col, title in enumerate(
            (
                "Actual native raster",
                "Independent analytic reference",
                "Absolute error ×100000",
            )
        ):
            axes[0, col].set_title(
                f"{title}\n{SELECTION[0][1] if col == 0 else ' '}", fontsize=10
            )
        figure.suptitle(
            "Shared MediumView224 • actual 43×31 homogeneous transport controls",
            fontsize=15,
        )
        figure.supxlabel(
            "No Place, joint-cloud/Thin/history, frame-budget or Metal/A18 acceptance.",
            fontsize=10,
        )
        pdf.savefig(figure)
        plt.close(figure)
        figure, axes = plt.subplots(2, 3, figsize=(11, 7), layout="constrained")
        for row, index in enumerate((17, 31)):
            for col, (target, component, label, unit) in enumerate(
                (
                    (1, 0, "World origin offset X", "m"),
                    (1, 1, "World origin offset Y", "m"),
                    (0, 3, "Fragment query distance", "km"),
                )
            ):
                values = geometry[index, :, :, target, component]
                values = np.ma.masked_where(geometry[index, :, :, 2, 3] < 0, values)
                plot = axes[row, col].imshow(
                    values, interpolation="nearest", cmap="viridis"
                )
                axes[row, col].set_title(label)
                axes[row, col].set_axis_off()
                figure.colorbar(plot, ax=axes[row, col], label=unit, shrink=0.8)
        figure.suptitle(
            "Actual orthographic per-pixel origins • full / rotated+jittered inset",
            fontsize=14,
        )
        pdf.savefig(figure)
        plt.close(figure)


if __name__ == "__main__":
    main()
