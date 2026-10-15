"""Plot actual native low-cloud front captures without changing their rendered content."""

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages


def load(path):
    case = json.loads(path.read_text())
    stem = str(path)[:-5]
    return case, np.fromfile(stem + ".images.bin", dtype="<f4").reshape(
        5, case["height"], case["width"], 4
    )


def display(rgb):
    value = np.clip(rgb, 0.0, 1.0)
    return np.where(
        value <= 0.0031308, 12.92 * value, 1.055 * value ** (1 / 2.4) - 0.055
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clear", type=Path, required=True)
    parser.add_argument("--cloud", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    _clear, a = load(args.clear)
    cloud, b = load(args.cloud)
    if a.shape != b.shape or not np.array_equal(a[0], b[0]):
        raise ValueError(
            "comparison requires identical actual native scene inputs and profile"
        )
    pdf = args.output / "cloud-front-raster.pdf"
    with PdfPages(pdf) as pages:
        figure, axes = plt.subplots(3, 1, figsize=(12, 21), constrained_layout=True)
        panels = (
            (
                a[0, :, :, :3],
                "Native scene before the medium pass; receivers at 500 / 1600 / 4500 m",
            ),
            (a[1, :, :, :3], "Captured known-zero low cloud: clear-air front transfer"),
            (
                b[1, :, :, :3],
                "Separately declared low cloud 0.65: one joint cloud / air front transfer",
            ),
        )
        for axis, (rgb, title) in zip(axes, panels):
            axis.imshow(display(rgb))
            axis.set_title(title)
            axis.axis("off")
        figure.suptitle(
            f"Actual Vulkan raster outputs, {cloud['width']}×{cloud['height']}\n"
            "Linear radiance converted to sRGB for display; native source files remain unchanged"
        )
        pages.savefig(figure)
        figure.savefig(args.output / "cloud-front-raster.png", dpi=100)
        plt.close(figure)
        figure, axes = plt.subplots(2, 2, figsize=(13, 9), constrained_layout=True)
        difference = np.linalg.norm(b[1, :, :, :3] - a[1, :, :, :3], axis=-1)
        values = (
            (
                b[3, :, :, :3].mean(axis=-1),
                "Native joint RGB transmittance, mean",
                "viridis",
            ),
            (difference, "RGB change from known-zero low layer", "magma"),
            (
                b[4, :, :, 0],
                "Actual logical view-density evaluations per pixel",
                "viridis",
            ),
            (
                b[4, :, :, 1],
                "Actual logical cloud-source evaluations per pixel",
                "viridis",
            ),
        )
        for axis, (value, title, palette) in zip(axes.flat, values):
            chart = axis.imshow(value, cmap=palette)
            axis.set_title(title)
            axis.set_xlabel("pixel x")
            axis.set_ylabel("pixel y")
            figure.colorbar(chart, ax=axis)
        figure.suptitle(
            "Physical cloud shell 2572–3172 m; default finite partition 16 view / 6 solar samples\n"
            "Homogeneous-air and fixed illumination controls; not a complete Place or GPU-budget result"
        )
        pages.savefig(figure)
        figure.savefig(args.output / "cloud-front-fields.png", dpi=110)
        plt.close(figure)
    print(pdf)


if __name__ == "__main__":
    main()
