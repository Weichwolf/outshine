"""Finite cut normals without moving exact null facets in closed topology."""

import numpy as np

from .prism_union import primitive_mesh


def triangle_normals(vertices, faces, original, cutters, owners, references):
    points = vertices[faces]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    lengths = np.linalg.norm(normals, axis=1)
    null = np.all(normals == 0, axis=1)
    if not np.isfinite(lengths).all() or np.any((lengths == 0) & ~null):
        raise ValueError("Cut produced an unresolved surface normal")
    for index in np.flatnonzero(null):
        owner, reference = owners[index], references[index]
        if owner == 0:
            parent = original["vertices"][original["faces"][reference]]
        else:
            positions, triangles = primitive_mesh(cutters[owner - 1])
            parent = positions[triangles[reference]][::-1]
        normal = np.cross(parent[1] - parent[0], parent[2] - parent[0])
        length = np.linalg.norm(normal)
        if not np.isfinite(length) or length == 0:
            raise ValueError("Null cut facet has no resolved parent surface normal")
        normals[index], lengths[index] = normal / length, 1.0
    normals /= lengths[:, None]
    return normals
