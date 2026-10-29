"""Identify newly exposed original geometry independently of capture quantization."""

import argparse
import json
from pathlib import Path

import numpy as np
from field_view_inputs import read_view
from triangle_image_reference import (
    intersect_source,
    prepare_source,
    visible_pixel_bounds,
)


def original_mesh(path):
    parts = json.loads((path / "input.json").read_text())["parts"]
    vertices, faces, offset = [], [], 0
    for i, part in enumerate(parts):
        positions = np.fromfile(path / str(i) / "positions.bin", dtype="<f4").reshape(
            part["vertices"], 3
        )
        indices = np.fromfile(path / str(i) / "indices.bin", dtype="<u4").reshape(
            part["triangles"], 3
        )
        if not np.isfinite(positions).all() or (
            len(indices) and indices.max() >= len(positions)
        ):
            raise ValueError("Invalid unchanged native original geometry")
        vertices.append(positions)
        faces.append(indices.astype(np.int64) + offset)
        offset += len(positions)
    return {"positions": np.concatenate(vertices), "indices": np.concatenate(faces)}


def exposed(source, prepared, grid, query, *, samples_axis):
    eye, basis, focal = (
        np.asarray(query["eye"]),
        np.asarray(query["basis"]),
        query["focal_px"],
    )
    view = (eye, basis, focal, 1280, 720)
    box = visible_pixel_bounds(source["positions"], view)
    width, height = box[1] - box[0]
    if min(width, height) <= 0:
        raise ValueError("The original visibility region is outside the image")
    count = int(width * height * samples_axis**2)
    if not 1 <= samples_axis <= 32 or count > 100000:
        raise ValueError("Original visibility-ray capacity exceeded before expansion")
    x = box[0, 0] + (np.arange(width * samples_axis) + 0.5) / samples_axis
    y = box[0, 1] + (np.arange(height * samples_axis) + 0.5) / samples_axis
    xx, yy = np.meshgrid(x, y)
    direction = (
        basis[2]
        + ((xx.ravel() - 640) / focal)[:, None] * basis[0]
        - ((yy.ravel() - 360) / focal)[:, None] * basis[1]
    )
    depth, _ = intersect_source(
        source, np.broadcast_to(eye, direction.shape), direction, prepared=prepared
    )
    valid = np.isfinite(depth)
    point = eye + depth[valid, None] * direction[valid]
    toward = np.asarray(grid.toward_eye)
    signed_height = (point - grid.centre) @ toward
    flat = point - signed_height[:, None] * toward
    origins = flat + 3 * grid.half_extent_m * toward
    capture_depth, _ = intersect_source(
        source, origins, np.broadcast_to(-toward, origins.shape), prepared=prepared
    )
    capture_height = 3 * grid.half_extent_m - capture_depth
    hidden = ~np.isfinite(capture_depth) | (capture_height - signed_height > 0.001)
    behind = np.isfinite(capture_depth) & (capture_height - signed_height > 0.001)
    # Native base-frame centre hits provide a distinct geometric opponent. This
    # comparison is not used to overwrite the finite supersampled colour oracle.
    centre_x, centre_y = np.meshgrid(
        np.arange(box[0, 0], box[1, 0]) + 0.5, np.arange(box[0, 1], box[1, 1]) + 0.5
    )
    rays = (
        basis[2]
        + ((centre_x.ravel() - 640) / focal)[:, None] * basis[0]
        - ((centre_y.ravel() - 360) / focal)[:, None] * basis[1]
    )
    centre_depth, _ = intersect_source(
        source, np.broadcast_to(eye, rays.shape), rays, prepared=prepared
    )
    return {
        "query": query["name"],
        "profile": [1280, 720],
        "subpixel_axis": samples_axis,
        "rays": count,
        "original_hits": int(valid.sum()),
        "points_not_first_visible_from_capture": int(hidden.sum()),
        "without_capture_front_hit": int((~np.isfinite(capture_depth)).sum()),
        "behind_capture_first_hit": int(behind.sum()),
        "minimum_positive_occlusion_m": float(
            (capture_height - signed_height)[behind].min()
        )
        if behind.any()
        else None,
        "newly_visible_fraction_of_original_hits": float(hidden.mean())
        if len(hidden)
        else None,
        "centre_origin": box[0].tolist(),
        "centre_shape": [int(height), int(width)],
        "centre_forward_depth_m": [
            float(v) if np.isfinite(v) else None for v in centre_depth
        ],
        "scope": "Same Float32 original triangles at both viewpoints; front-face first intersections, 1mm signed-depth tolerance. Geometric visibility only, no capture quantization/shading oracle or global newly-visible-area bound.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("geometry", "field", "queries", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--held-query", required=True)
    parser.add_argument("--query", action="append", required=True)
    args = parser.parse_args()
    state = next(
        r
        for r in json.loads((args.field / "radiance-result.json").read_text())["cases"]
        if r["query"] == args.held_query
    )
    atlas = read_view(args.field / "world-view-assets.sqlite", state["field_asset_key"])
    source = original_mesh(args.geometry)
    prepared = prepare_source(source)
    queries = json.loads(args.queries.read_text())["queries"]
    rows = [
        exposed(source, prepared, atlas["grid"], q, samples_axis=8)
        for q in queries
        if q["name"] in args.query
    ]
    if {r["query"] for r in rows} != set(args.query):
        raise ValueError("An original visibility query is missing")
    args.output.write_text(json.dumps(rows, indent=2) + "\n")
    print(
        json.dumps(
            [{k: v for k, v in r.items() if not k.startswith("centre_")} for r in rows]
        )
    )


if __name__ == "__main__":
    main()
