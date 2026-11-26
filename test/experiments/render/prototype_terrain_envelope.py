"""Build a diagnostic mesh for the terrain excluded by the panorama near plane."""

import argparse
import hashlib
import math
from pathlib import Path

import numpy as np

from outshine_ref.places.camera import axes, ecef
from pack_surface_tile import pack
from terrain_cached_page import eligible, package, page_fractions, page_prefix, page_records


def height_samples(pages, x, y):
    result = np.full(x.shape, np.nan)
    for zoom in sorted({page[0] for page in pages}, reverse=True):
        pending = np.flatnonzero(np.isnan(result))
        if not len(pending):
            break
        u, v = x[pending] * (1 << zoom), y[pending] * (1 << zoom)
        tx, ty = np.floor(u).astype(int), np.floor(v).astype(int)
        for index, col, row, fx, fy in zip(pending, tx, ty, u - tx, v - ty):
            source = pages.get((zoom, col, row))
            if source is None:
                continue
            page, nodes = source
            gx, gy = np.interp([fx, fy], page_fractions(page), np.arange(33))
            ix, iy = min(31, int(gx)), min(31, int(gy))
            rx, ry = gx - ix, gy - iy
            samples = np.frombuffer(nodes, dtype="<f4").reshape(35, 35)
            a, b = samples[iy + 1, ix + 1:ix + 3]
            c, d = samples[iy + 2, ix + 1:ix + 3]
            result[index] = (a * (1 - rx) + b * rx) * (1 - ry) + (c * (1 - rx) + d * rx) * ry
    if not np.isfinite(result).all():
        raise ValueError("terrain envelope has samples without source coverage")
    return result


def envelope(store, key, radius, side=129):
    data, _ = package(store, key)
    reader, (lon, lat), count = page_prefix(data)
    pages = {(p["zoom"], p["x"], p["y"]): (p, nodes)
             for p, nodes in page_records(reader, count) if eligible(p)}
    cx = (lon + 180) / 360
    cy = (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2
    metres_per_turn = 2 * math.pi * 6378137 * math.cos(math.radians(lat))
    # Concentrate samples around the eye while extending across the cube's axial near cut.
    grid = np.sinh(np.linspace(-2.8, 2.8, side)) / math.sinh(2.8)
    dx, dy = np.meshgrid(grid * radius / metres_per_turn, grid * radius / metres_per_turn)
    x, y = (cx + dx).ravel(), (cy + dy).ravel()
    heights = height_samples(pages, x, y)
    lon_mesh, lat_mesh = x * 360 - 180, np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * y))))
    enu = (ecef(lat_mesh, lon_mesh, heights) - ecef(lat, lon, 0)) @ axes(lat, lon).T
    vertices = (enu[:, [0, 2, 1]] * [1, 1, -1]).astype(np.float32)
    corner = np.arange(side**2).reshape(side, side)[:-1, :-1]
    triangles = np.stack((corner, corner + side, corner + side + 1,
                          corner, corner + side + 1, corner + 1), axis=-1).reshape(-1, 3)
    points = vertices[triangles].astype(np.float64)
    weighted = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normals = np.zeros_like(vertices, dtype=np.float64)
    np.add.at(normals, triangles.ravel(), np.repeat(weighted, 3, axis=0))
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    lower, upper = vertices.min(axis=0), vertices.max(axis=0)
    # This direct benchmark attachment is one envelope, with no indexed tile address.
    return pack(dict(version=1, coverage=[0, 0, 0], origin_lon_lat=[lon, lat],
                     revision=hashlib.sha256(vertices.tobytes()).hexdigest(),
                     geometric_error_m=float(np.linalg.norm(upper.astype(float) - lower)),
                     bounds_eus_m=[lower.tolist(), upper.tolist()],
                     vertices_eus_m=vertices.tolist(), normals_eus=normals.tolist(),
                     indices=triangles.ravel().tolist()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--store", type=Path, required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--radius-m", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not math.isfinite(args.radius_m) or args.radius_m <= 0:
        parser.error("--radius-m must be finite and positive")
    payload = envelope(args.store, args.key, args.radius_m)
    with args.output.open("xb") as file:
        file.write(payload)


if __name__ == "__main__":
    main()
