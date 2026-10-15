"""Actual GPU albedo in an explicit small metric frame; no production activation."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_native import (
    ROOT,
    capture,
    metric_inputs,
    native_profiles,
    prepare_input,
    shaders,
)
from facade_footprint_image import image, pixels
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    values, cases = pixels()
    metric = metric_inputs(values)
    per_m = (
        (1 / native_profiles()[values[:, 6].astype(int), 1])
        .astype(np.float32)
        .astype(float)
    )
    prepare_input(output, values, metric, per_m)
    shaders(output)
    old, new = [
        capture(args.binary.resolve(), output, mode, "native", len(values), 13)
        for mode in (0, 1)
    ]
    canvas = Image.new("RGB", (640, 672), "white")
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (6, 4),
        "Native albedo, explicit small metre frame; 2x enlargement. Runtime inactive.",
        fill="black",
    )
    for row, (style, _, _) in enumerate(cases):
        for col, (label, colour) in enumerate(
            [("current material", old[:, :3]), ("joint coating prototype", new[:, :3])]
        ):
            draw.text(
                (col * 320 + 4, row * 216 + 24), f"{label}, style {style}", fill="black"
            )
            start = row * 160 * 96
            canvas.paste(
                image(colour[start : start + 160 * 96].reshape(96, 160, 3)),
                (col * 320, row * 216 + 44),
            )
    canvas.save(output / "albedo.png")
    report = {
        "scope": __doc__,
        "cases": len(values),
        "gpu_executed": True,
        "change_max": float(np.max(np.abs(old[:, :3] - new[:, :3]))),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
