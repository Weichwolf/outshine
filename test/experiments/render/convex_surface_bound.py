"""Continuous directed surface bounds from dyadic convex triangle witnesses.

Nearest-point queries only propose witnesses. Convex weights and outward
arithmetic prove each accepted patch; exact bisections cover original triangles.
An incomplete proof never publishes a distance bound.
"""

from dataclasses import dataclass

import numpy as np
import trimesh
from facing_intervals import add, length, mul, point, sub
from scipy.spatial import cKDTree

DENOMINATOR = 1 << 30


def affine_points(triangles, weights):
    """Exact represented weights applied to represented triangle coordinates."""
    result = point(np.zeros((*weights.shape[:-1], 3)))
    for k in range(3):
        result = add(
            result, mul(point(weights[..., k, None]), point(triangles[:, None, k, :]))
        )
    return result


def triangle_keys(triangles):
    order = np.lexsort(
        (triangles[:, :, 2], triangles[:, :, 1], triangles[:, :, 0]), axis=1
    )
    ordered = np.take_along_axis(triangles, order[:, :, None], axis=1)
    return (
        np.ascontiguousarray(ordered.reshape(-1, 9))
        .view(np.dtype((np.void, 72)))
        .ravel()
    )


def bisect(weights, edge):
    a, b = ((0, 1), (1, 2), (2, 0))[edge]
    midpoint = (weights[a] + weights[b]) * 0.5
    first, second = weights.copy(), weights.copy()
    first[a], second[b] = midpoint, midpoint
    return first, second


@dataclass
class SurfaceBound:
    complete: bool
    upper_m: float | None
    exact_targets: np.ndarray
    splits: list
    leaves: list
    work: dict
    status: str


def _witnesses(source, weights, target, candidates):
    source_interval = affine_points(source, weights)
    p = (source_interval[0] + source_interval[1]) * 0.5
    b, k = candidates.shape
    target_tri = target[candidates]
    repeated = np.repeat(target_tri[:, :, None], 3, axis=2).reshape(-1, 3, 3)
    source_points = np.broadcast_to(p[:, None], (b, k, 3, 3)).reshape(-1, 3)
    with np.errstate(invalid="ignore", divide="ignore", over="ignore"):
        closest = trimesh.triangles.closest_point(repeated, source_points)
        bary = trimesh.triangles.points_to_barycentric(repeated, closest)
    bary = np.clip(np.nan_to_num(bary, nan=0.0, posinf=0.0, neginf=0.0), 0, 1)
    bary /= np.maximum(bary.sum(axis=1, keepdims=True), 1)
    a = np.clip(np.floor(bary[:, 0] * DENOMINATOR).astype(np.int64), 0, DENOMINATOR)
    c = np.clip(np.floor(bary[:, 1] * DENOMINATOR).astype(np.int64), 0, DENOMINATOR - a)
    integer = np.column_stack((a, c, DENOMINATOR - a - c))
    witness = affine_points(repeated, (integer / DENOMINATOR)[:, None, :])
    original = tuple(
        np.broadcast_to(x[:, None], (b, k, 3, 3)).reshape(-1, 3)
        for x in source_interval
    )
    distances = length(sub(original, tuple(x[:, 0] for x in witness)))[1].reshape(
        b, k, 3
    )
    bounds = distances.max(axis=2)
    choice = bounds.argmin(axis=1)
    return (
        bounds[np.arange(b), choice],
        candidates[np.arange(b), choice],
        integer.reshape(b, k, 3, 3)[np.arange(b), choice],
    )


def directed_bound(
    source, target, *, tolerance_m, max_tests=1000000, max_depth=18, neighbours=4
):
    source, target = np.asarray(source, float), np.asarray(target, float)
    for triangles in (source, target):
        if (
            triangles.ndim != 3
            or triangles.shape[1:] != (3, 3)
            or not np.isfinite(triangles).all()
            or np.abs(triangles).max(initial=0) > 1e8
            or len(triangles) > 2000000
        ):
            raise ValueError("bounded finite complete triangle sets required")
    if (
        not np.isfinite(tolerance_m)
        or tolerance_m < 0
        or type(max_tests) is not int
        or max_tests < 0
        or type(max_depth) is not int
        or not 0 <= max_depth <= 24
        or type(neighbours) is not int
        or not 1 <= neighbours <= 16
    ):
        raise ValueError("explicit finite tolerance and bounded proof work required")
    exact = np.full(len(source), -1, dtype=np.int64)
    work = {
        "exact_triangles": 0,
        "patch_tests": 0,
        "corner_witness_tests": 0,
        "kd_queries": 0,
        "splits": 0,
        "accepted_patches": 0,
        "max_depth": 0,
    }
    result = SurfaceBound(False, None, exact, [], [], work, "pending")
    if not len(source):
        result.complete = True
        result.upper_m = 0.0
        result.status = "complete"
        return result
    if not len(target):
        result.status = "empty_target"
        return result
    keys = triangle_keys(target)
    order = np.argsort(keys)
    sorted_keys = keys[order]
    source_keys = triangle_keys(source)
    at = np.searchsorted(sorted_keys, source_keys)
    hit = at < len(keys)
    hit[hit] &= sorted_keys[at[hit]] == source_keys[hit]
    exact[hit] = order[at[hit]]
    work["exact_triangles"] = int(hit.sum())
    pending = [(int(i), 1, np.eye(3), 0) for i in np.flatnonzero(~hit)]
    tree = cKDTree(target.mean(axis=1))
    k = min(neighbours, len(target))
    per_patch = 12 * k
    upper = 0.0
    while pending:
        size = min(
            128, len(pending), (max_tests - work["corner_witness_tests"]) // per_patch
        )
        if not size:
            result.status = "budget"
            return result
        batch = [pending.pop() for _ in range(size)]
        original = np.array([source[i] for i, _, _, _ in batch])
        weights = np.array([w for _, _, w, _ in batch])
        sample = (
            affine_points(original, weights)[0] + affine_points(original, weights)[1]
        ) * 0.5
        query = np.concatenate((sample, sample.mean(axis=1, keepdims=True)), axis=1)
        candidates = tree.query(query, k=k)[1].reshape(size, 4 * k)
        values, targets, convex = _witnesses(original, weights, target, candidates)
        work["patch_tests"] += size
        work["corner_witness_tests"] += size * per_patch
        work["kd_queries"] += 4 * size
        for row, (index, path, w, depth) in enumerate(batch):
            work["max_depth"] = max(work["max_depth"], depth)
            if values[row] <= tolerance_m:
                result.leaves.append(
                    (index, path, int(targets[row]), convex[row], float(values[row]))
                )
                upper = max(upper, float(values[row]))
                work["accepted_patches"] += 1
            elif depth == max_depth:
                result.status = "depth"
                return result
            else:
                sides = np.sum(
                    (sample[row] - np.roll(sample[row], -1, axis=0)) ** 2, axis=1
                )
                edge = int(sides.argmax())
                first, second = bisect(w, edge)
                result.splits.append((index, path, edge))
                work["splits"] += 1
                pending.extend(
                    (
                        (index, 2 * path, first, depth + 1),
                        (index, 2 * path + 1, second, depth + 1),
                    )
                )
    result.complete = True
    result.upper_m = upper
    result.status = "complete"
    return result
