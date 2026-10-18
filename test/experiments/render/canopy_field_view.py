"""Inspect actual native computed canopy fields against their finite reference."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import numpy as np
from exchange_project import write_rgb
from matplotlib import pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from PIL import Image


def run(inputs, output):
    output = output.resolve()
    if (
        output.exists()
        or not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or output.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        raise ValueError("fresh explicit system-temp output outside checkout required")
    report = json.loads((inputs / "result.json").read_text())
    with np.load(inputs / "frame-diagnostics.npz", allow_pickle=False) as data:
        native, reference = data["actual"], data["reference"]
    output.mkdir(parents=True)
    width = report["sizes"][0]
    images = {}
    with PdfPages(output / "comparison.pdf") as pdf:
        figure, axes = plt.subplots(4, 3, figsize=(12, 14), constrained_layout=True)
        for row, name in enumerate(
            ("front", "back", "back-no-transmission", "ground-only")
        ):
            state = report["states"].index(name)
            values = [reference[state, : width * width], native[state, : width * width]]
            for col, (mode, data) in enumerate(
                zip(("F64 nodal model", "Native GPU field"), values, strict=True)
            ):
                rgb = (data[:, :3] + data[:, 3:]).reshape(width, width, 3)
                path = output / f"{name}-{col}.png"
                images[path.name] = write_rgb(path, rgb)
                axes[row, col].imshow(Image.open(path), interpolation="nearest")
                axes[row, col].set_title(f"{name}: {mode}")
                axes[row, col].axis("off")
            indirect = native[state, : width * width, 3:].reshape(width, width, 3)
            path = output / f"{name}-indirect.png"
            images[path.name] = write_rgb(path, indirect)
            axes[row, 2].imshow(Image.open(path), interpolation="nearest")
            axes[row, 2].set_title("Native first-neighbour term")
            axes[row, 2].axis("off")
        figure.suptitle(
            "1m² ground-anchored footprint, fixed east view; linear RGB to sRGB, no exposure change"
        )
        pdf.savefig(figure)
        plt.close(figure)
        figure, axes = plt.subplots(
            2, len(report["sizes"]), figsize=(16, 6), constrained_layout=True
        )
        for row, name in enumerate(("front", "back")):
            state = report["states"].index(name)
            first = 0
            for col, size in enumerate(report["sizes"]):
                data = native[state, first : first + size * size]
                rgb = (data[:, :3] + data[:, 3:]).reshape(size, size, 3)
                path = output / f"{name}-mip{col}.png"
                images[path.name] = write_rgb(path, rgb)
                axes[row, col].imshow(Image.open(path), interpolation="nearest")
                axes[row, col].set_title(f"{name} {size}×{size}")
                axes[row, col].axis("off")
                first += size * size
        figure.suptitle(
            "Native stored mips, enlarged for inspection; no near-detail or angular qualification"
        )
        pdf.savefig(figure)
        plt.close(figure)
    (output / "images.json").write_text(json.dumps(images, indent=2) + "\n")
    print(output / "comparison.pdf")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
