"""Chunked Euclidean point-to-triangle distances, including edges and vertices."""

import numpy as np


def point_mesh_distances(mesh, points):
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
        raise ValueError("finite spatial point batches required")
    triangles = mesh["vertices"][mesh["faces"]]
    result = np.full(len(points), np.inf)
    for begin in range(0, len(points), 16):
        selected = points[begin : begin + 16]
        best = np.full(len(selected), np.inf)
        for start in range(0, len(triangles), 1024):
            triangle = triangles[start : start + 1024]
            first, second = (
                triangle[:, 1] - triangle[:, 0],
                triangle[:, 2] - triangle[:, 0],
            )
            normal = np.cross(first, second)
            squared = np.sum(normal**2, axis=1)
            if np.any(squared <= 0):
                raise ValueError("nondegenerate triangles required")
            relative = selected[:, None] - triangle[None, :, 0]
            u = np.sum(np.cross(relative, second) * normal, axis=2) / squared
            v = np.sum(np.cross(first, relative) * normal, axis=2) / squared
            inside = (u >= 0) & (v >= 0) & (u + v <= 1)
            distance = np.where(
                inside, np.sum(relative * normal, axis=2) ** 2 / squared, np.inf
            )
            for offset, edge in (
                (0, first),
                (first, second - first),
                (second, -second),
            ):
                local = relative - offset
                parameter = np.clip(
                    np.sum(local * edge, axis=2) / np.sum(edge**2, axis=1), 0, 1
                )
                distance = np.minimum(
                    distance,
                    np.sum((local - parameter[:, :, None] * edge) ** 2, axis=2),
                )
            best = np.minimum(best, np.min(distance, axis=1))
        result[begin : begin + len(selected)] = np.sqrt(best)
    if not np.isfinite(result).all():
        raise ValueError("finite points and a nonempty mesh required")
    return result
