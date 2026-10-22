"""Positive screen-area quadrature on convex pixel/support intersections.

The polygon is a work support, never an occupancy or visibility certificate.
Inside-support detail still needs transport integration at the generated nodes.
"""

import itertools

import numpy as np


def convex_hull(points):
    points = np.unique(np.asarray(points, float), axis=0)
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite screen points required")
    if len(points) < 3:
        return points

    def half(sequence):
        result = []
        for point in sequence:
            while len(result) >= 2:
                a, b = result[-1] - result[-2], point - result[-1]
                if a[0] * b[1] - a[1] * b[0] > 0:
                    break
                result.pop()
            result.append(point)
        return result

    return np.array(half(points)[:-1] + half(points[::-1])[:-1])


def clip_axis(polygon, axis, value, positive):
    if not len(polygon):
        return polygon
    result = []
    previous = polygon[-1]
    old = (previous[axis] - value) * positive
    for point in polygon:
        new = (point[axis] - value) * positive
        if (old >= 0) != (new >= 0):
            crossing = previous + (point - previous) * old / (old - new)
            crossing[axis] = value
            result.append(crossing)
        if new >= 0:
            result.append(point)
        previous, old = point, new
    return np.asarray(result, float).reshape(-1, 2)


def clip_pixel(polygon, pixel):
    polygon = np.asarray(polygon, float) - pixel
    for axis in range(2):
        polygon = clip_axis(polygon, axis, 0.0, 1)
        polygon = clip_axis(polygon, axis, 1.0, -1)
    return np.unique(polygon, axis=0)


def strip_quadrature(polygon, roots, weights):
    """Split at x vertices; vertical cross-section endpoints are then affine."""
    polygon = convex_hull(polygon)
    if len(polygon) < 3:
        return np.empty((0, 2)), np.empty(0), 0
    following = np.roll(polygon, -1, axis=0)
    delta = following - polygon
    knots = np.unique(polygon[:, 0])
    points, areas = [], []
    strips = 0
    for left, right in itertools.pairwise(knots):
        x = left + (right - left) * roots
        y = []
        for start, step, end in zip(polygon, delta, following, strict=True):
            if step[0] != 0 and min(start[0], end[0]) <= (left + right) / 2 <= max(
                start[0], end[0]
            ):
                y.append(start[1] + step[1] * (x - start[0]) / step[0])
        if not y:
            continue
        low, high = np.min(y, axis=0), np.max(y, axis=0)
        at_y = low[:, None] + (high - low)[:, None] * roots
        points.append(np.column_stack((np.repeat(x, len(roots)), at_y.ravel())))
        areas.append(
            (
                (right - left) * weights[:, None] * (high - low)[:, None] * weights
            ).ravel()
        )
        strips += 1
    return np.concatenate(points), np.concatenate(areas), strips


def pixel_quadrature(polygon, box, *, order=2, skip_full=False):
    """Return screen nodes, row-major pixel IDs, area weights and work counts.

    Weights sum to polygon/pixel intersection area; they are never normalized
    to one. Empty/outside support contributes zero signed scene correction.
    """
    if type(order) is not int or not 1 <= order <= 16:
        raise ValueError("quadrature order 1..16 required")
    box = np.asarray(box)
    if box.shape != (2, 2) or np.any(box != np.floor(box)) or np.any(box[1] < box[0]):
        raise ValueError("ordered integer pixel box required")
    polygon = convex_hull(polygon)
    roots, weights = np.polynomial.legendre.leggauss(order)
    roots, weights = (roots + 1) / 2, weights / 2
    work = {
        "pixel_clip_tests": 0,
        "clipped_pixels": 0,
        "axis_strips": 0,
        "max_clip_vertices": 0,
        "full_pixel_ids": [],
    }
    points, owners, areas = [], [], []
    width = int(box[1, 0] - box[0, 0])
    if len(polygon) >= 3:
        low = np.maximum(box[0], np.floor(polygon.min(axis=0))).astype(int)
        high = np.minimum(box[1], np.ceil(polygon.max(axis=0))).astype(int)
        for y in range(low[1], high[1]):
            for x in range(low[0], high[0]):
                pixel = np.array([x, y])
                clipped = clip_pixel(polygon, pixel)
                work["pixel_clip_tests"] += 1
                work["max_clip_vertices"] = max(work["max_clip_vertices"], len(clipped))
                owner = (y - box[0, 1]) * width + x - box[0, 0]
                if skip_full and np.array_equal(
                    clipped, [[0, 0], [0, 1], [1, 0], [1, 1]]
                ):
                    work["full_pixel_ids"].append(int(owner))
                    continue
                if len(clipped) < 3:
                    continue
                xy, weight, count = strip_quadrature(clipped, roots, weights)
                positive = weight > 0
                if not positive.any():
                    continue
                points.append(xy[positive] + pixel)
                areas.append(weight[positive])
                owners.append(
                    np.full(
                        positive.sum(),
                        owner,
                        np.int64,
                    )
                )
                work["clipped_pixels"] += 1
                work["axis_strips"] += count
    xy = np.concatenate(points) if points else np.empty((0, 2))
    ids = np.concatenate(owners) if owners else np.empty(0, np.int64)
    weight = np.concatenate(areas) if areas else np.empty(0)
    work.update(
        quadrature_samples=len(xy),
        named_output_bytes=xy.nbytes + ids.nbytes + weight.nbytes,
    )
    return xy, ids, weight, work
