"""Probe logical field heights separately from terrain-displaced raster positions."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

RADIUS_M = 6378137.0


def displacements(x):
    return (
        ("flat", np.zeros_like(x)),
        ("slope", x * 0.004),
        ("parabolic curvature", -x * x / (2 * RADIUS_M)),
    )


def coordinate_checks(x, tops):
    fractions = np.array([-0.25, 0.125, 0.5, 0.875, 1.25])[:, None]
    logical = fractions * tops
    expected = (tops > 0) & (fractions >= 0) & (fractions <= 1)
    checks = 0
    for _, displacement in displacements(x):
        for datum in (-500, 0, 1200):
            base = (displacement + datum).astype(np.float32)
            placed = (logical + base).astype(np.float32)
            restored = placed - base
            selected = (tops > 0) & (restored >= 0) & (restored <= tops)
            np.testing.assert_array_equal(selected, expected)
            checks += expected.size
    return checks


def render_case(x, tops, inside, base, origin, height):
    physical_y = np.linspace(90, -50, height)[:, None]
    logical_y = physical_y - base
    correct = inside & (tops > 0) & (logical_y >= 0) & (logical_y <= tops)
    fixed_origin_y = physical_y - origin
    naive = correct & (fixed_origin_y >= 0) & (fixed_origin_y <= tops)
    images = []
    for mask in (correct, naive):
        pixels = np.full((height, len(x), 3), [113, 157, 183], dtype=np.uint8)
        pixels[np.broadcast_to(physical_y < base, mask.shape)] = [70, 88, 67]
        pixels[mask] = [198, 194, 180]
        images.append(Image.fromarray(pixels))
    return images, {
        "occupied_samples": int(correct.sum()),
        "naive_lost_samples": int((correct & ~naive).sum()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    output = parser.parse_args().output.resolve()
    if not any(
        output.is_relative_to(root.resolve())
        for root in (Path(tempfile.gettempdir()), Path("/tmp"))
    ):
        parser.error("output must be in the system temp directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    width, height, cells, pitch = 640, 220, 64, 256.0
    x = (np.arange(width) + 0.5) / width * cells * pitch
    centred = x - cells * pitch / 2
    cell = np.floor(x / pitch).astype(int)
    inside = np.abs(x / pitch - cell - 0.5) <= 0.375
    tops = np.array([0.0, 6.0, 12.0, 24.0, 48.0])[1 + (cell * 13 % 4)]
    tops[(cell % 11) == 0] = 0
    checks = coordinate_checks(centred, tops)
    canvas = Image.new("RGB", (width * 2, (height + 20) * 3), "#171c24")
    rows = []
    for row, (name, displacement) in enumerate(displacements(centred)):
        origin = 0 if row == 0 else 30
        images, report = render_case(
            x, tops, inside, displacement + origin, origin, height
        )
        rows.append({"case": name, **report})
        for column, image in enumerate(images):
            canvas.paste(image, (column * width, row * (height + 20) + 20))
        ImageDraw.Draw(canvas).text((8, row * (height + 20) + 3), name, fill="white")
    assert rows[0]["naive_lost_samples"] == 0
    assert all(row["naive_lost_samples"] > 0 for row in rows[1:])
    ImageDraw.Draw(canvas).text(
        (width + 8, 3),
        "Invalid mapping: displaced Y used as logical height",
        fill="white",
    )
    canvas.save(output / "field-height-coordinates.png")
    report = {
        "scope": "coordinate counterexample; no native shader/DEM/World qualification",
        "float32_point_checks": checks,
        "cases": rows,
        "patch_width_m": pitch,
        "parabolic_reference_linear_error_bound_m": pitch**2 / (8 * RADIUS_M),
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
