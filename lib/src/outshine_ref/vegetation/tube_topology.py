"""Shared closed ring topology and hard cap corner normals for swept branch surfaces."""

import numpy as np


def close_rings(surface, normals, centre, tangents):
    stations, sides, _ = surface.shape
    vertices = np.vstack((surface.reshape(-1, 3), centre[0], centre[-1]))
    normals = np.vstack((normals.reshape(-1, 3), -tangents[0], tangents[-1]))
    ring = np.arange(stations * sides).reshape(-1, sides)
    a, b = ring[:-1], np.roll(ring[:-1], -1, axis=1)
    d, c = ring[1:], np.roll(ring[1:], -1, axis=1)
    faces = np.stack(
        (np.stack((a, d, c), axis=-1), np.stack((a, c, b), axis=-1)), axis=2
    ).reshape(-1, 3)
    base = np.column_stack(
        (np.full(sides, len(vertices) - 2), ring[0], np.roll(ring[0], -1))
    )
    tip = np.column_stack(
        (np.full(sides, len(vertices) - 1), np.roll(ring[-1], -1), ring[-1])
    )
    faces = np.vstack((faces, base, tip)).astype(np.int32)
    corners = normals[faces].copy()
    corners[-2 * sides : -sides] = -tangents[0]
    corners[-sides:] = tangents[-1]
    points = vertices[faces]
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if (
        not np.isfinite(vertices).all()
        or not np.isfinite(corners).all()
        or not np.all(np.sum(cross[:, None] * corners, axis=2) > 0)
    ):
        raise ValueError("tube subdivision does not resolve the oriented surface")
    return {"vertices": vertices, "faces": faces, "normals": corners.reshape(-1, 3)}
