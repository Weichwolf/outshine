"""Actual closed-solid checks shared by indoor floor and stair experiments."""

from collections import Counter

import numpy as np


def closed_mesh(mesh):
    vertices, faces, parts = (mesh[k] for k in ("vertices", "faces", "parts"))
    if not len(faces) or not np.isfinite(vertices).all():
        raise ValueError("indoor_mesh_missing_or_nonfinite")
    triangles = vertices[faces]
    crosses = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if np.any(np.linalg.norm(crosses, axis=1) == 0):
        raise ValueError("indoor_mesh_degenerate_face")
    edges, direction = Counter(), Counter()
    for triangle, part in zip(triangles, parts, strict=True):
        for first, last in zip(triangle, np.roll(triangle, -1, axis=0), strict=True):
            a, b = tuple(first), tuple(last)
            edge = (int(part), tuple(sorted((a, b))))
            edges[edge] += 1
            direction[edge] += 1 if a < b else -1
    if any(v != 2 for v in edges.values()) or any(direction.values()):
        raise ValueError("indoor_mesh_not_oriented_closed")
    local = triangles - vertices[0]
    products = (
        np.einsum("ij,ij->i", local[:, 0], np.cross(local[:, 1], local[:, 2])) / 6
    )
    if products.sum() <= 0:
        raise ValueError("indoor_mesh_nonpositive_signed_volume")
    return {
        "triangles": triangles,
        "crosses": crosses,
        "signed_volume_m3": float(products.sum()),
        "absolute_volume_products_m3": float(abs(products).sum()),
        "surface_area_m2": float(np.linalg.norm(crosses, axis=1).sum() / 2),
        "oriented_edge_pairs": len(edges),
    }
