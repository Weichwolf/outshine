"""Test camera-neutral gridded building masses against source silhouettes."""

import argparse
import json
import math
from pathlib import Path
from time import perf_counter

import numpy as np
from group_footprints import Extrusion, measure, read_source, triangles
from shapely import box as boxes
from shapely import intersects
from shapely.geometry import box


def runs(values):
    starts = np.r_[0, np.flatnonzero(np.diff(values)) + 1]
    stops = np.r_[starts[1:], len(values)]
    return (
        (int(a), int(b), int(values[a]))
        for a, b in zip(starts, stops)
        if values[a] != 0
    )


def rectangles(height):
    active = {}
    for y, values in enumerate(height):
        following = {}
        for key in runs(values):
            following[key] = active.pop(key, y)
        for (left, right, top), first in active.items():
            yield left, first, right, y, top
        active = following
    for (left, right, top), first in active.items():
        yield left, first, right, len(height), top


def exposed_walls(height):
    walls = 0
    for axes in (height, height.T):
        padded = np.pad(axes, ((1, 1), (0, 0)))
        near, far = padded[:-1], padded[1:]
        exposed = near != far
        changes = (near[:, 1:] != near[:, :-1]) | (far[:, 1:] != far[:, :-1])
        walls += np.count_nonzero(exposed[:, 0]) + np.count_nonzero(
            exposed[:, 1:] & changes
        )
    return int(walls)


def grid_mass(source, pitch_m, height_m):
    limits = np.asarray([piece.footprint.bounds for piece in source])
    low = np.floor(limits[:, :2] / pitch_m).astype(int)
    high = np.ceil(limits[:, 2:] / pitch_m).astype(int)
    origin = low.min(axis=0)
    width, height = high.max(axis=0) - origin
    if width * height > 4_000_000:
        raise ValueError("choose a smaller sample window")
    upper = np.zeros((height, width), dtype=np.int32)
    fallback = []
    for piece, start, end in zip(source, low - origin, high - origin, strict=True):
        if piece.bottom_m != 0:
            fallback.append(piece)
            continue
        left, near = start
        right, far = end
        x = (np.arange(left, right) + origin[0]) * pitch_m
        z = (np.arange(near, far) + origin[1]) * pitch_m
        cells = boxes(
            x[None, :], z[:, None], x[None, :] + pitch_m, z[:, None] + pitch_m
        )
        covered = intersects(piece.footprint, cells)
        if not covered.any():
            fallback.append(piece)
            continue
        top = math.ceil(piece.top_m / height_m)
        area = upper[near:far, left:right]
        np.maximum(area, top, out=area, where=covered)
    x, z = origin
    grouped = [
        Extrusion(
            box(
                (left + x) * pitch_m,
                (near + z) * pitch_m,
                (right + x) * pitch_m,
                (far + z) * pitch_m,
            ),
            0,
            top * height_m,
        )
        for left, near, right, far, top in rectangles(upper)
    ]
    faces = 2 * (len(grouped) + exposed_walls(upper)) + triangles(fallback)
    return fallback + grouped, len(fallback), upper.nbytes, faces


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("--extent-m", type=float, default=1600)
    parser.add_argument("--centre-m", nargs=2, type=float, default=(0, 0))
    args = parser.parse_args()
    if not math.isfinite(args.extent_m) or args.extent_m <= 0:
        parser.error("expected a finite positive half-extent")
    x, z = args.centre_m
    source = [
        part
        for part in read_source(args.source)
        if abs(part.footprint.centroid.x - x) <= args.extent_m
        and abs(part.footprint.centroid.y - z) <= args.extent_m
    ]
    if not source:
        parser.error("no source in the sample window")
    result, candidates = {}, {}
    for pitch in (2, 4, 8):
        began = perf_counter()
        pieces, original, workspace, faces = grid_mass(source, pitch, pitch)
        result[pitch] = {
            "fallback": original,
            "workspace_bytes": workspace,
            "triangles_exposed_surface": faces,
            "triangles_closed_prisms": triangles(pieces),
            "volume_error_m": math.hypot(pitch, pitch, pitch),
            "build_ms": (perf_counter() - began) * 1000,
        }
        candidates[pitch] = pieces
    for pitch, comparisons in measure(
        source, candidates, (4500, 9000, 18000), args.image
    ).items():
        result[pitch]["views"] = comparisons
    print(
        json.dumps(
            {
                "source": len(source),
                "reference_triangles": triangles(source),
                "plans": result,
            }
        )
    )


if __name__ == "__main__":
    main()
