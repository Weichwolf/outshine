"""Export a cached native terrain page for the independent Metal tile benchmark."""

import argparse
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np

from outshine_ref.places.camera import axes, ecef
from terrain_cached_page import (
    PrefixReader, checked_heights, eligible, package, page_fractions, page_records, select_window,
)


def source_page(data, kind, tile):
    zoom, x, y = tile
    if kind == "ground-region":
        selection, _ = select_window(data, zoom, x, x, y, y)
        return selection[0]
    reader = PrefixReader(data)
    version, count = reader.read("II")
    if version != 1 or not 0 < count <= 64:
        raise ValueError("unsupported native terrain pack")
    selected = [
        (page, checked_heights(heights))
        for page, heights in page_records(reader, count)
        if eligible(page) and (page["zoom"], page["x"], page["y"]) == tile
    ]
    if len(selected) != 1 or reader.at != len(data):
        raise ValueError("exactly one requested native terrain page required")
    return selected[0]


def surface_tile(cache, key, kind, zoom, x, y):
    data, provenance = package(cache, key, kind=kind)
    page, heights = source_page(data, kind, (zoom, x, y))
    side = page["side"]
    scale = 2**zoom
    fractions = page_fractions(page)
    longitudes = (x + fractions) / scale * 360 - 180
    latitudes = np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * (y + fractions) / scale))))
    height = np.frombuffer(heights, dtype="<f4").reshape(side + 2, side + 2)[1:-1, 1:-1]
    origin_lon = float(np.mean(longitudes[[0, -1]]))
    origin_lat = float(np.mean(latitudes[[0, -1]]))
    enu = (ecef(latitudes[:, None], longitudes, height) - ecef(origin_lat, origin_lon, 0)) @ axes(
        origin_lat, origin_lon
    ).T
    eus = enu[..., [0, 2, 1]] * [1, 1, -1]
    packed = eus.astype(np.float32)
    vertices = packed.reshape(-1, 3)
    corner = np.arange(side**2).reshape(side, side)[:-1, :-1]
    indices = np.stack((corner, corner + side, corner + side + 1,
                        corner, corner + side + 1, corner + 1), axis=-1)
    triangles = indices.reshape(-1, 3)
    points = vertices[triangles].astype(np.float64)
    weighted = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normals = np.zeros_like(vertices, dtype=np.float64)
    np.add.at(normals, triangles.ravel(), np.repeat(weighted, 3, axis=0))
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    # Any surface triangulated from the same four source corners per cell stays
    # within that cell's convex hull. The diameter bounds either diagonal choice.
    corners = (eus[:-1, :-1], eus[1:, :-1], eus[1:, 1:], eus[:-1, 1:])
    cell_diameter = max(np.linalg.norm(a - b, axis=-1).max()
                        for a, b in itertools.combinations(corners, 2))
    rounding = np.linalg.norm(packed - eus, axis=-1).max()
    revision = hashlib.sha256(f"1:{key}:{zoom}:{x}:{y}".encode() + heights).hexdigest()
    return {
        "version": 1,
        "coverage": [zoom, x, y],
        "origin_lon_lat": [origin_lon, origin_lat],
        "revision": revision,
        "bounds_eus_m": [vertices.min(axis=0).tolist(), vertices.max(axis=0).tolist()],
        "geometric_error_m": float(np.nextafter(cell_diameter + rounding, np.inf)),
        "vertices_eus_m": vertices.tolist(),
        "normals_eus": normals.astype(np.float32).tolist(),
        "indices": triangles.ravel().tolist(),
        "source": provenance,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--kind", choices=("ground-region", "ground-terrain-pack"),
                        default="ground-region")
    parser.add_argument("--tile", type=int, nargs=3, metavar=("Z", "X", "Y"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    product = surface_tile(args.cache, args.key, args.kind, *args.tile)
    with args.output.open("x") as destination:
        json.dump(product, destination, separators=(",", ":"), allow_nan=False)
        destination.write("\n")


if __name__ == "__main__":
    main()
