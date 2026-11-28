"""Replay flat impostor admission with depth restricted to the stored source bounds."""

import argparse
import itertools
import json
import sqlite3
import struct
import xml.etree.ElementTree as ET
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from impostor_depth import content
from impostor_support import lens
from tile_selection import camera_path, local_frame

CORNERS = np.asarray(list(itertools.product((0, 1), repeat=3)))


def corners(low, high):
    return np.where(CORNERS, high, low)


def capture_axes(toward):
    right = np.cross([0, 1, 0], toward)
    if np.linalg.norm(right) <= 1e-12:
        right = np.cross([0, 0, 1], toward)
    right /= np.linalg.norm(right)
    return np.column_stack((right, np.cross(toward, right), toward))


def verify_depth(field, pixels, half_extent, low, high):
    magic, version, _, width, count, materials = struct.unpack_from("<QIQIII", field)
    assert magic == 0x004E574F5243534F and width == pixels and count == 1
    assert version in (1, 2, 3, 4)
    offset = 64 + materials * (68 if version >= 3 else 52) + 24
    layout = np.dtype([("normal", "<f4", 3), ("depth", "<f4"), ("surface", "<u4")])
    texels = np.frombuffer(field, layout, pixels * pixels, offset)
    depths = (
        texels["depth"][texels["surface"] != 0].astype(np.float64) * 4 - 2
    ) * half_extent
    if len(depths):
        assert low <= depths.min() and depths.max() <= high, (
            low,
            high,
            depths.min(),
            depths.max(),
        )
    return len(depths)


def replay_bounds(eye, centre, axes, low, high, focal, scale, near, far):
    points = corners(low, high)
    points = centre - eye + points @ axes.T
    axis = axes[:, 2]
    heights = corners(low, high)[:, 2]
    supports = points - heights[:, None] * axis
    outside = np.concatenate((points, supports))
    lo, hi = outside.min(axis=0), outside.max(axis=0)
    near_distance = np.linalg.norm(np.maximum(lo, np.maximum(0, -hi)))
    depth = near_distance / scale - np.linalg.norm(hi - lo)
    greatest = np.linalg.norm(np.maximum(abs(lo), abs(hi)))
    if depth < near or (far and greatest > far) or np.dot(eye - centre, axis) <= 0:
        return np.inf
    span = max(abs(heights))
    perpendicular = np.linalg.norm(np.cross(points, axis), axis=1).max()
    return np.sqrt(2) * focal * span * perpendicular / depth**2


def measure(db, anchor, start, finish, radius, projection):
    positions = tuple(start + (finish - start) * t for t in np.linspace(0, 1, 5))
    demand = tuple(float(x + d) for x in anchor for d in (radius, -radius))
    rows = db.execute(
        """
        SELECT a.key,a.minx,a.miny,a.minz,a.maxx,a.maxy,a.maxz,f.bytes,f.key
        FROM bounds b JOIN assets a ON a.id=b.id JOIN assets f ON f.parent=a.key
        WHERE b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=?
          AND b.minz<=? AND b.maxz>=? AND a.kind='fractional-impostor-views-1'
          AND f.kind='impostor-material-view-1'
        GROUP BY a.key ORDER BY a.key
        """,
        demand,
    )
    samples = []
    origins = set()
    checked = 0
    for key, *coordinates, bytes_count, field_key in rows:
        bounds = np.asarray(coordinates).reshape(2, 3) - anchor
        data = content(db, key)
        format_id, provenance_length = struct.unpack_from("<QI", data)
        if format_id != 0x000231565046:
            continue
        at = 12 + provenance_length + 48
        *centre, half_extent, pixels, count = struct.unpack_from("<4d2I", data, at)
        if count != 1:
            continue
        direction = np.asarray(struct.unpack_from("<3d", data, at + 40))
        frame = np.frombuffer(data, "<f8", 16, at + 64).reshape(4, 4).T
        basis = capture_axes(direction)
        rotation = frame[:3, :3] @ basis
        centre = frame[:3, 3] - anchor + frame[:3, :3] @ centre
        depth = (corners(*bounds) - centre) @ np.linalg.inv(rotation).T
        full = half_extent * np.asarray([1, 1, 2])
        # Leave the full width and height available to the filtered alpha texture.
        low, high = -full.copy(), full.copy()
        padding = max(1, half_extent) * 128 * np.finfo(np.float32).eps
        low[2] = max(low[2], depth[:, 2].min() - padding)
        high[2] = min(high[2], depth[:, 2].max() + padding)
        if low[2] > high[2]:
            continue
        identity = tuple(coordinates)
        if identity in origins:
            continue
        origins.add(identity)
        checked += verify_depth(
            content(db, field_key), pixels, half_extent, low[2], high[2]
        )
        original = [
            replay_bounds(p, centre, rotation, -full, full, *projection)
            for p in positions
        ]
        narrowed = [
            replay_bounds(p, centre, rotation, low, high, *projection)
            for p in positions
        ]
        assert np.all(np.asarray(narrowed) <= np.asarray(original) + 1e-10)
        samples.append((bytes_count, pixels, low[2], high[2], original, narrowed))
    return samples, checked


def summarize(samples):
    bytes_count = np.array([each[0] for each in samples])
    errors = np.asarray([each[4:] for each in samples])
    return {
        "sampled_products": len(samples),
        "error_budget_px": {
            px: {
                label: [
                    {
                        "groups": int(np.count_nonzero(take)),
                        "decoded_asset_bytes": int(bytes_count[take].sum()),
                    }
                    for take in (error <= px).T
                ]
                for label, error in zip(
                    ("full_volume", "source_depth"), errors.transpose(1, 0, 2)
                )
            }
            for px in (1, 2, 4)
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--radius-m", type=float, default=14000)
    args = parser.parse_args()
    world = ET.parse(args.scenario).getroot().find("world")
    anchor, axes = local_frame(float(world.attrib["lat"]), float(world.attrib["lon"]))
    report = json.loads(args.benchmark.read_text())
    start, finish = camera_path(report, anchor, axes)
    view = report["motion"]["start"]
    projection = (*lens(report), view["near_m"], view["far_m"])
    began = perf_counter()
    with closing(
        sqlite3.connect(args.database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        samples, checked = measure(
            db, anchor, start - anchor, finish - anchor, args.radius_m, projection
        )
    print(
        json.dumps(
            summarize(samples)
            | {"checked_texels": checked, "replay_seconds": perf_counter() - began},
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
