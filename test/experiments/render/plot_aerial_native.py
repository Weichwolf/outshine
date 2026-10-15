"""Display genuine native aerial captures beside their homogeneous controls."""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

WIDTH, HEIGHT, FRAMES, CHANNELS = 35, 19, 39, 4
SELECTION = (
    (0, "Y up, sun up, view up, E=1"),
    (2, "Y up, sun up, view along, E=1"),
    (3, "Y up, sun down, view up, E=1"),
    (5, "Y up, sun down, view along, E=1"),
    (18, "X up, sun up, view up, E=4"),
    (20, "X up, sun up, view along, E=4"),
    (36, "Zero solar illuminance"),
    (37, "Vacuum"),
    (38, "Zero camera distance"),
)


def load(path):
    values = np.fromfile(path, dtype="<f4")
    if values.size != WIDTH * HEIGHT * FRAMES * CHANNELS:
        raise ValueError("expected all 39 tightly packed 35x19 RGBA32F frames")
    if not np.isfinite(values).all():
        raise ValueError("capture contains nonfinite values")
    return values.reshape(FRAMES, HEIGHT, WIDTH, CHANNELS)


def display(rgb):
    rgb = np.clip(rgb, 0, 1)
    return np.where(rgb <= 0.0031308, 12.92 * rgb, 1.055 * rgb ** (1 / 2.4) - 0.055)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    actual = load(args.samples)
    reference = load(Path(str(args.samples) + ".reference"))
    figure, axes = plt.subplots(len(SELECTION), 3, figsize=(10, 13))
    for row, (frame, label) in enumerate(SELECTION):
        axes[row, 0].imshow(display(actual[frame, :, :, :3]), interpolation="nearest")
        axes[row, 1].imshow(
            display(reference[frame, :, :, :3]), interpolation="nearest"
        )
        axes[row, 2].imshow(
            np.clip(
                np.abs(actual[frame, :, :, :3] - reference[frame, :, :, :3]) * 1e4, 0, 1
            ),
            interpolation="nearest",
        )
        axes[row, 0].set_ylabel(label, fontsize=8)
        for axis in axes[row]:
            axis.set_xticks([])
            axis.set_yticks([])
    for column, title in enumerate(
        (
            "Native GPU",
            "Independent homogeneous reference",
            "Absolute linear RGB error x10000",
        )
    ):
        axes[0, column].set_title(title, fontsize=10)
    figure.suptitle(
        "Finite aerial raster control: actual scene/depth, half LUTs, "
        "linear RGBA32F output\nDisplay encoding only; no Place/image-quality "
        "or runtime-budget acceptance",
        fontsize=11,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.96))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for suffix in (".pdf", ".png"):
        figure.savefig(args.output.with_suffix(suffix), dpi=160)
    plt.close(figure)
    error = np.abs(actual.astype(float) - reference.astype(float))
    print(f"frames={FRAMES} maximum_absolute_RGBA_error={error.max():.9g}")


if __name__ == "__main__":
    main()
