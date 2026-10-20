"""Private whole-cell coverage diagnostics, not a shaded Outshine image (#952)."""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def plot(output):
    report = json.loads((output / "report.json").read_text())
    fig, axes = plt.subplots(3, 4, figsize=(16, 7), layout="constrained")
    labels = [
        "Original triangle coverage",
        "Only supports <=1px; incomplete",
        "Leaf-stop diagnostic; over budget",
        "Leaf-stop minus original",
    ]
    for row, name in enumerate(report["images"]):
        with np.load(output / f"{name}.npz") as data:
            fields = [
                data["reference"],
                data["admissible_only"],
                data["leaf_stop"],
                data["leaf_stop"] - data["reference"],
            ]
            for column, (label, field) in enumerate(zip(labels, fields, strict=True)):
                ax = axes[row, column]
                ax.imshow(
                    field,
                    interpolation="nearest",
                    cmap="coolwarm" if column == 3 else "gray_r",
                    vmin=-1 if column == 3 else 0,
                    vmax=1,
                )
                ax.set_title(label, fontsize=10)
                ax.set_xlabel(name + " — same joint viewport ROI")
                ax.set_xticks([])
                ax.set_yticks([])
    fig.suptitle(
        "Complete original cell, 1280×720 / 4×4 samples per pixel — geometry/coverage only, no PBR"
    )
    fig.savefig(output / "coverage.png", dpi=120)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    plot(parser.parse_args().output)
