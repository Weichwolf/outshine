"""Cell-local scalar reconstruction; clipped I<=0 solids, not unit-distance data."""

import itertools

import numpy as np

CORNERS = np.array(list(itertools.product([0, 1], repeat=3)), float)
ABSCISSA = np.linspace(0, 1, 4)
POWER_INVERSE = np.linalg.inv(np.vander(ABSCISSA, 4, increasing=True))


def evaluate(values, low, high, points):
    q = (points - low) / (high - low)
    weights = np.where(CORNERS[None] > 0, q[:, None], 1 - q[:, None])
    value = np.prod(weights, axis=2) @ values
    gradient = np.empty_like(points, dtype=float)
    for axis in range(3):
        others = [i for i in range(3) if i != axis]
        gradient[:, axis] = (
            np.prod(weights[:, :, others], axis=2)
            @ (values * (2 * CORNERS[:, axis] - 1))
        ) / (high[axis] - low[axis])
    return value, gradient


def lipschitz_bound(values, extent):
    grid = np.asarray(values).reshape(2, 2, 2)
    rates = [np.max(abs(np.diff(grid, axis=i))) / extent[i] for i in range(3)]
    return float(np.linalg.norm(rates))


def intervals(low, high, origins, directions, stop, minimum):
    parallel = directions == 0
    times = np.divide(
        np.stack((low, high))[None] - origins[:, None],
        directions[:, None],
        out=np.zeros((len(origins), 2, 3)),
        where=~parallel[:, None],
    )
    entry = np.where(parallel, -np.inf, times.min(1))
    leave = np.where(parallel, np.inf, times.max(1)).min(1)
    outside = np.any(parallel & ((origins < low) | (origins > high)), axis=1)
    near = np.maximum(minimum, entry.max(1))
    far = np.minimum(stop, leave)
    hit = ~outside & (near <= far)
    return hit, near, far, entry.argmax(1), entry.max(1)


def first_surface(values, low, high, origins, directions, near, far, axes, box_near):
    samples = near[:, None] + ABSCISSA * (far - near)[:, None]
    points = origins[:, None] + samples[:, :, None] * directions[:, None]
    scalar = evaluate(values, low, high, points.reshape(-1, 3))[0].reshape(-1, 4)
    coefficients = scalar @ POWER_INVERSE.T
    depth = np.full(len(origins), np.inf)
    cap = scalar[:, 0] <= 0
    interior = cap & (box_near < near)
    depth[cap & ~interior] = near[cap & ~interior]
    roots_tested = 0
    for ray in np.flatnonzero(~cap):
        coef = coefficients[ray]
        tolerance = 64 * np.finfo(float).eps * max(np.max(abs(coef)), 1)
        coef = np.polynomial.polynomial.polytrim(coef, tol=tolerance)
        roots = np.polynomial.polynomial.polyroots(coef)
        roots_tested += len(roots)
        valid = roots.real[
            (abs(roots.imag) <= 1e-8)
            & (roots.real >= -1e-10)
            & (roots.real <= 1 + 1e-10)
        ]
        if len(valid):
            depth[ray] = near[ray] + np.clip(valid.min(), 0, 1) * (far[ray] - near[ray])
    normal = np.zeros_like(origins)
    hit = np.isfinite(depth)
    if hit.any():
        points = origins[hit] + depth[hit, None] * directions[hit]
        gradient = evaluate(values, low, high, points)[1]
        magnitude = np.linalg.norm(gradient, axis=1)
        normal[hit] = np.divide(
            gradient,
            magnitude[:, None],
            out=np.zeros_like(gradient),
            where=magnitude[:, None] > 0,
        )
    capped = np.flatnonzero(cap & ~interior)
    normal[capped] = 0
    normal[capped, axes[capped]] = -np.sign(directions[capped, axes[capped]])
    return (
        depth,
        normal,
        interior,
        {
            "polynomial_ray_queries": int((~cap).sum()),
            "polynomial_roots": roots_tested,
            "box_cap_hits": len(capped),
        },
    )
