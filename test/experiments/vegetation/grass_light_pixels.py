"""Descriptive common-footprint RGB errors, not a Blender transport oracle gate."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def compare(paths, output):
    images = {
        name: np.asarray(Image.open(path).convert("RGB"), float) / 255
        for name, path in paths.items()
    }
    if any(image.shape != (1080, 1920, 3) for image in images.values()):
        raise ValueError("matched1920x1080 east-view diagnostic frames required")
    # The renderer's1m² ground footprint,60° view,1.4m orthographic width.
    scale = 1920 / 1.4
    x0, x1 = int(np.ceil(960 - 0.5 * scale)) + 2, int(np.floor(960 + 0.5 * scale)) - 2
    y0, y1 = int(np.ceil(540 - 0.25 * scale)) + 2, int(np.floor(540 + 0.25 * scale)) - 2
    linear = {}
    for name, image in images.items():
        crop = image[y0:y1, x0:x1]
        linear[name] = np.where(
            crop <= 0.04045, crop / 12.92, ((crop + 0.055) / 1.055) ** 2.4
        )
    result = {
        "roi_xyxy": [x0, y0, x1, y1],
        "colour_space": "sRGB PNG decoded to relative linear RGB, Standard view/exposure0",
        "mean_rgb": {k: v.mean((0, 1)).tolist() for k, v in linear.items()},
        "mean_absolute_rgb_error": {
            k: float(abs(v - linear["physical"]).mean())
            for k, v in linear.items()
            if k != "physical"
        },
        "scope": "Descriptive fixed-scene comparison. Spatial aliasing and different EEVEE shadows remain; no chosen error threshold or physical transport validation",
    }
    output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("physical", "correlated", "independent", "reflection"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    compare(
        {
            name: getattr(args, name)
            for name in ("physical", "correlated", "independent", "reflection")
        },
        args.output,
    )
