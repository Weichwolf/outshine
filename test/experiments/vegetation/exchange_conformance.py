"""Independently reconstruct stored full-stand transfer entries in Float64."""

import argparse
import json
from pathlib import Path

import numpy as np
from diffuse_scatter import sample_sheet, uniform_samples
from exchange_basis import finite_mesh
from exchange_io import read_exchange
from scatter_reference import Float64Nearest


def run(inputs, prepared, output):
    _, vertices, faces, identity = finite_mesh(inputs)
    basis, links = read_exchange(prepared / "product", identity)
    chosen = np.random.default_rng(1139).choice(links.size, 64, replace=False)
    rows = chosen // basis["samples"]
    nodes = rows // 2
    poles = basis["normals"][nodes] * np.where(rows % 2 == 0, 1, -1)[:, None]
    random = np.concatenate([uniform_samples(basis["seed"], int(i), 1) for i in chosen])
    optical = np.c_[np.ones((64, 3)), np.zeros((64, 3))]
    directions, _ = sample_sheet(optical, poles, poles, random)
    origins = basis["points"][nodes] + basis["bias_m"] * poles
    length = float(np.linalg.norm(np.ptp(vertices, axis=0))) + 1
    ids, t, tested = Float64Nearest(vertices, faces).query(origins, directions, length)
    expected = np.full(64, -1, np.int32)
    for i, face in enumerate(ids):
        if face < 0:
            continue
        point = origins[i] + t[i] * directions[i]
        if face < basis["leaf_nodes"]:
            node = int(face)
        else:
            grid = basis["ground_grid"]
            x = min(grid - 1, max(0, int((point[0] + 3) / (6 / grid))))
            y = min(grid - 1, max(0, int((point[1] + 3) / (6 / grid))))
            node = basis["leaf_nodes"] + y * grid + x
        triangle = vertices[faces[face]]
        normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
        expected[i] = 2 * node + int(np.dot(normal, directions[i]) > 0)
    np.testing.assert_array_equal(links.ravel()[chosen], expected)
    result = {
        "product_identity": basis["identity"],
        "independent_rays": 64,
        "hits": int(sum(ids >= 0)),
        "reference_triangle_tests": tested,
        "source_geometry_sha256": identity,
        "scope": "Stored counter-selected contacts and source-side/node mapping agree with independent Float64 closest triangles; not certification of every Float32 miss.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "prepared", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    run(a.inputs, a.prepared, a.output)
