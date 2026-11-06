"""Finite ribbon correspondence; an offline error/cost experiment, not runtime LOD."""

import numpy as np


def parameters(rings):
    rings = np.asarray(rings, dtype=np.float64)
    if rings.ndim != 3 or rings.shape[1:] != (4, 3) or len(rings) < 2:
        raise ValueError("at least two ordered four-point rings required")
    if not np.isfinite(rings).all():
        raise ValueError("finite source positions required")
    centre = rings.mean(axis=1)
    distance = np.linalg.norm(np.diff(centre, axis=0), axis=1)
    if np.any(distance <= 0):
        return np.arange(len(rings), dtype=np.float64)
    return np.r_[0.0, np.cumsum(distance)]


def correspondence_bound(rings, along):
    """Maximum of two PL maps on their common triangulation overlay.

    Source and candidate share a strip parameter domain. The candidate diagonal
    crosses each source row and each source diagonal. On each overlay face their
    difference is affine; its norm attains a maximum at an overlay vertex. Checking
    source vertices and both types of crossing therefore bounds both directions.
    This Float64 probe does not provide directed-rounding guarantees.
    """
    rings = np.asarray(rings, dtype=np.float64)
    along = np.asarray(along, dtype=np.float64)
    if (
        rings.ndim != 3
        or rings.shape[1:] != (4, 3)
        or len(rings) < 2
        or along.shape != (len(rings),)
        or not np.isfinite(rings).all()
        or not np.isfinite(along).all()
        or np.any(np.diff(along) <= 0)
    ):
        raise ValueError("finite source rings and strictly increasing parameters required")
    if len(rings) == 2:
        return 0.0
    t = (along - along[0]) / (along[-1] - along[0])
    if not np.isfinite(t).all() or np.any(np.diff(t) <= 0) or np.any(np.diff(t) >= 1):
        return np.inf
    chord = rings[0] + t[:, None, None] * (rings[-1] - rings[0])
    maximum = float(np.linalg.norm(rings - chord, axis=2).max())
    crossing = t[:, None, None] * rings[:, :-1] + (1 - t[:, None, None]) * rings[:, 1:]
    diagonal = (1 - t[:, None, None]) * rings[0, 1:] + t[:, None, None] * rings[-1, :-1]
    maximum = max(maximum, float(np.linalg.norm(crossing - diagonal, axis=2).max()))
    u = t[:-1] / (1 - np.diff(t))
    crossing = (1 - u[:, None, None]) * rings[:-1, 1:] + u[:, None, None] * rings[1:, :-1]
    diagonal = (1 - u[:, None, None]) * rings[0, 1:] + u[:, None, None] * rings[-1, :-1]
    return max(maximum, float(np.linalg.norm(crossing - diagonal, axis=2).max()))


def simplify(rings, error_m):
    rings = np.asarray(rings, dtype=np.float64)
    along = parameters(rings)
    if not np.isfinite(error_m) or error_m < 0:
        raise ValueError("finite nonnegative position budget required")
    pending = [(0, len(rings) - 1)]
    chosen = [0]
    bound = 0.0
    visits = 0
    while pending:
        first, last = pending.pop()
        visits += last - first + 1
        error = correspondence_bound(rings[first : last + 1], along[first : last + 1])
        if error <= error_m:
            chosen.append(last)
            bound = max(bound, error)
        else:
            middle = (first + last) // 2
            pending.extend(((middle, last), (first, middle)))
    return np.asarray(chosen, dtype=np.uint32), bound, visits


def triangles(rings):
    rings = np.asarray(rings)
    positions = rings.reshape(-1, 3)
    a = (4 * np.arange(len(rings) - 1)[:, None] + np.arange(3)).ravel()
    indices = np.stack((a, a + 4, a + 1, a + 1, a + 4, a + 5), axis=1).reshape(-1, 3)
    return positions[indices]
