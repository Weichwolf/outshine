"""Measure hidden building groups in a flat-datum scene made from source footprints.

Walls are assumed ready, solid and two-sided. The counts describe grouped
source bounds. Terrain, material coverage and native cache requests are outside
this experiment.
"""

import argparse
import json
from itertools import pairwise, product
from pathlib import Path
from time import perf_counter

import numpy as np
from group_footprints import read_source
from outshine_ref.geometry.occlusion import classify_boxes, convex_patch_umbra
from shapely import bounds as polygon_bounds
from tile_selection import camera_samples


def bounds_of(parts):
    horizontal = polygon_bounds([part.footprint for part in parts])
    heights = np.asarray([(p.bottom_m, p.top_m) for p in parts])
    low = np.column_stack((horizontal[:, 0], heights[:, 0], horizontal[:, 1]))
    high = np.column_stack((horizontal[:, 2], heights[:, 1], horizontal[:, 3]))
    return np.stack((low, high), axis=1)


def walls_of(parts, boxes, eye, reach_m):
    centres = boxes.mean(axis=1)
    candidates = np.flatnonzero(np.linalg.norm(centres - eye, axis=1) < reach_m)
    for i in candidates:
        part = parts[i]
        ring = np.asarray(part.footprint.exterior.coords)
        low, high = part.bottom_m, part.top_m
        for a, b in pairwise(ring):
            if not np.array_equal(a, b):
                yield (
                    (a[0], low, a[1]),
                    (b[0], low, b[1]),
                    (b[0], high, b[1]),
                    (a[0], high, a[1]),
                )


def select_walls(faces, observer, limit):
    if not len(faces):
        return ()
    eye = observer.mean(axis=0)
    normal = np.cross(faces[:, 1] - faces[:, 0], faces[:, 3] - faces[:, 0])
    delta = eye - faces.mean(axis=1)
    facing = np.abs(np.einsum("ij,ij->i", delta, normal))
    score = facing / np.maximum(1, np.linalg.norm(delta, axis=1)) ** 3
    selected = faces[score.argsort()[-limit:][::-1]]
    return tuple(
        (
            face,
            convex_patch_umbra(face, observer, certified_opaque=True, two_sided=True),
        )
        for face in selected
    )


def visible(camera, boxes):
    center = boxes.mean(axis=1)
    radii = (boxes[:, 1] - boxes[:, 0]) * 0.5
    position = (center - camera.eye) @ camera.axes
    extent = radii @ np.abs(camera.axes)
    far = position[:, 2] + extent[:, 2]
    lateral = np.abs(position[:, :2]) - extent[:, :2] <= far[:, None] * camera.slopes
    return (far >= camera.near_m) & lateral.all(axis=1)


def corners(box):
    return np.asarray(tuple(product(*box.T)))


def check_wall_intersection(box, wall, observer):
    horizontal = wall[1] - wall[0]
    normal = np.cross(horizontal, wall[3] - wall[0])
    for origin in corners(observer):
        rays = corners(box) - origin
        t = ((wall[0] - origin) @ normal) / (rays @ normal)
        hit = origin + t[:, None] * rays
        u = (hit - wall[0]) @ horizontal / (horizontal @ horizontal)
        assert ((t > 0) & (t < 1) & (u > 0) & (u < 1)).all()
        assert ((wall[0, 1] < hit[:, 1]) & (hit[:, 1] < wall[2, 1])).all()


def hidden(boxes, certificates, observer):
    hidden = np.zeros(len(boxes), dtype=bool)
    for wall, umbra in certificates:
        if not len(umbra.low):
            continue
        coefficients = (umbra.low + umbra.high) * 0.5
        normal = coefficients[:, :3]
        d = coefficients[:, 3] + boxes[:, 0] @ np.maximum(normal.T, 0)
        d += boxes[:, 1] @ np.minimum(normal.T, 0)
        candidates = np.flatnonzero(~hidden & (d > 0).all(axis=1))
        for start in range(0, len(candidates), 512):
            batch = candidates[start : start + 512]
            proven, _ = classify_boxes(boxes[batch], umbra)
            accepted = batch[proven]
            if accepted.size:
                check_wall_intersection(boxes[accepted[0]], wall, observer)
                check_wall_intersection(boxes[accepted[-1]], wall, observer)
            hidden[accepted] = True
    return hidden


def grouped(boxes, metres):
    if not len(boxes):
        empty = np.empty(0, dtype=np.intp)
        return boxes, empty, empty
    points = np.floor(boxes.mean(axis=1)[:, (0, 2)] / metres).astype(np.int64)
    order = np.lexsort((points[:, 1], points[:, 0]))
    addresses = points[order]
    cuts = np.flatnonzero(np.any(addresses[1:] != addresses[:-1], axis=1)) + 1
    starts = np.r_[0, cuts]
    low = np.minimum.reduceat(boxes[order, 0], starts, axis=0)
    high = np.maximum.reduceat(boxes[order, 1], starts, axis=0)
    counts = np.diff(np.r_[starts, len(boxes)])
    return np.stack((low, high), axis=1), counts, order


def measure(boxes, faces, observer, options):
    started = perf_counter()
    certificates = select_walls(faces, observer, options.walls)
    leaves = hidden(boxes, certificates, observer)
    result = []
    for metres in options.tile_m:
        groups, counts, order = grouped(boxes, metres)
        selected = hidden(groups, certificates, observer)
        assert leaves[order][np.repeat(selected, counts)].all()
        result.append(
            {
                "tile_m": metres,
                "groups": len(groups),
                "hidden_groups": int(selected.sum()),
                "hidden_source_bounds": int(counts[selected].sum()),
            }
        )
    return {
        "source_bounds": len(boxes),
        "hidden_source_bounds": int(leaves.sum()),
        "observers_span_m": (observer[1] - observer[0]).tolist(),
        "certificates": sum(bool(len(umbra.low)) for _, umbra in certificates),
        "requests": result,
        "analysis_ms": (perf_counter() - started) * 1000,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--tile-m", type=int, nargs="+", default=(32, 64, 128, 256))
    parser.add_argument("--walls", type=int, default=16)
    parser.add_argument("--occluder-reach-m", type=float, default=1500.0)
    options = parser.parse_args()
    if (
        min(options.tile_m) <= 0
        or options.walls <= 0
        or not options.occluder_reach_m > 0
    ):
        parser.error("expected positive tile sizes, wall budget and reach")
    parts = tuple(read_source(options.source))
    if not parts:
        parser.error("sample needs source geometry")
    bounds = bounds_of(parts)
    report = json.loads(options.report.read_text())
    motion, profile = report["motion"], report["profile"]
    views = tuple(camera_samples(motion, profile, motion["start"]["near_m"], 240000))
    live = np.logical_or.reduce([visible(camera, bounds) for camera in views])
    eyes = np.asarray([view.eye for view in views])
    domain = np.stack((eyes.min(axis=0), eyes.max(axis=0)))
    faces = np.asarray(
        tuple(walls_of(parts, bounds, domain.mean(axis=0), options.occluder_reach_m))
    )
    result = measure(bounds[live], faces, domain, options)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
