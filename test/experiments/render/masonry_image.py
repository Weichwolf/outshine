"""Actual native component albedo; synthetic affine pixels, no lit Place/raster."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from facade_footprint import ROOT
from ground_litter_native import capture
from masonry_native import SELECTED_METHOD, prepare


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    width, height = 192, 128
    u, v = np.meshgrid(
        np.arange(width) - width / 2 + 0.5, np.arange(height) - height / 2 + 0.5
    )
    rows, names = [], []
    for name, centre, dx, dy, style in (
        (
            "plain near / 1.2m across",
            [0.31, 0.27],
            [1.2 / width, 0],
            [0, -0.8 / height],
            0,
        ),
        ("House opening edge", [0.9, 0.75], [1.2 / width, 0], [0, -0.8 / height], 1),
        ("grazing sheared footprint", [0.31, 0.27], [0.02, 0.005], [0.02, 0.001], 0),
    ):
        at = np.array(centre) + u[..., None] * dx + v[..., None] * dy
        rows.append(
            np.column_stack(
                [
                    at.reshape(-1, 2),
                    np.broadcast_to(dx, (width * height, 2)),
                    np.broadcast_to(dy, (width * height, 2)),
                    np.full(width * height, style),
                ]
            )
        )
        names.append(name)
    values = np.concatenate(rows).astype(np.float32).astype(float)
    prepare(output, values, baseline=True)
    actual = {
        method: capture(args.binary.resolve(), output, index, "native", len(values), 13)
        for index, method in (
            (6, "flat baseline"),
            (0, "point"),
            (SELECTED_METHOD, "joint16"),
        )
    }
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(3, 3, figsize=(13, 9), constrained_layout=True)
    for row, name in enumerate(names):
        for col, (method, pixels) in enumerate(actual.items()):
            rgb = pixels[row * width * height : (row + 1) * width * height, :3].reshape(
                height, width, 3
            )
            assert np.isfinite(rgb).all() and rgb.min() >= 0 and rgb.max() <= 1
            srgb = np.where(
                rgb <= 0.0031308, rgb * 12.92, 1.055 * rgb ** (1 / 2.4) - 0.055
            )
            axes[row, col].imshow(srgb, interpolation="nearest")
            axes[row, col].set_title(name + "\n" + method)
            axes[row, col].axis("off")
    figure.suptitle("Native compute albedo / same affine pixels / no lighting or Place")
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)
    report = {
        "scope": __doc__,
        "native_executed": True,
        "pixels_per_capture": len(values),
        "captures": 3,
        "size_per_panel": [width, height],
        "max_point_joint_rgb_difference": float(
            np.max(np.abs(actual["point"][:, :3] - actual["joint16"][:, :3]))
        ),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
