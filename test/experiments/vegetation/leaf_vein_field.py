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


def segment_heights(points, a, b, first_radius, last_radius):
    tangent, offset = b - a, points - a
    length2 = np.sum(tangent**2, axis=-1)
    dot = np.sum(offset * tangent, axis=-1)
    distance2 = np.sum(offset**2, axis=-1)
    dr = last_radius - first_radius
    # Stationarity of r(t) * (1 - D(t)/(9*r(t)**2))**3.
    aa = dr * (9 * dr**2 - length2)
    bb = 18 * first_radius * dr**2 - 6 * length2 * first_radius - 4 * dot * dr
    cc = 9 * first_radius**2 * dr + 6 * dot * first_radius + 5 * dr * distance2
    discriminant = bb**2 - 4 * aa * cc
    q = -0.5 * (bb + np.copysign(np.sqrt(np.maximum(discriminant, 0)), bb))
    first, second = np.zeros_like(bb), np.zeros_like(bb)
    np.divide(q, aa, out=first, where=(aa != 0) & (discriminant >= 0))
    np.divide(cc, q, out=second, where=(q != 0) & (discriminant >= 0))
    np.divide(-cc, bb, out=first, where=(aa == 0) & (bb != 0))
    peak = np.zeros_like(bb)
    for t in (
        np.zeros_like(bb),
        np.ones_like(bb),
        np.clip(first, 0, 1),
        np.clip(second, 0, 1),
    ):
        radius = first_radius + t * dr
        delta = offset - t[..., None] * tangent
        value = (
            radius * np.maximum(1 - np.sum(delta**2, axis=-1) / (9 * radius**2), 0) ** 3
        )
        peak = np.maximum(peak, value)
    return peak


def vein_field(plan, points, method="indexed", field="normalized"):
    if len(plan.positions) > 2048:
        raise ValueError("bounded vein graph required")
    return segment_field(
        plan.positions[plan.parents[1:]],
        plan.positions[1:],
        plan.radii[plan.parents[1:]],
        plan.radii[1:],
        points,
        method,
        field,
    )


def segment_field(
    a, b, first_radius, last_radius, points, method="indexed", field="normalized"
):
    if field not in ("normalized", "height_m"):
        raise ValueError("normalized or height_m field required")
    evaluate = segment_values if field == "normalized" else segment_heights
    points = np.asarray(points, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite Nx2 physical material points required")
    if len(points) > 262_144 or method not in ("dense", "indexed"):
        raise ValueError("bounded dense or indexed material request required")
    a, b = np.asarray(a, float), np.asarray(b, float)
    first_radius, last_radius = np.asarray(first_radius), np.asarray(last_radius)
    if (
        a.ndim != 2
        or a.shape[1] != 2
        or a.shape != b.shape
        or first_radius.shape != (len(a),)
        or last_radius.shape != (len(a),)
        or not np.isfinite([a, b]).all()
    ):
        raise ValueError("finite segment endpoints and matching radii required")
    if np.any(np.linalg.norm(b - a, axis=1) <= 0) or len(a) > 4096:
        raise ValueError("bounded nondegenerate vein segments required")
    if (
        not np.isfinite([first_radius, last_radius]).all()
        or np.any(first_radius <= 0)
        or np.any(last_radius <= 0)
    ):
        raise ValueError("finite positive vein radii required")
    value = np.zeros(len(points))
    work = 0
    if method == "dense":
        for offset in range(0, len(points), 1024):
            block = points[offset : offset + 1024]
            sample = evaluate(block[:, None], a, b, first_radius, last_radius)
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
                value[rows], evaluate(points[rows], first, last, r0, r1)
            )
            work += len(rows)
    return value, work
