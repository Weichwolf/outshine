"""Write only newly procedural cached bark maps and a complete local recipe."""

import argparse
import hashlib
import json
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from bark_furrows import FurrowRecipe
from furrow_maps import maps
from furrow_raster import raster
from PIL import Image


def export(recipe, fields, output):
    encoded = {
        "height": np.rint(fields["height_m"] / 0.012 * 65535).astype(np.uint16),
        "colour": np.rint(fields["colour_linear"] * 255).astype(np.uint8),
        "normal_roughness": np.concatenate(
            (
                np.rint((fields["normal"] * 0.5 + 0.5) * 255).astype(np.uint8),
                np.rint(fields["roughness"][:, :, None] * 255).astype(np.uint8),
            ),
            axis=2,
        ),
    }
    for name, values in encoded.items():
        Image.fromarray(values[::-1]).save(output / f"{name}.png")
    metadata = {
        "format": "procedural-furrow-bark-1",
        "recipe": asdict(recipe),
        "texels_height_width": list(fields["height_m"].shape),
        "height_scale_m": 0.012,
        "colour_space": "linear",
        "orientation": "array row grows with metric V; PNG rows flipped for bottom-left UV",
        "normal_space": "right-handed tangent, positive V; central periodic metric derivative",
        "metallic": 0,
        "packing": "colour=RGB8 linear; normal_roughness=RGBA8 (XYZ normal, roughness alpha); height=R16",
        "maps": {
            name: {
                "file": f"{name}.png",
                "sha256": hashlib.sha256(
                    (output / f"{name}.png").read_bytes()
                ).hexdigest(),
            }
            for name in encoded
        },
    }
    (output / "material.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return metadata


def run(output, shape):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private material assets must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    recipe = FurrowRecipe()
    start = time.perf_counter()
    relief, work = raster(recipe, shape)
    relief_s = time.perf_counter() - start
    start = time.perf_counter()
    fields = maps(recipe, relief)
    maps_s = time.perf_counter() - start
    metadata = export(recipe, fields, output)
    report = {
        **work,
        "relief_s": relief_s,
        "maps_s": maps_s,
        "field_array_bytes": sum(v.nbytes for v in fields.values()),
        "height_min_max_m": [
            float(fields["height_m"].min()),
            float(fields["height_m"].max()),
        ],
        "normal_max_unit_error": float(
            abs(np.linalg.norm(fields["normal"], axis=2) - 1).max()
        ),
        "physical_texel_m": [recipe.height_m / shape[0], recipe.width_m / shape[1]],
        "png_bytes": sum(
            (output / v["file"]).stat().st_size for v in metadata["maps"].values()
        ),
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--height", type=int, default=2048)
    parser.add_argument("--width", type=int, default=1024)
    args = parser.parse_args()
    run(args.output, (args.height, args.width))
