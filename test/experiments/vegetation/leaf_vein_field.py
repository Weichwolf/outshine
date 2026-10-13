"""Physical compact vein ridges; indexed and dense evaluation use identical segments."""

import numpy as np
from scipy.spatial import cKDTree


def segment_values(points, a, b, radius):
    tangent = b - a
    length2 = np.sum(tangent**2, axis=-1)
    u = np.clip(np.sum((points - a) * tangent, axis=-1) / length2, 0, 1)
    delta = points - a - u[..., None] * tangent
    q = np.maximum(1 - np.sum(delta**2, axis=-1) / (3 * radius) ** 2, 0)
    return q**3


def vein_field(plan, points, method="indexed"):
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite Nx2 physical material points required")
    if len(points) > 262_144 or method not in ("dense", "indexed"):
        raise ValueError("bounded dense or indexed material request required")
    a = plan.positions[plan.parents[1:]]
    b = plan.positions[1:]
    radius = plan.radii[1:]
    if np.any(np.linalg.norm(b - a, axis=1) <= 0) or len(a) > 2047:
        raise ValueError("bounded nondegenerate vein segments required")
    value = np.zeros(len(points))
    work = 0
    if method == "dense":
        for offset in range(0, len(points), 1024):
            block = points[offset : offset + 1024]
            sample = segment_values(block[:, None], a, b, radius)
            value[offset : offset + len(block)] = sample.sum(axis=1)
            work += len(block) * len(a)
    else:
        tree = cKDTree(points)
        for first, last, r in zip(a, b, radius):
            centre = (first + last) / 2
            bound = np.linalg.norm(last - first) / 2 + 3 * r
            rows = tree.query_ball_point(centre, np.nextafter(bound, np.inf))
            if not rows:
                continue
            value[rows] += segment_values(points[rows], first, last, r)
            work += len(rows)
    return -np.expm1(-value), work
