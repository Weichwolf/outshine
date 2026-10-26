"""Plot captured native angular responses; refuses preparation-only reports."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("explicit system-temp image required")
    report = json.loads((args.input / "report.json").read_text())
    if not report["gpu_executed"] or report["status"] != "compared":
        raise ValueError("native capture required")
    values = np.fromfile(args.input / "native.f32", dtype="<f4").reshape(
        *report["shape"][:-1], 8
    )
    rgb = values[..., :3] + values[..., 4:7]
    luminance = rgb @ np.array([0.2126, 0.7152, 0.0722])
    figure, axes = plt.subplots(2, 3, figsize=(14, 7), layout="constrained")
    for axis, kind in zip(
        list(axes.flat)[:5],
        ("albedo", "roughness", "normal", "constant_gloss", "native_pane"),
        strict=True,
    ):
        i = next(
            i
            for i, c in enumerate(report["cases"])
            if c["kind"] == kind and c["weight"] == 0.5 and c["view_degrees"] == 35
        )
        row = luminance[i]
        reference = 0.5 * (row[:, 0] + row[:, 1])
        axis.plot(
            np.linspace(-80, 80, 161),
            reference,
            color="black",
            linewidth=2,
            label="fine response mean",
        )
        for j, label in (
            (2, "parameter mean"),
            (3, "ideal atlas moments"),
            (4, "UNORM8 r^4"),
            (5, "UNORM8 r (control)"),
        ):
            axis.plot(np.linspace(-80, 80, 161), row[:, j], label=label, alpha=0.8)
        axis.set(
            title=kind,
            xlabel="light angle (degrees)",
            ylabel="linear luminance / unit irradiance",
        )
        axis.set_yscale("symlog", linthresh=0.05)
        axis.grid(alpha=0.2)
    axes[0, 0].legend(fontsize=8)
    axes[1, 2].set_axis_off()
    figure.suptitle(
        "Captured production direct GGX; fixed geometry, view 35°, equal area weights"
    )
    figure.savefig(args.output, dpi=130)
    plt.close(figure)


if __name__ == "__main__":
    main()
