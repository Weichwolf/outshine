"""Explicitly expanded diagnostic triangles; production pose products stay instanced."""

import numpy as np


def posed_bounds(product, origins, frames):
    low, high = product["vertices"].min(axis=0), product["vertices"].max(axis=0)
    centre = origins + np.einsum("nij,j->ni", frames, (low + high) / 2)
    extent = np.einsum("nij,j->ni", abs(frames), (high - low) / 2)
    # The diagnostic mesh is handed to Blender as Float32 positions.
    guard = (
        8
        * np.finfo(np.float32).eps
        * (np.max(abs(centre), axis=1) + np.max(extent, axis=1) + 1)
    )
    return centre - extent - guard[:, None], centre + extent + guard[:, None]


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
