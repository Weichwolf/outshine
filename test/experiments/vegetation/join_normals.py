"""Unify only smooth local join corners while retaining source geometry and hard edges."""

import numpy as np


def average_directions(first, second):
    combined = np.asarray(first, dtype=float) + np.asarray(second, dtype=float)
    if combined.shape[-1] != 3 or not np.isfinite(combined).all():
        raise ValueError("finite direction triples required")
    length = np.linalg.norm(combined, axis=-1, keepdims=True)
    return combined / np.where(length > 0, length, 1)


def corner_weights(vertices, faces, method):
    points = vertices[faces]
    first = np.roll(points, -1, axis=1) - points
    second = np.roll(points, 1, axis=1) - points
    cross = np.linalg.norm(np.cross(first, second), axis=-1)
    if method == "angle":
        return np.arctan2(cross, np.sum(first * second, axis=-1))
    if method == "area":
        return cross
    if method == "uniform":
        return np.ones(faces.shape)
    raise ValueError("declared corner weighting required")


def smooth_join_normals(mesh, axes, plans, method="angle"):
    vertices, faces = mesh["vertices"], mesh["faces"]
    if len(faces) > 1_000_000 or len(axes) > 128:
        raise ValueError("bounded joined wood required")
    lookup = {a.path: a for a in axes}
    local = np.zeros(len(vertices), dtype=bool)
    for plan in plans:
        child = lookup[tuple(plan["path"])]
        local |= (
            np.linalg.norm(vertices - child.pose.anchor, axis=1) < 2 * plan["span_m"]
        )
    return smooth_join_corners(mesh, local, method)


def smooth_join_corners(mesh, local, method="angle"):
    vertices, faces = mesh["vertices"], mesh["faces"]
    normals = mesh["normals"].reshape(-1, 3, 3)
    local = np.asarray(local)
    if len(faces) > 1_000_000 or local.shape != (len(vertices),) or local.dtype != bool:
        raise ValueError("bounded mesh and one boolean join marker per vertex required")
    weights = corner_weights(vertices, faces, method)
    accumulated = np.zeros_like(vertices)
    np.add.at(accumulated, faces.ravel(), (normals * weights[..., None]).reshape(-1, 3))
    length = np.linalg.norm(accumulated, axis=1)
    average = accumulated / np.where(length > 0, length, 1)[:, None]
    dot = np.sum(normals * average[faces], axis=-1)
    rejected = np.zeros(len(vertices), dtype=bool)
    np.logical_or.at(rejected, faces.ravel(), (dot < np.cos(np.pi / 4)).ravel())
    triangles = vertices[faces]
    geometric = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    facing = np.sum(average[faces] * geometric[:, None], axis=-1)
    np.logical_or.at(rejected, faces.ravel(), (facing <= 0).ravel())
    selected = local & ~rejected & (length > 0)
    result = normals.copy()
    result[selected[faces]] = average[faces][selected[faces]]
    changed = np.linalg.norm(result - normals, axis=-1) > 1e-12
    return mesh | {"normals": result.reshape(-1, 3)}, {
        "local_vertices": int(local.sum()),
        "unified_vertices": int(selected.sum()),
        "rejected_local_vertices": int((local & rejected).sum()),
        "changed_corners": int(changed.sum()),
        "method": method,
    }
