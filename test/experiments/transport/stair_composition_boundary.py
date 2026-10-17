"""Research comparison of equality predicates for already valid triangle coverages."""

import numpy as np
import shapely

GENERAL_UNION = shapely.union_all


def oriented_equal(before, after):
    """Sufficient directed-chain equality; inputs must be valid triangle coverages."""
    polygons = np.r_[np.asarray(before, object), np.asarray(after, object)]
    if not len(polygons) or not np.all(shapely.get_num_coordinates(polygons) == 4):
        return False
    coords = shapely.get_coordinates(polygons).reshape(-1, 4, 2)[:, :3]
    if not np.isfinite(coords).all():
        return False
    _xy, inverse = np.unique(coords.reshape(-1, 2), axis=0, return_inverse=True)
    faces = inverse.reshape(-1, 3)
    edges = faces[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
    low, high = edges.min(axis=1), edges.max(axis=1)
    codes = low.astype(np.uint64) * np.uint64(2**32) + high
    weight = np.empty(len(polygons), dtype=np.int8)
    for begin in range(0, len(polygons), 1024):
        rings = shapely.get_exterior_ring(polygons[begin : begin + 1024])
        weight[begin : begin + 1024] = np.where(shapely.is_ccw(rings), 1, -1)
    weight[len(before) :] *= -1
    coefficients = np.where(edges[:, 0] < edges[:, 1], 1, -1) * np.repeat(weight, 3)
    order = np.argsort(codes, kind="stable")
    ordered = codes[order]
    begin = np.r_[0, np.flatnonzero(ordered[1:] != ordered[:-1]) + 1]
    sums = np.add.reduceat(coefficients[order], begin, dtype=np.int64)
    return bool(np.all(sums == 0))


def equal_coverage(before, after, method):
    if method == "oriented" and oriented_equal(before, after):
        return True
    merge = shapely.coverage_union_all if method == "coverage" else GENERAL_UNION
    return bool(merge(before).symmetric_difference(merge(after)).area <= 1e-7)
