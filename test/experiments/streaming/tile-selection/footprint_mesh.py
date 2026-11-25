"""Export terrain-seated footprint groups and a recorded camera for the native card probe."""

import argparse
import gzip
import json
import math
import struct
from pathlib import Path
from time import perf_counter

import numpy as np
from footprint_cards import catalogue
from footprint_flight import Terrain, render_poses, seat
from group_footprints import HEIGHT, read_source
from mapbox_earcut import triangulate_float64
from shapely.geometry.polygon import orient


def mesh(parts, roofs):
    vertices, normals, indices = [], [], []
    for part in parts:
        shape = orient(part.footprint)
        if roofs:
            rings = [
                np.asarray(ring.coords)[:-1]
                for ring in (shape.exterior, *shape.interiors)
            ]
            points = np.concatenate(rings)
            ends = np.cumsum([len(ring) for ring in rings], dtype=np.uint32)
            faces = triangulate_float64(points, ends).reshape(-1, 3)
            a, b, c = points[faces].transpose(1, 0, 2)
            first, second = b - a, c - a
            signed = first[:, 0] * second[:, 1] - first[:, 1] * second[:, 0]
            if not math.isclose(np.abs(signed).sum() * 0.5, shape.area, abs_tol=1e-5):
                raise ValueError(
                    "triangulated roof does not cover its source footprint"
                )
            faces[signed > 0] = faces[signed > 0][:, ::-1]
            indices.extend((faces + len(vertices)).ravel())
            vertices.extend((x, part.top_m, z) for x, z in points)
            normals.extend([(0, 1, 0)] * len(points))
            continue
        for ring in (shape.exterior, *shape.interiors):
            for (x, z), (a, b) in zip(ring.coords, list(ring.coords)[1:]):
                dx, dz = a - x, b - z
                length = math.hypot(dx, dz)
                if length == 0:
                    continue
                first = len(vertices)
                vertices.extend(
                    [
                        (x, part.bottom_m, z),
                        (x, part.top_m, z),
                        (a, part.bottom_m, b),
                        (a, part.top_m, b),
                    ]
                )
                normals.extend([(dz / length, 0, -dx / length)] * 4)
                indices.extend(first + i for i in (0, 1, 2, 2, 1, 3))
    return tuple(
        np.asarray(data, dtype=kind).ravel()
        for data, kind in zip((vertices, normals, indices), ("<f4", "<f4", "<u4"))
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "dem", "scene", "benchmark", "export"):
        parser.add_argument(field, type=Path)
    parser.add_argument("--extent-m", type=float, default=1200)
    args = parser.parse_args()
    began = perf_counter()
    with gzip.open(args.source, "rt") as source:
        terrain = Terrain(args.dem, json.load(source))
    parts = [
        part
        for part in read_source(args.source)
        if max(abs(part.footprint.centroid.x), abs(part.footprint.centroid.y))
        <= args.extent_m
    ]
    if not parts:
        parser.error("the selected window has no source footprints")
    start, end = render_poses(
        json.loads(args.benchmark.read_text()), args.scene, terrain, 2
    )
    groups = catalogue(seat(parts, terrain), 256)
    with args.export.open("wb") as output:
        output.write(
            struct.pack(
                "<13dI",
                *start.eye,
                *end.eye,
                *start.axes[:, 2],
                *-start.axes[:, 1],
                2 * math.atan(HEIGHT / (2 * start.focal)),
                len(groups),
            )
        )
        triangles = 0
        for group in groups:
            for mode in (False, True):
                arrays = mesh(group, mode)
                for array in arrays:
                    output.write(struct.pack("<I", array.size))
                    output.write(array.tobytes())
                triangles += arrays[-1].size // 3
    print(
        json.dumps(
            {
                "parts": len(parts),
                "groups": len(groups),
                "triangles": triangles,
                "export_bytes": args.export.stat().st_size,
                "seconds": perf_counter() - began,
            }
        )
    )


if __name__ == "__main__":
    main()
