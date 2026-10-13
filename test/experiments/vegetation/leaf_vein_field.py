"""Subdivision-invariant tapered vein envelopes in physical leaf coordinates."""

import numpy as np
from scipy.spatial import cKDTree


def segment_values(points, a, b, first_radius, last_radius):
    tangent = b - a
    length2 = np.sum(tangent**2, axis=-1)
    offset = points - a
    dot = np.sum(offset * tangent, axis=-1)
    distance2 = np.sum(offset**2, axis=-1)
    dr = last_radius - first_radius
    denominator = length2 * first_radius + dot * dr
    # Minimize |point - (a + t v)|² / (r0 + t dr)² over the whole segment.
    u = np.zeros_like(denominator)
    np.divide(
        dot * first_radius + distance2 * dr,
        denominator,
        out=u,
        where=denominator > 0,
    )
    u = np.clip(u, 0, 1)
    radius = first_radius + u * dr
    delta = offset - u[..., None] * tangent
    normalized = np.minimum(
        distance2 / first_radius**2,
        np.sum((points - b) ** 2, axis=-1) / last_radius**2,
    )
    normalized = np.minimum(normalized, np.sum(delta**2, axis=-1) / radius**2)
    q = np.maximum(1 - normalized / 9, 0)
    return q**3


def vein_field(plan, points, method="indexed"):
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite Nx2 physical material points required")
    if len(points) > 262_144 or method not in ("dense", "indexed"):
        raise ValueError("bounded dense or indexed material request required")
    a = plan.positions[plan.parents[1:]]
    b = plan.positions[1:]
    first_radius = plan.radii[plan.parents[1:]]
    last_radius = plan.radii[1:]
    if np.any(np.linalg.norm(b - a, axis=1) <= 0) or len(a) > 2047:
        raise ValueError("bounded nondegenerate vein segments required")
    if not np.isfinite(plan.radii).all() or np.any(plan.radii <= 0):
        raise ValueError("finite positive vein radii required")
    value = np.zeros(len(points))
    work = 0
    if method == "dense":
        for offset in range(0, len(points), 1024):
            block = points[offset : offset + 1024]
            sample = segment_values(block[:, None], a, b, first_radius, last_radius)
            value[offset : offset + len(block)] = sample.max(axis=1, initial=0)
            work += len(block) * len(a)
    else:
        tree = cKDTree(points)
        for first, last, r0, r1 in zip(a, b, first_radius, last_radius):
            centre = (first + last) / 2
            bound = np.linalg.norm(last - first) / 2 + 3 * max(r0, r1)
            rows = tree.query_ball_point(centre, np.nextafter(bound, np.inf))
            if not rows:
                continue
            value[rows] = np.maximum(
                value[rows], segment_values(points[rows], first, last, r0, r1)
            )
            work += len(rows)
    return value, work
