"""Rejected point-sampled Voronoi source control; no filter or material admission."""

import argparse
import itertools
import json
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import shapely
from asphalt_angular import coverage, pixel_bounds
from asphalt_field import parameters
from asphalt_filter import srgb
from asphalt_lighting_reference import specular


def controls():
    period, count, resolution = 16, 4096, 512
    rng = np.random.default_rng(781)
    points = rng.uniform(0, period, (count, 2))
    weights = rng.uniform(0.75, 1.25, count)
    offsets = np.array(list(itertools.product([-1, 0, 1], repeat=2))) * period
    all_points = np.concatenate([points + offset for offset in offsets])
    cells = shapely.get_parts(
        shapely.voronoi_polygons(shapely.multipoints(all_points), ordered=True)
    )[4 * count : 5 * count]
    for shrink in (0.85, 0.95):
        field, dc = np.zeros((resolution, resolution)), 0.0
        for point, weight, polygon in zip(points, weights, cells, strict=True):
            vertices = point + shrink * (
                np.asarray(polygon.exterior.coords)[:-1] - point
            )
            a, b = vertices[1] - vertices[0], vertices[2] - vertices[1]
            if a[0] * b[1] - a[1] * b[0] < 0:
                vertices = vertices[::-1]
            dc += 0.75 * weight * shapely.Polygon(vertices).area
            for offset in offsets:
                translated = vertices + offset
                box = pixel_bounds(translated, resolution / period, resolution)
                x0, y0, x1, y1 = box
                if x1 > x0 and y1 > y0:
                    field[y0:y1, x0:x1] += (
                        0.75
                        * weight
                        * coverage(translated, box, resolution / period, "point")
                    )
        yield shrink, field - dc / period**2, points, weights


def run(output):
    figure, axes = plt.subplots(1, 2, figsize=(10, 5), constrained_layout=True)
    rows = []
    fresnel = 0.04 + 0.96 * (1 - np.sqrt(0.9)) ** 5
    for ax, (shrink, field, points, weights) in zip(axes, controls(), strict=True):
        np.savez_compressed(
            output / f"voronoi-{shrink}.npz",
            field=field,
            points=points,
            weights=weights,
        )
        colour, roughness = parameters(field)
        light = colour * (1 - fresnel) * 0.8 / np.pi + specular(roughness)[..., None]
        ax.imshow(np.clip(srgb(light * 4), 0, 1))
        ax.set_title(f"Core scale {shrink}; 0.384×0.384 m")
        ax.axis("off")
        rows.append(
            {
                "shrink": shrink,
                "mean": float(field.mean()),
                "minimum": float(field.min()),
                "maximum": float(field.max()),
            }
        )
    figure.suptitle(
        "Rejected spatial source: point sampling / fixed MR fixture, no filter or native approval"
    )
    figure.savefig(output / "voronoi-control.png", dpi=150)
    plt.close(figure)
    (output / "report.json").write_text(
        json.dumps(
            {
                "scope": "Visual rejection control only. Point-sampled source; unverified filter/periodic partition/BRDF/production costs. Authored fixed-count uniform periodic seed set, not measured asphalt gradation.",
                "seed": 781,
                "points": 4096,
                "period_cells": 16,
                "cell_pitch_m": 0.024,
                "variants": rows,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    target.mkdir(parents=True)
    run(target)
