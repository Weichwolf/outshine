"""Measure current-building depth rejection of unchanged cached terrain patches."""

import argparse
import csv
import json
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds, camera, load, occluded, project, raster
from terrain_grid_lod import page_geometry, triangles


def partition(cells):
    if cells not in (4, 8, 16, 32):
        raise ValueError("patch side must be 4, 8, 16 or 32 cells")
    faces = triangles(cells + 1, 1)
    grid = np.arange((cells + 1) ** 2).reshape(cells + 1, cells + 1)
    faces = (faces // (cells + 1)) * 33 + faces % (cells + 1)
    nodes = (grid // (cells + 1)) * 33 + grid % (cells + 1)
    offsets = np.array(
        [y * 33 + x for y in range(0, 32, cells) for x in range(0, 32, cells)]
    )
    return faces[None] + offsets[:, None, None], nodes.ravel()[None] + offsets[:, None]


def compare(structures, pages, width, height, cells, yaw, levels=4):
    if width <= 0 or height <= 0 or levels <= 0:
        raise ValueError("image dimensions must be positive")
    started = time.perf_counter()
    scene, vertices, faces, _, _ = load(structures)
    view = camera(scene, vertices, width, height, np.deg2rad(yaw))
    projected, z = project(vertices, view)
    depth = raster(projected, z, faces, width, height)
    full, retained = depth.copy(), depth.copy()
    patches, nodes = partition(cells)
    baseline = triangles(33, 1)
    anchor = np.array(scene["anchor_ecef"])
    totals = {"pages": 0, "patches": 0, "frustum_patches": 0, "hidden_patches": 0}
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    for row in rows:
        if int(row["side"]) != 33:
            raise ValueError("reference requires native 33x33 terrain pages")
        _, _, _, world, _ = page_geometry(row, pages)
        vertices = world - anchor
        points = vertices[nodes]
        b = points.min(axis=1), points.max(axis=1)
        lo, hi, nearest, safe, visible, _ = box_bounds(b, view)
        hidden = occluded(lo, hi, nearest, safe, depth, levels) & visible
        totals["pages"] += 1
        totals["patches"] += len(patches)
        totals["frustum_patches"] += int(visible.sum())
        totals["hidden_patches"] += int(hidden.sum())
        projected, z = project(vertices, view)
        page_depth = raster(projected, z, baseline, width, height)
        full = np.maximum(full, page_depth)
        selected = patches[visible & ~hidden].reshape(-1, 3)
        if np.all(visible & ~hidden):
            culled_depth = page_depth
        else:
            culled_depth = raster(projected, z, selected, width, height)
        retained = np.maximum(retained, culled_depth)
    difference = np.abs(full - retained)
    report = {
        **totals,
        "asset": scene["asset"],
        "width": width,
        "height": height,
        "yaw": yaw,
        "cells": cells,
        "levels": levels,
        "input_terrain_triangles": len(rows) * 2048,
        "frustum_triangles": totals["frustum_patches"] * cells * cells * 2,
        "hidden_triangles": totals["hidden_patches"] * cells * cells * 2,
        "changed_pixels": int(np.count_nonzero(difference > 1e-12)),
        "max_depth_difference": float(difference.max()),
        "seconds": time.perf_counter() - started,
        "scope": "One opaque building asset, cached terrain interiors, pixel-centre depth; "
        "skirts, alpha, MSAA, shader rounding and device cost are not proved.",
    }
    return report, depth, full, retained


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--structures", type=Path, required=True)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--cells", type=int, choices=(4, 8, 16, 32), default=32)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--levels", type=int, default=4)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report, buildings, reference, result = compare(
        args.structures,
        args.pages,
        args.width,
        args.height,
        args.cells,
        args.yaw,
        args.levels,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        args.output / "depths.npz",
        buildings=buildings,
        reference=reference,
        culled=result,
    )
    print(json.dumps(report))


if __name__ == "__main__":
    main()
