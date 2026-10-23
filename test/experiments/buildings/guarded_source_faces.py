"""Bound wall-facing selection over an eye ball before native mesh generation."""

import argparse
import hashlib
import json
import math
import time
from pathlib import Path

import numpy as np


def wall_planes(ring, hole=False):
    points = np.asarray(ring, dtype=float).reshape(-1, 2)
    if len(points) and np.array_equal(points[0], points[-1]):
        points = points[:-1]
    if len(points) < 3 or not np.isfinite(points).all():
        raise ValueError("finite polygon ring required")
    following = np.roll(points, -1, axis=0)
    area = np.sum(points[:, 0] * following[:, 1] - points[:, 1] * following[:, 0])
    orientation = 1.0 if ((area >= 0) != hole) else -1.0
    edge = following - points
    return points, orientation * np.column_stack((edge[:, 1], -edge[:, 0]))


def facing_in_ball(points, normals, eye, radius):
    if not math.isfinite(radius) or radius < 0:
        raise ValueError("finite nonnegative eye radius required")
    distance = np.sum((eye - points) * normals, axis=1)
    return distance + radius * np.linalg.norm(normals, axis=1) >= 0


def cached_planes(receipts, latitude, longitude):
    import mapbox_vector_tile

    starts, normals = [], []
    footprints = holes = 0
    for receipt in receipts:
        raw = Path(receipt["path"]).read_bytes()
        if hashlib.sha256(raw).hexdigest() != receipt["sha256"]:
            raise ValueError("source receipt digest differs")
        zoom, x, y = receipt["tile"]
        side = 1 << zoom
        centre = np.array(
            [
                (longitude + 180) / 360 * side,
                (1 - math.asinh(math.tan(math.radians(latitude))) / math.pi)
                / 2 * side,
            ]
        )
        span = 2 * math.pi * 6378137 / side * math.cos(math.radians(latitude))
        layer = mapbox_vector_tile.decode(
            raw, default_options={"y_coord_down": True}
        ).get("building", {})
        extent = layer.get("extent", 4096)
        for feature in layer.get("features", []):
            geometry = feature["geometry"]
            polygons = (
                [geometry["coordinates"]] if geometry["type"] == "Polygon"
                else geometry["coordinates"] if geometry["type"] == "MultiPolygon"
                else []
            )
            for polygon in polygons:
                footprints += 1
                for index, ring in enumerate(polygon):
                    local = (np.asarray(ring) / extent + [x, y] - centre) * [span, -span]
                    points, directions = wall_planes(local, hole=index > 0)
                    starts.append(points)
                    normals.append(directions)
                    holes += index > 0
    if not starts:
        raise ValueError("nonempty building footprint input required")
    return np.concatenate(starts), np.concatenate(normals), footprints, holes


def measure(points, normals, radius):
    eye = np.zeros(2)
    point = facing_in_ball(points, normals, eye, 0)
    guarded = facing_in_ball(points, normals, eye, radius)
    support = np.sum(-points * normals, axis=1) + radius * np.linalg.norm(normals, axis=1)
    if np.any((~guarded) & (support >= 0)):
        raise AssertionError("discarded face intersects the analytic eye support")
    rng = np.random.default_rng(782)
    violations = 0
    for _ in range(32):
        angle = rng.uniform(0, 2 * math.pi)
        offset = radius * math.sqrt(rng.uniform()) * np.array([math.cos(angle), math.sin(angle)])
        violations += np.count_nonzero(facing_in_ball(points, normals, offset, 0) & ~guarded)
    if violations:
        raise AssertionError("a sampled eye exposes a discarded wall")
    samples = []
    for _ in range(21):
        began = time.perf_counter()
        facing_in_ball(points, normals, eye, radius)
        samples.append((time.perf_counter() - began) * 1000)
    return {
        "wall_faces": len(points),
        "point_faces": int(point.sum()),
        "guarded_faces": int(guarded.sum()),
        "radius_m": radius,
        "sampled_poses": 32,
        "false_negative_faces": int(violations),
        "selection_ms_p50": float(np.median(samples)),
        "scope": "Wall planes only; local Mercator metric approximation. No roof, floor, "
                 "occlusion, native LOD, shadow or GPU guarantee. Tile occurrences are not unique IDs.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipts", type=Path, required=True)
    parser.add_argument("--latitude", type=float, required=True)
    parser.add_argument("--longitude", type=float, required=True)
    parser.add_argument("--radius", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    points, normals, footprints, holes = cached_planes(
        json.loads(args.receipts.read_text()), args.latitude, args.longitude
    )
    result = measure(points, normals, args.radius)
    result.update(footprint_occurrences=footprints, hole_rings=holes)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
