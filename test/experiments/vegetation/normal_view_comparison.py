"""Plot actual one-pixel response/coverage witnesses from unchanged saved EXRs."""

import argparse
import hashlib
import json
from pathlib import Path

import bpy
import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read(path):
    image = bpy.data.images.load(str(path), check_existing=False)
    if tuple(image.size) != (64, 64):
        raise ValueError("actual64x64control frame required")
    result = np.asarray(image.pixels[:], float).reshape(64, 64, 4)
    bpy.data.images.remove(image)
    return result


def plot(previous, current, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private comparison must be under/tmp")
    cases = [
        ("atlas", previous / "mip-controls/Linear-1.exr"),
        ("UDIM", current / "constant/isolated-0-Linear-1.exr"),
        ("clamped view", current / "view-constant-final/view-0-Linear-1.exr"),
    ]
    fig, axes = plt.subplots(2, 3, figsize=(16, 9), dpi=160)
    records = []
    for column, (name, path) in enumerate(cases):
        rgba = read(path)
        alpha = rgba[..., 3]
        conditional = np.divide(
            rgba[..., 0], alpha, out=np.zeros_like(alpha), where=alpha > 0
        )
        response = float(rgba[..., 0].sum() / alpha.sum())
        alpha_sum = float(alpha.sum())
        for row, (values, title) in enumerate(
            [
                (conditional, f"conditional response {response:.6f}"),
                (alpha, f"coverage sum {alpha_sum:.6f}"),
            ]
        ):
            axes[row, column].imshow(
                values[29:35, 29:35],
                vmin=0,
                vmax=1,
                cmap="gray",
                origin="lower",
                interpolation="nearest",
            )
            axes[row, column].set_title(name + "\n" + title)
            axes[row, column].axis("off")
        records.append(
            {
                "sampler": name,
                "path": str(path),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "response": response,
                "coverage_sum": alpha_sum,
            }
        )
    fig.suptitle(
        "Same opaque1pixel quad / expected response1 and coverage1 / actual64x64EEVEE frames"
    )
    fig.text(
        0.5,
        0.02,
        "Enlarged6x6native-pixel crop, common [0,1] display range. No source geometry or light change; raw sums above.",
        ha="center",
    )
    fig.tight_layout(rect=(0, 0.04, 1, 0.94))
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output)
    plt.close(fig)
    output.with_suffix(".json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("previous", "current", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    plot(args.previous, args.current, args.output)
