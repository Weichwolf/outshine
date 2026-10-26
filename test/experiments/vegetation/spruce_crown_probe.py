"""Two physical-aspect unlit crown diagnostics, never a whole-plant atlas bake."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from conifer_window_render import tree_image
from PIL import Image, ImageDraw
from window_portrait_source import source, wood


def probe(products, catalog, output):
    output.mkdir(parents=True, exist_ok=False)
    canvas = Image.new("RGB", (1920, 1080), (227, 231, 234))
    draw = ImageDraw.Draw(canvas)
    rows = []
    for index, product in enumerate(products):
        began = time.perf_counter()
        graph, axes, windows, banks, evidence = source(
            product, product / "execution", catalog
        )
        image, (frame, lower, upper, _) = tree_image(
            windows, banks, wood(graph, axes), np.array((18.0, -70, 0)), 1024, samples=1
        )
        np.testing.assert_allclose(frame[:, 1], (0, 0, 1), rtol=0, atol=1e-14)
        alpha = image[..., 3, None]
        colour = image[..., :3] + (1 - alpha) * np.array((0.77, 0.8, 0.82))
        colour = np.where(
            colour <= 0.0031308,
            colour * 12.92,
            1.055 * np.maximum(colour, 0) ** (1 / 2.4) - 0.055,
        )
        pixels = Image.fromarray(
            np.uint8(np.rint(np.flipud(np.clip(colour, 0, 1)) * 255))
        )
        extent = upper - lower
        width = max(1, round(1000 * extent[0] / extent[1]))
        upright = pixels.resize((width, 1000), Image.Resampling.LANCZOS)
        canvas.paste(upright, (index * 960 + (960 - width) // 2, 54))
        draw.text(
            (index * 960 + 24, 16),
            f"Picea abies | {product.name} | unlit physical aspect",
            fill=(20, 28, 30),
        )
        row = {
            "identity": evidence["identity"],
            "projection_extent_m": extent.tolist(),
            "projected_coverage_m2": float(
                image[..., 3].sum() * np.prod(extent) / 1024**2
            ),
            "windows": len(windows["origins"]),
            "seconds": time.perf_counter() - began,
            "scope": "One unlit CPU raster at1024 square, physical aspect restored; final diagnostic canvas1920x1080, no lighting/material/botanical/LOD admission.",
        }
        rows.append(row)
        print(json.dumps(row), flush=True)
    canvas.save(output / "crowns.png")
    (output / "probe.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, nargs=2, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe(args.products, args.catalog, args.output)
