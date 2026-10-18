"""Explicitly expanded diagnostic triangles; production pose products stay instanced."""

import numpy as np


def batch_geometry(product, origins, frames):
    count = len(origins)
    vertices = len(product["vertices"])
    if not 0 < count <= 100000 or count * vertices >= 2**31:
        raise ValueError(
            "bounded diagnostic batch with signed32 vertex indices required"
        )
    positions = np.einsum("nij,vj->nvi", frames, product["vertices"]) + origins[:, None]
    normals = np.einsum("nij,vj->nvi", frames, product["normals"])
    faces = (
        product["faces"][None]
        + (np.arange(count, dtype=np.int64) * vertices)[:, None, None]
    )
    return {
        "vertices": positions.reshape(-1, 3),
        "faces": faces.reshape(-1, 3).astype(np.int32),
        "normals": normals.reshape(-1, 3),
        "uv": np.tile(product["uv"], (count, 1)),
    }
