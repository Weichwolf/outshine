"""Brute-force prism control for the isolated native shadow-atlas outliers."""

import argparse
import json
import sys
import tempfile
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from bench_box_field import load


def bounds(heights, pitch, fill, native):
    row, column = np.nonzero(heights)
    grid = np.column_stack((column, row))
    step, coverage = np.float32(pitch), np.float32(fill)
    if native:
        centre = ((grid + 0.5) * pitch).astype(np.float32)
        half = np.float32(pitch * fill) * np.float32(0.5)
        low, high = centre - half, centre + half
    else:
        inset = np.float32(1 - coverage) * step * np.float32(0.5)
        low = grid.astype(np.float32) * step + inset
        high = (grid + 1).astype(np.float32) * step - inset
    minimum = np.column_stack((low[:, 0], np.zeros(len(row)), low[:, 1]))
    maximum = np.column_stack((high[:, 0], heights[row, column], high[:, 1]))
    return minimum.astype(float), maximum.astype(float), grid


def nearest(origin, direction, minimum, maximum, floor_extent):
    # Independent all-prism comparison: no grid walk or hierarchy pruning.
    parallel = direction == 0
    a, b = np.empty_like(minimum), np.empty_like(maximum)
    np.divide(minimum - origin, direction, out=a, where=~parallel)
    np.divide(maximum - origin, direction, out=b, where=~parallel)
    a[:, parallel], b[:, parallel] = -np.inf, np.inf
    entry = np.minimum(a, b).max(axis=1)
    leave = np.maximum(a, b).min(axis=1)
    entry[(entry < 0) | (entry > leave)] = np.inf
    if parallel.any():
        outside = np.any(((origin < minimum) | (origin > maximum)) & parallel, axis=1)
        entry[outside] = np.inf
    index = int(np.argmin(entry)) if entry.size else -1
    closest = entry[index] if index >= 0 else np.inf
    floor_t = -origin[1] / direction[1] if direction[1] < 0 else -1.0
    floor_point = origin + floor_t * direction
    floor_xy = floor_point[[0, 2]]
    if (
        floor_t > 0
        and np.all(floor_xy >= 0)
        and np.all(floor_xy <= floor_extent)
        and floor_t < closest
    ):
        edge = np.minimum(floor_xy, floor_extent - floor_xy).min()
        return floor_point, -1, float(edge)
    if not np.isfinite(closest):
        return None
    point = origin + entry[index] * direction
    faces = np.abs(np.concatenate((point - minimum[index], point - maximum[index])))
    return point, index, float(np.partition(faces, 1)[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--boxes", required=True, type=Path, help="case directory")
    parser.add_argument("--field", required=True, type=Path, help="case directory")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--rays", type=int, default=128)
    args = parser.parse_args()
    if not 1 <= args.rays <= 1024:
        parser.error("--rays must be 1..1024")
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("--output must be in the system temporary directory")
    if args.output.exists():
        parser.error("--output must be a fresh file")
    heights, pitch, fill = load(args.input)
    floor_extent = (np.array(heights.shape[::-1]) * pitch).astype(np.float32)
    frames = [
        np.loadtxt(p / "cold.shadow-frame", skiprows=2).reshape(4, 4).T
        for p in (args.boxes, args.field)
    ]
    assert np.array_equal(frames[0], frames[1])
    tile, atlas = np.loadtxt(args.boxes / "cold.shadow-frame", max_rows=1, dtype=int)
    arrays = [
        np.fromfile(p / "cold.shadow", dtype="<f4").reshape(atlas, atlas)
        for p in (args.boxes, args.field)
    ]
    difference = np.abs(arrays[0] - arrays[1])
    candidates = np.flatnonzero(difference > 1e-5)
    order = np.argsort(difference.reshape(-1)[candidates], kind="stable")[::-1]
    selected = candidates[order[: args.rays]]
    inverse = np.linalg.inv(frames[0])
    geometry = [bounds(heights, pitch, fill, native) for native in (True, False)]
    results = []
    for address in selected:
        y, x = divmod(int(address), int(atlas))
        assert x < tile and y < tile
        xy = np.array([(x + 0.5) * 2 / tile - 1, 1 - (y + 0.5) * 2 / tile])
        start = inverse @ np.r_[xy, 1.0, 1.0]
        finish = inverse @ np.r_[xy, 0.0, 1.0]
        origin = start[:3] / start[3]
        direction = finish[:3] / finish[3] - origin
        direction /= np.linalg.norm(direction)
        expected = []
        for mode, (minimum, maximum, grid) in enumerate(geometry):
            gpu_depth = arrays[mode][y, x]
            gpu_point = inverse @ np.r_[xy, gpu_depth, 1.0]
            gpu_point = gpu_point[:3] / gpu_point[3]
            outside = np.maximum(
                np.maximum(minimum - gpu_point, gpu_point - maximum), 0
            )
            distance = float(np.sqrt(np.sum(outside * outside, axis=1).min()))
            hit = nearest(origin, direction, minimum, maximum, floor_extent)
            if hit is None:
                expected.append(None)
                continue
            point, index, edge = hit
            clip = frames[0] @ np.r_[point, 1.0]
            expected.append(
                {
                    "depth": float(clip[2] / clip[3]),
                    "cell": grid[index].tolist() if index >= 0 else None,
                    "edge_distance_m": edge,
                    "gpu_point_nearest_prism_distance_m": distance,
                }
            )
        results.append(
            {
                "pixel": [x, y],
                "gpu": [float(a[y, x]) for a in arrays],
                "all_prism_controls": expected,
            }
        )
    report = {
        "candidate_depth_delta": 1e-5,
        "candidates": len(candidates),
        "sampled": len(results),
        "prisms_per_ray_per_geometry": len(geometry[0][0]),
        "scope": "all-prism diagnostic; not an error/SSE bound",
        "rays": results,
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print("outliers", len(candidates), "sampled", len(results), flush=True)


if __name__ == "__main__":
    main()
