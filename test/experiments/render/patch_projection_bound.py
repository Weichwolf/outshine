"""Constructive directed surface/projection bounds; no visibility admission."""

import math
from fractions import Fraction

import numpy as np
import trimesh
from capture_region_visibility import _dot, _subtract, _vector

WEIGHT_SCALE = 1 << 30
ROOT_SCALE = 1 << 40
PAIR_LIMIT = 4096


def root_interval(value):
    """Outward rational square-root bracket, never a geometry tolerance."""
    low = math.isqrt(value.numerator * ROOT_SCALE**2 // value.denominator)
    exact = low * low * value.denominator == value.numerator * ROOT_SCALE**2
    return Fraction(low, ROOT_SCALE), Fraction(low + (not exact), ROOT_SCALE)


def fragments(depth):
    """Exact dyadic barycentric subdivision of one whole source triangle."""
    result = np.eye(3, dtype=np.int64)[None] * (1 << depth)
    for _ in range(depth):
        a, b, c = result.transpose(1, 0, 2)
        ab, bc, ca = (a + b) // 2, (b + c) // 2, (c + a) // 2
        result = np.concatenate(
            [
                np.stack(parts, 1)
                for parts in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca))
            ]
        )
    return result


def mix(vertices, weights, denominator):
    return tuple(
        sum(
            Fraction(int(weight), denominator) * point[axis]
            for point, weight in zip(vertices, weights, strict=True)
        )
        for axis in range(3)
    )


def directed_correspondence(source, target, *, depth=0):
    """Each source fragment maps affinely into one actual target triangle."""
    for values in (source, target):
        if (
            not isinstance(values, np.ndarray)
            or values.dtype != np.float32
            or values.ndim != 3
            or values.shape[1:] != (3, 3)
            or not 0 < len(values) <= 512
            or not np.isfinite(values).all()
        ):
            raise ValueError("bounded finite native Float32 triangles required")
    if type(depth) is not int or depth not in (0, 1, 2):
        raise ValueError("subdivision depth must be 0, 1 or 2")
    count = len(source) * 4**depth
    pairs = count * len(target)
    if pairs > PAIR_LIMIT:
        return {"squared_m": None, "reason": "pair_budget", "pairs": 0}
    target64 = target.astype(float)
    e1, e2 = target64[:, 1] - target64[:, 0], target64[:, 2] - target64[:, 0]
    cross = np.cross(e1, e2)
    condition = np.sum(cross * cross, axis=1)
    scale = np.sum(e1 * e1, axis=1) * np.sum(e2 * e2, axis=1)
    if (condition <= 64 * np.finfo(float).eps * scale).any():
        return {"squared_m": None, "reason": "unreliable_proposal_triangle", "pairs": 0}
    bary = fragments(depth)
    proposal = np.einsum(
        "ijk,akl->aijl", bary / (1 << depth), source.astype(float)
    ).reshape(-1, 3, 3)
    original_ids = np.repeat(np.arange(len(source)), len(bary))
    grids = np.tile(bary, (len(source), 1, 1))
    targets = np.broadcast_to(
        target64[None, :, None], (count, len(target), 3, 3, 3)
    ).reshape(-1, 3, 3)
    points = np.broadcast_to(proposal[:, None], (count, len(target), 3, 3)).reshape(
        -1, 3
    )
    closest = trimesh.triangles.closest_point(targets, points)
    delta = (
        np.linalg.norm(closest - points, axis=1).reshape(count, len(target), 3).max(2)
    )
    chosen = delta.argmin(1)
    selection = (
        np.arange(count)[:, None] * len(target) + chosen[:, None]
    ) * 3 + np.arange(3)
    weights = trimesh.triangles.points_to_barycentric(
        targets[selection.ravel()], closest[selection.ravel()], method="cross"
    )
    if not np.isfinite(weights).all():
        return {"squared_m": None, "reason": "nonfinite_proposal", "pairs": pairs}
    weights = np.maximum(weights, 0)
    if (weights.sum(1) == 0).any():
        return {"squared_m": None, "reason": "empty_proposal", "pairs": pairs}
    weights /= weights.sum(1)[:, None]
    integer = np.floor(weights * WEIGHT_SCALE).astype(np.int64)
    integer[np.arange(len(integer)), weights.argmax(1)] += WEIGHT_SCALE - integer.sum(1)
    if (integer < 0).any() or (integer.sum(1) != WEIGHT_SCALE).any():
        raise ValueError("infeasible quantized barycentric proposal")
    integer = integer.reshape(count, 3, 3)
    source_exact = [tuple(map(_vector, t)) for t in source]
    target_exact = [tuple(map(_vector, t)) for t in target]
    squared = Fraction(0)
    for row in range(count):
        for vertex in range(3):
            p = mix(source_exact[original_ids[row]], grids[row, vertex], 1 << depth)
            q = mix(target_exact[chosen[row]], integer[row, vertex], WEIGHT_SCALE)
            difference = _subtract(p, q)
            squared = max(squared, _dot(difference, difference))
    return {
        "squared_m": squared,
        "pairs": pairs,
        "fragments": count,
        "source_triangle": original_ids,
        "source_weights": grids,
        "source_scale": 1 << depth,
        "target_triangle": chosen,
        "target_weights": integer,
        "proposal_operand_extent_bytes": int(
            targets.nbytes
            + points.nbytes
            + closest.nbytes
            + delta.nbytes
            + weights.nbytes
            + integer.nbytes
        ),
        "exact_vertex_checks": 3 * count,
    }


def projection_bound(
    squared_m, source, target, eye, radius_m, focal_px, width, height, error_px
):
    """All rotations placing a source point on screen, and every eye in the ball.

    With displacement d, distance D and S=1+screen_radius²/focal²,
    |projection(q)-projection(p)| <= focal*d*S/(D-d*sqrt(S)).
    Directed source/candidate correspondence is required; this says nothing
    about discarded faces, depth ordering, clipping or material response.
    """
    if (
        np.shape(eye) != (3,)
        or not np.isfinite(eye).all()
        or not all(math.isfinite(v) for v in (radius_m, focal_px, error_px))
        or radius_m < 0
        or focal_px <= 0
        or error_px <= 0
        or any(type(n) is not int or n <= 0 for n in (width, height))
    ):
        raise ValueError("finite eye, radius and positive explicit projection required")
    if squared_m is None:
        return {
            "upper_px": None,
            "within": False,
            "reason": "unresolved_correspondence",
        }
    all_points = np.concatenate((source.reshape(-1, 3), target.reshape(-1, 3)))
    low, high, origin = (
        _vector(all_points.min(0)),
        _vector(all_points.max(0)),
        _vector(eye),
    )
    nearest = tuple(
        max(lo - at, at - hi, Fraction(0))
        for lo, hi, at in zip(low, high, origin, strict=True)
    )
    distance = root_interval(_dot(nearest, nearest))[0]
    displacement = root_interval(squared_m)[1]
    focal, allowed = Fraction(float(focal_px)), Fraction(float(error_px))
    screen = 1 + (Fraction(width, 2) ** 2 + Fraction(height, 2) ** 2) / focal**2
    denominator = (
        distance - Fraction(float(radius_m)) - displacement * root_interval(screen)[1]
    )
    if denominator <= 0:
        return {"upper_px": None, "within": False, "reason": "projection_domain"}
    upper = focal * displacement * screen / denominator
    radius_ceiling = (
        distance
        - displacement * root_interval(screen)[1]
        - focal * displacement * screen / allowed
    )
    return {
        "upper_px": upper,
        "within": upper <= allowed,
        "radius_ceiling_exclusive_m": max(Fraction(0), radius_ceiling),
        "distance_lower_m": distance,
        "displacement_upper_m": displacement,
    }
