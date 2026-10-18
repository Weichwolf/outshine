"""One unlit physical-aspect view per mature species before the full bake."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_whole_products import BASE, load_catalog
from conifer_window_render import plan_wood, tree_image
from conifer_windows import plan_windows
from mature_conifer_recipes import prepare
from mature_density import MAX_AXES, MAX_WINDOWS, WINDOW_SETTINGS
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern
from PIL import Image


def probe(output, catalogs, size=512):
    rows = []
    for case in prepare(output / "recipes"):
        if case["variant"] != 1:
            continue
        began = time.perf_counter()
        plant = NeedledPlant.from_recipe(json.loads(Path(case["recipe"]).read_text()))
        catalog, banks = load_catalog(catalogs / case["species"] / "catalog")
        bark = json.loads((BASE / case["species"] / "bark.json").read_text())
        with (
            patch.object(NeedledPlant, "foliage", side_effect=AssertionError("organs")),
            patch.object(NeedlePattern, "detail", side_effect=AssertionError("poses")),
            patch.object(Needle, "mesh", side_effect=AssertionError("needle meshes")),
        ):
            windows = plan_windows(
                plant, catalog, MAX_WINDOWS, max_axes=MAX_AXES, **WINDOW_SETTINGS
            )
            wood = plan_wood(plant, bark=bark)
            data, (frame, lower, upper, _) = tree_image(
                windows, banks, wood, np.array((0.8, 0.6, 0.0)), size, samples=2
            )
        np.testing.assert_allclose(frame[:, 1], (0, 0, 1), atol=1e-14)
        alpha = data[..., 3:4]
        colour = data[..., :3] + (1 - alpha) * np.array((0.77, 0.8, 0.82))
        colour = np.where(
            colour <= 0.0031308,
            colour * 12.92,
            1.055 * np.maximum(colour, 0) ** (1 / 2.4) - 0.055,
        )
        image = Image.fromarray(np.uint8(np.flipud(np.clip(colour, 0, 1)) * 255))
        image.save(output / f"{case['key']}-projection.png")
        extent = upper - lower
        width = max(1, round(size * extent[0] / extent[1]))
        aspect = image.resize((width, size), Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", (size, size), (227, 231, 234))
        canvas.paste(aspect, ((size - width) // 2, 0))
        canvas.save(output / f"{case['key']}-aspect.png")
        row = {
            "key": case["key"],
            "identity": plant.identity,
            "density": case["density"],
            "covered_projection_m2": float(alpha.sum() * np.prod(extent) / size**2),
            "projection_extent_m": extent.tolist(),
            "windows": len(windows["origins"]),
            "needles": int(windows["needle_counts"].sum()),
            "cpu_elapsed_s": time.perf_counter() - began,
            "scope": "unlit diagnostic; square projection rescaled to physical aspect; not a showcase or shading approval",
        }
        rows.append(row)
        print(json.dumps(row), flush=True)
    (output / "crown-probes.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--catalogs", required=True, type=Path)
    args = parser.parse_args()
    probe(args.output, args.catalogs)
