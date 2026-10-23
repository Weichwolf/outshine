"""Single opaque convex-core control for from-region source pruning (#1141).

A core is the intersection abs(normals @ (point-centre)) < half_extent.
The caller must prove that this entire core lies inside opaque source geometry.
The result controls primary visibility only, never caster or simulation demand.
"""

import itertools

import numpy as np

_CHOICES = np.array(list(itertools.product((0, 1), repeat=3)))
_EPS = np.finfo(np.float64).eps
_GAMMA = 32 * _EPS / (1 - 32 * _EPS)


def corners(bounds):
    return np.where(_CHOICES, bounds[1], bounds[0])


def covered(observer, target, centre, normals, half_extent, *, opaque=False):
    """Sufficient whole-AABB predicate; different cores cannot share this proof.

    For fixed x, the points whose segment to x crosses a convex body form a
    convex shadow cone. Apply convexity first to all target vertices, then to
    all observer vertices. All 64 segments must cross the SAME body strictly.
    Slabs only propose floating t witnesses. Their strict interiors are checked
    with a forward-error guard; t itself denotes an exact binary parameter.
    """
    for value, shape in (
        (observer, (2, 3)),
        (target, (2, 3)),
        (centre, (3,)),
        (normals, (3, 3)),
        (half_extent, (3,)),
    ):
        if (
            not isinstance(value, np.ndarray)
            or value.shape != shape
            or value.dtype != np.float64
            or not np.isfinite(value).all()
            or np.max(np.abs(value)) > 1e12
        ):
            raise ValueError("bounded finite Float64 core and AABBs required")
    if np.any(observer[0] > observer[1]) or np.any(target[0] > target[1]):
        raise ValueError("ordered AABB endpoints required")
    if np.any(half_extent <= 0) or abs(np.linalg.det(normals)) < 1e-10:
        raise ValueError("nondegenerate bounded convex core required")
    if opaque is not True:
        return False
    eyes, points = corners(observer), corners(target)
    a = np.einsum("ei,ji->ej", eyes - centre, normals)
    b = np.einsum("pi,ji->pj", points - centre, normals)
    eye_error = _GAMMA * np.einsum(
        "ei,ji->ej", np.abs(eyes) + np.abs(centre) + 1, np.abs(normals)
    )
    # Require a separating core plane: no observer may start inside the body.
    outside = np.all(a - eye_error > half_extent, axis=0) | np.all(
        a + eye_error < -half_extent, axis=0
    )
    if not outside.any():
        return False
    delta = b[None] - a[:, None]
    moving = delta != 0
    safe = np.where(moving, delta, 1)
    with np.errstate(over="ignore", divide="ignore", invalid="ignore"):
        first = (-half_extent - a[:, None]) / safe
        last = (half_extent - a[:, None]) / safe
    lower = np.maximum(
        0, np.max(np.where(moving, np.minimum(first, last), -np.inf), axis=2)
    )
    upper = np.minimum(
        1, np.min(np.where(moving, np.maximum(first, last), np.inf), axis=2)
    )
    if not np.all(lower < upper):
        return False
    t = (lower + upper) * 0.5
    witness = a[:, None] + t[:, :, None] * delta
    magnitude = np.abs(eyes)[:, None] + np.abs(points)[None] + 2 * np.abs(centre) + 1
    error = _GAMMA * np.einsum("epi,ji->epj", magnitude, np.abs(normals))
    return bool(
        np.all(
            (lower < upper)
            & (t > 0)
            & (t < 1)
            & np.all(half_extent - np.abs(witness) > error, axis=2)
        )
    )
