"""Place canopy groups on the indexed terrain triangles for the replay experiment."""

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np

from bake_surface_ground import SAMPLE
from pack_surface_tile import HEADER, INDEX, VERTEX


def read_surface(path):
    raw = path.read_bytes()
    header = HEADER.unpack_from(raw)
    if header[0] != b"OUTSHST5":
        raise ValueError("packed surface with ground materials required")
    side = INDEX.unpack_from(raw, HEADER.size)[0]
    ground = INDEX.unpack_from(raw, HEADER.size + INDEX.size)[0]
    if side < 2 or ground < 6 * (side - 1)**2 or header[-2] < side**2:
        raise ValueError("surface does not contain a complete face grid")
    start = HEADER.size + 2 * INDEX.size
    packed = np.frombuffer(raw, dtype="<f4", offset=start, count=6 * side**2)
    points = packed.reshape(-1, 6)[:, :3].astype(float)
    cursor = start + header[-2] * VERTEX.size
    indices = np.frombuffer(raw, dtype="<u4", offset=cursor, count=6 * (side - 1)**2)
    faces = indices.reshape(side - 1, side - 1, 2, 3)
    if faces.max() >= len(points):
        raise ValueError("surface faces require a regular ground grid")
    metadata = {"coverage": list(header[1:4]), "origin_lon_lat": list(header[4:6]),
                "revision": header[6].decode("ascii")}
    return metadata, points, faces


def plant(faces, points, classes, densities, rng, limit):
    roots = []
    for face in faces.reshape(-1, 3):
        density = min(densities.get(int(cl), 0) for cl in classes[face])
        triangle = points[face]
        if not density or triangle[:, 1].min() > limit:
            continue
        left, right = (triangle[1:] - triangle[0])[:, [0, 2]]
        area = 0.5 * abs(left[0] * right[1] - left[1] * right[0])
        uv = rng.random((int(area * density + rng.random()), 2))
        flip = uv.sum(axis=1) > 1
        uv[flip] = 1 - uv[flip]
        on_face = triangle[0] + uv @ (triangle[1:] - triangle[0])
        roots.extend(on_face[on_face[:, 1] < limit])
    return np.asarray(roots)


def card_side(half_extent_m, pitch_m):
    for side in (32, 64, 128):
        if 2 * half_extent_m / side <= pitch_m:
            return side
    return 256


def group(key, surface, roots, prototype, rng, pitch_m):
    scale = rng.uniform(0.46, 0.77, len(roots))
    centres = roots + scale[:, None] * prototype["centre_m"]
    radius = scale * prototype["half_extent_m"]
    centre = ((centres - radius[:, None]).min(axis=0)
              + (centres + radius[:, None]).max(axis=0)) * 0.5
    extent = np.max(np.linalg.norm(centres - centre, axis=1) + radius)
    side = card_side(extent, pitch_m)
    return {"pixels": side, "card_side": side, "centre_m": centre.tolist(), "half_extent_m": float(extent),
            "roots_m": roots.tolist(), "tree_scales": scale.tolist(), "canopy_cell": key,
            "surface": surface}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("surface", type=Path)
    parser.add_argument("ground_samples", type=Path)
    parser.add_argument("vegetation", type=Path)
    parser.add_argument("prototype_manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cell", action="append", nargs=2, type=int, metavar=("ROW", "COL"))
    parser.add_argument("--max-root-elevation-m", type=float, default=float("inf"))
    parser.add_argument("--card-pitch-m", type=float, default=0.8,
                        help="target metres per card texel, limited to 32–256 texels per side")
    opts = parser.parse_args()
    if not (math.isfinite(opts.card_pitch_m) and opts.card_pitch_m > 0):
        parser.error("--card-pitch-m must be finite and positive")
    surface, points, faces = read_surface(opts.surface)
    classes = np.fromfile(opts.ground_samples, dtype=SAMPLE)["class"]
    if len(classes) != len(points):
        raise ValueError("ground samples must match the surface vertices")
    vegetation = json.loads(opts.vegetation.read_text())
    densities = {i: recipe["trees"]["perM2"] for i, recipe in enumerate(vegetation["templates"])
                 if recipe["name"] in {"mixed_broadleaf", "conifer_forest"}}
    prototype = json.loads(opts.prototype_manifest.read_text())
    cells = opts.cell or [(row, col) for row in range(0, faces.shape[0], 2)
                         for col in range(0, faces.shape[1], 2)]
    opts.output.mkdir(parents=True, exist_ok=True)
    address = "-".join(map(str, surface["coverage"]))
    for row, col in cells:
        if not 0 <= row < faces.shape[0] or not 0 <= col < faces.shape[1]:
            raise ValueError("cell lies outside the ground grid")
        key = hashlib.sha256(f"{address}/{row}/{col}".encode()).hexdigest()
        rng = np.random.default_rng(int(key[:16], 16))
        roots = plant(faces[row:row+2, col:col+2], points, classes, densities,
                      rng, opts.max_root_elevation_m)
        if len(roots) < 10:
            continue
        output = opts.output / f"layout-{key[:8]}.json"
        layout = group(key, surface, roots, prototype, rng, opts.card_pitch_m)
        output.write_text(json.dumps(layout, indent=2) + "\n")


if __name__ == "__main__":
    main()
