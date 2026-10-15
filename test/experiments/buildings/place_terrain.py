"""Declared coarse DEM context for building views, independent of native terrain admission."""

import numpy as np
from place_tiles import geography


def mesh(reference, receipts, elevation, material):
    tiles = np.asarray([row["tile"] for row in receipts if row["kind"] == "vector"])
    if len(set(tiles[:, 0])) != 1:
        raise ValueError("Initial Place terrain requires one vector acquisition zoom")
    zoom = int(tiles[0, 0])
    lower, upper = tiles[:, 1:].min(axis=0), tiles[:, 1:].max(axis=0) + 1
    x, y = (
        np.linspace(lower[0] + 1e-8, upper[0] - 1e-8, 129),
        np.linspace(lower[1] + 1e-8, upper[1] - 1e-8, 129),
    )
    xx, yy = np.meshgrid(x, y)
    latitude, longitude = geography(
        np.column_stack([xx.ravel(), yy.ravel()]), 1, (zoom, 0, 0)
    )
    heights = elevation.sample(latitude, longitude)
    vertices = reference.local(latitude, longitude, heights)
    a = (np.arange(128)[:, None] * 129 + np.arange(128)[None, :]).ravel()
    faces = np.concatenate(
        [
            np.column_stack([a, a + 129, a + 1]),
            np.column_stack([a + 1, a + 129, a + 130]),
        ]
    ).astype(np.uint32)
    triangles = vertices[faces]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    return {
        "vertices": triangles.reshape(-1, 3),
        "faces": np.arange(triangles.size // 3, dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normal, 3, axis=0),
        "uv": np.zeros((triangles.size // 3, 2)),
        "materials": np.full(len(faces), material, dtype=np.uint16),
        "facade_extent": np.zeros((triangles.size // 3, 3)),
    }
