"""Exact displacement between nested leaf meshes on their common parameter domain."""

import numpy as np


def parameter_grid(blade, segments, across):
    t = np.linspace(0, 1, segments + 1)
    if blade.petiole > 0 and blade.base_fill > 0:
        t = np.insert(t, 0, -blade.petiole / blade.length)
    s = np.linspace(-1, 1, 2 * across + 1)
    u, v = np.meshgrid(t, s, indexing="ij")
    points = blade.evaluate(u, v, np.where(v < 0, -1, 1))[0]
    return t, s, points


def interpolate(t, s, points, parameters):
    row = np.clip(np.searchsorted(t, parameters[:, 0], side="right") - 1, 0, len(t) - 2)
    column = np.clip(
        np.searchsorted(s, parameters[:, 1], side="right") - 1, 0, len(s) - 2
    )
    u = (parameters[:, 0] - t[row]) / (t[row + 1] - t[row])
    v = (parameters[:, 1] - s[column]) / (s[column + 1] - s[column])
    a, b = points[row, column], points[row, column + 1]
    c, d = points[row + 1, column + 1], points[row + 1, column]
    upper = (1 - v)[:, None] * a + (v - u)[:, None] * b + u[:, None] * c
    lower = (1 - u)[:, None] * a + v[:, None] * c + (u - v)[:, None] * d
    return np.where((v >= u)[:, None], upper, lower)


def edges(t, s):
    u, v = np.meshgrid(t, s, indexing="ij")
    nodes = np.stack((u, v), axis=-1)
    return np.concatenate(
        (
            np.stack((nodes[:-1], nodes[1:]), axis=-2).reshape(-1, 2, 2),
            np.stack((nodes[:, :-1], nodes[:, 1:]), axis=-2).reshape(-1, 2, 2),
            np.stack((nodes[:-1, :-1], nodes[1:, 1:]), axis=-2).reshape(-1, 2, 2),
        )
    )


def cross2(left, right):
    return left[..., 0] * right[..., 1] - left[..., 1] * right[..., 0]


def displacement(blade, segments=16, across=1):
    if (
        not isinstance(segments, int)
        or not isinstance(across, int)
        or segments <= 0
        or across <= 0
        or 64 % segments
        or 2 % across
    ):
        raise ValueError("LOD grid must divide the 64-by-4 reference grid")
    fine_t, fine_s, fine_points = parameter_grid(blade, 64, 2)
    coarse_t, coarse_s, coarse_points = parameter_grid(blade, segments, across)
    source_edges, target_edges = edges(fine_t, fine_s), edges(coarse_t, coarse_s)
    vertices = [source_edges.reshape(-1, 2), target_edges.reshape(-1, 2)]
    for start, end in target_edges:
        direction = end - start
        spans, offsets = (
            source_edges[:, 1] - source_edges[:, 0],
            source_edges[:, 0] - start,
        )
        denominator = cross2(direction, spans)
        regular = np.abs(denominator) > 1e-15
        u = np.divide(
            cross2(offsets, spans), denominator, out=np.zeros(len(spans)), where=regular
        )
        v = np.divide(
            cross2(offsets, direction),
            denominator,
            out=np.zeros(len(spans)),
            where=regular,
        )
        valid = regular & (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
        vertices.append(start + u[valid, None] * direction)
    parameters = np.concatenate(vertices)
    difference = interpolate(fine_t, fine_s, fine_points, parameters) - interpolate(
        coarse_t, coarse_s, coarse_points, parameters
    )
    maximum = np.linalg.norm(difference, axis=-1).max()
    return float(maximum + 1e-12)


def merge_instances(mesh, origins, frames, ids):
    count = len(mesh["vertices"])
    vertices = np.einsum("nij,vj->nvi", frames, mesh["vertices"]) + origins[:, None, :]
    faces = mesh["faces"][None] + np.arange(len(origins))[:, None, None] * count
    normals = np.einsum("nij,vj->nvi", frames, mesh["normals"])
    return {
        "vertices": vertices.reshape(-1, 3),
        "faces": faces.reshape(-1, 3).astype(np.int32),
        "normals": normals.reshape(-1, 3),
        "uv": np.tile(mesh["uv"], (len(origins), 1)),
        "organ_ids": np.repeat(ids, count),
    }
