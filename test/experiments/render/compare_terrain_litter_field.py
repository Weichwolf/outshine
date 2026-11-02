"""Compare a stored scalar mask against the existing complete material composition."""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from compare_terrain_material_filter import error
from probe_terrain_material_substrate import optical_row
from relief_shading_reference import direction
from terrain_litter_field import bake, sample
from terrain_material_filter import response
from terrain_material_litter import compose, footprint_field


def light_response(raw, dx, dy):
    raw = raw.copy()
    raw[:, 4:] = np.linalg.solve(np.stack((dx, dy), axis=1), raw[:, 4:, None])[..., 0]
    lights = np.array([direction(0), direction(60, 37), direction(80, 170)])
    return np.array(
        [response(row[None], np.ones(1), lights, direction(40)) for row in raw]
    )


def compare(capture):
    palette, layout, receipt = load_capture(capture)
    anchor = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    points = np.random.default_rng(1934).uniform(-4, 4, (64, 2))
    records, products, unused = [], {}, []
    for row in range(layout["rows"]):
        recipe = optical_row(palette, layout, row)
        scale = recipe["recipe"][2]
        if not 0 < recipe["coverage"] < 1 or scale <= 0:
            unused.append(row)
            continue
        origin = np.floor(anchor / (16 * scale)) * (16 * scale) - 8 * scale
        at = anchor + points * scale
        for width in (0.005, 0.05, 0.25, 0.75, 1.5, 1.99, 2, 4):
            for angle, anisotropy, shear in (
                (0, 1, 0),
                (0.731, 1, 0),
                (0.731, 16, 0),
                (0.731, 4, 0.7),
            ):
                u, v = np.array(
                    [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
                )
                dx = np.tile(u * width * scale, (len(at), 1))
                dy = np.tile((v / anisotropy + shear * u) * width * scale, (len(at), 1))
                mask = footprint_field(at, dx, dy, scale)
                expected = light_response(compose(at, dx, dy, **recipe), dx, dy)
                for dtype in ("float16", "float32"):
                    key = (float(scale), dtype)
                    if key not in products:
                        products[key] = bake(origin, scale, dtype=dtype)
                    field = products[key]
                    for bias in (0, -0.5):
                        cached = sample(field, at, dx, dy, lod_bias=bias)
                        actual = light_response(
                            compose(at, dx, dy, **recipe, mask_values=cached), dx, dy
                        )
                        records.append(
                            {
                                "row": row,
                                "scale_m": float(scale),
                                "width_scales": width,
                                "anisotropy": anisotropy,
                                "shear": shear,
                                "angle": angle,
                                "dtype": dtype,
                                "lod_bias": bias,
                                "retained_bytes": field.retained_bytes,
                                "mask_error": error(mask, cached),
                                "linear_rgb_error": error(expected, actual),
                            }
                        )
    return receipt, records, unused, products


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private field outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    receipt, records, unused, products = compare(args.capture)
    for index, field in enumerate(products.values()):
        np.savez(output / f"field-{index}.npz", *field.levels)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "palette": receipt["palette"],
        "skipped_before_generation": unused,
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "terrain_litter_field.py",
                "terrain_material_litter.py",
            )
        },
        "products": [
            {
                "origin_m": f.origin_m,
                "extent_m": f.extent_m,
                "scale_m": f.scale_m,
                "dtype": str(f.levels[0].dtype),
                "bytes": f.retained_bytes,
            }
            for f in products.values()
        ],
        "limitations": [
            "Stored scalar mask only; remaining material recipes are unchanged and not yet cached.",
            "Controlled metric patches, no class/rock blend, native environment or world frame-time acceptance.",
            "CPU ellipse-axis filter approximates the original oriented nine-point filter; not a native sampler proof.",
            "No AssetDB identity/admission, page halos, GPU lifetime or device budget acceptance.",
        ],
        "records": records,
    }
    (output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "comparison.json")


if __name__ == "__main__":
    main()
