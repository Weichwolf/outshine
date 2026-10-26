"""Plot actual native coordinate error; this is not a shaded masonry image."""

import argparse
from pathlib import Path

import matplotlib
import numpy as np
from facade_coating_raster import HEIGHT, WIDTH

matplotlib.use("Agg")
from matplotlib import pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args()
    figure, axes = plt.subplots(2, 3, figsize=(13, 7), constrained_layout=True)
    for row, name in enumerate(("near-perspective", "end255-perspective")):
        arrays = {
            mode: np.fromfile(args.input / f"{mode}-{name}.f32", dtype="<f4").reshape(
                3, HEIGHT, WIDTH, 4
            )
            for mode in ("inferred", "explicit")
        }
        assert np.array_equal(
            arrays["inferred"][2, :, :, :2], arrays["explicit"][2, :, :, :2]
        )
        for dimension, label in enumerate(("horizontal", "vertical")):
            for mode, pixels in arrays.items():
                error_mm = 1000 * (
                    pixels[1, :, :, dimension] - pixels[2, :, :, dimension]
                )
                axes[row, dimension].plot(
                    error_mm[HEIGHT // 2], label=mode, linewidth=1
                )
            axes[row, dimension].set_title(f"{name}: {label}")
            axes[row, dimension].set_ylabel("coordinate error (mm)")
            axes[row, dimension].set_xlabel("pixel column, middle row")
            axes[row, dimension].grid(alpha=0.2)
            axes[row, dimension].legend()
        for mode, pixels in arrays.items():
            axes[row, 2].plot(pixels[1, :, WIDTH // 2, 1], label=mode, linewidth=1)
        axes[row, 2].plot(
            arrays["inferred"][2, :, WIDTH // 2, 1],
            label="physical reference",
            linestyle="--",
        )
        axes[row, 2].set_title(f"{name}: vertical coordinate")
        axes[row, 2].set_ylabel("height over material origin (m)")
        axes[row, 2].set_xlabel("pixel row, middle column")
        axes[row, 2].grid(alpha=0.2)
        axes[row, 2].legend()
    figure.suptitle(
        "Actual lit vertex / RTX3060 raster: explicit metres versus UV-gradient inference"
    )
    figure.savefig(args.input / "comparison.pdf")
    plt.close(figure)


if __name__ == "__main__":
    main()
