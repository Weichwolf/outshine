"""Certified geometric subpatch controls, never surfaces inferred from depth points."""

import numpy as np
from current_visibility import project, raster


def selected_faces(vertices, triangles, view, eligible, limit):
    """Current projected area per original triangle; geometry-only finite seed control."""
    projected, z = project(vertices, view)
    p = projected[triangles]
    a, b = p[:, 1, :2] - p[:, 0, :2], p[:, 2, :2] - p[:, 0, :2]
    signed_area = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    area = np.abs(signed_area)
    width, height = view[3:]
    low = np.maximum(p[:, :, :2].min(axis=1), 0)
    high = np.minimum(p[:, :, :2].max(axis=1), [width, height])
    front = (
        (z[triangles] > 0.1).all(axis=1)
        & (signed_area < -1e-12)
        & (high > low).all(axis=1)
        & np.asarray(eligible, bool)
    )
    ids = np.flatnonzero(front)
    order = np.lexsort((ids, -area[ids]))
    return ids[order[:limit]]


def raster_faces(vertices, triangles, ids, view):
    projected, z = project(vertices, view)
    work = {}
    depth = raster(projected, z, triangles[ids], view[3], view[4], counters=work)
    return depth, work


def tile_patches(vertices, triangles, ids, previous_view, block=8, max_tiles=65536):
    """Old-image tiles wholly contained in an actual opaque original triangle.

    Caller supplies only static, opaque, valid source triangles. A face plane and
    its three actual edges prove support; matching sampled depth alone never does.
    A strictly interior margin leaves uncertain boundary tiles open. No source
    visibility or material eligibility is inferred by this function.
    """
    if type(block) is not int or not 1 <= block <= 64:
        raise ValueError("Support tiles require 1..64 pixels per side")
    if type(max_tiles) is not int or not 1 <= max_tiles <= 65536:
        raise ValueError("Support must have an explicit 1..65536 tile limit")
    eye, basis, focal, width, height = previous_view
    projected, z = project(vertices, previous_view)
    quads, owners = [], []
    tested = 0
    limited = False
    for face in ids:
        if len(owners) == max_tiles:
            limited = True
            break
        p = projected[triangles[face]]
        if (z[triangles[face]] <= 0.1).any():
            continue
        a, b, c = p[:, :2]
        matrix = np.column_stack((a - c, b - c))
        if np.linalg.det(matrix) >= -1e-12:
            continue
        low = np.maximum(np.ceil(p[:, :2].min(axis=0) / block), 0).astype(int)
        high = np.minimum(
            np.floor(p[:, :2].max(axis=0) / block), [width // block, height // block]
        ).astype(int)
        if (low >= high).any():
            continue
        y, x = np.mgrid[low[1] : high[1], low[0] : high[0]]
        origin = np.column_stack((x.ravel(), y.ravel())) * block
        corners = origin[:, None] + block * np.array([[0, 0], [0, 1], [1, 1], [1, 0]])
        tested += len(origin)
        coordinates = (corners - c) @ np.linalg.inv(matrix).T
        bary = np.concatenate(
            (coordinates, 1 - coordinates.sum(axis=2, keepdims=True)), axis=2
        )
        inside = (bary > 1e-9).all(axis=(1, 2))
        if not inside.any():
            continue
        corners = corners[inside]
        remaining = max_tiles - len(owners)
        limited |= len(corners) > remaining
        corners = corners[:remaining]
        local_rays = np.concatenate(
            (
                (corners - [width / 2, height / 2]) * [1, -1] / focal,
                np.ones((*corners.shape[:2], 1)),
            ),
            axis=2,
        )
        rays = local_rays @ basis
        source = vertices[triangles[face]]
        normal = np.cross(source[1] - source[0], source[2] - source[0])
        distance = ((source[0] - eye) @ normal) / (rays @ normal)
        quads.append(rays * distance[:, :, None] + eye)
        owners.extend([face] * len(rays))
    return (
        np.concatenate(quads) if quads else np.empty((0, 4, 3)),
        np.array(owners, dtype=np.int64),
        {
            "old_tiles_tested": tested,
            "certified_tiles": len(owners),
            "limit_reached": limited,
        },
    )


def raster_patches(quads, owners, view, valid_faces):
    valid = np.asarray(valid_faces, bool)
    if (
        quads.shape != (len(owners), 4, 3)
        or not np.isfinite(quads).all()
        or valid.ndim != 1
        or (len(owners) and (owners.min() < 0 or owners.max() >= len(valid)))
    ):
        raise ValueError("Support owners must reference current validity entries")
    live = quads[valid[owners]]
    points = live.reshape(-1, 3)
    triangles = (
        np.arange(len(live))[:, None, None] * 4 + [[0, 1, 2], [0, 2, 3]]
    ).reshape(-1, 3)
    return raster_faces(points, triangles, np.arange(len(triangles)), view)
