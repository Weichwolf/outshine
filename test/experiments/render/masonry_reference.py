"""Independent exact-area reference: clip periodic rectangles against the pixel.

Ordinary footprints use Shapely's vectorised polygon intersection. Rank-one
and zero-area material footprints use the earlier independent screen-space
half-plane clipping reference; their screen pixel area remains one.
"""

import itertools

import numpy as np
import shapely
from facade_footprint import screen_rectangle
from masonry_filter import BAND_DARKENING, BAND_M, BRICK_FRACTION, JOINT_M, MODULE_M


def bricks(low, high):
    rows = range(
        int(np.floor(low[1] / MODULE_M[1])) - 1, int(np.ceil(high[1] / MODULE_M[1])) + 1
    )
    result = []
    for row in rows:
        y = row * MODULE_M[1]
        shift = (row % 2) * MODULE_M[0] / 2
        columns = range(
            int(np.floor((low[0] - shift) / MODULE_M[0])) - 1,
            int(np.ceil((high[0] - shift) / MODULE_M[0])) + 1,
        )
        for col in columns:
            x = col * MODULE_M[0] + shift
            lower = np.array([x, y]) + JOINT_M / 2
            upper = np.array([x, y]) + MODULE_M - JOINT_M / 2
            if np.all(upper > low) and np.all(lower < high):
                result.append([*lower, *upper])
    return np.array(result).reshape(-1, 4)


def periodic_rectangles(low, high, cell, lower, upper):
    spans = [
        range(int(np.floor(low[a] / cell[a])) - 1, int(np.ceil(high[a] / cell[a])) + 1)
        for a in range(2)
    ]
    result = []
    for i, j in itertools.product(*spans):
        point = cell * [i, j]
        lo, hi = point + lower, point + upper
        if np.all(hi > low) and np.all(lo < high):
            result.append([*lo, *hi])
    return np.array(result).reshape(-1, 4)


def features(low, high, profile):
    if profile[2] == 0 or profile[3] == 0:
        return np.empty((0, 4)), np.empty((0, 4))
    opening_low, opening_high = (
        profile[:2] * (1 - profile[2:]) / 2,
        profile[:2] * (1 + profile[2:]) / 2,
    )
    openings = periodic_rectangles(low, high, profile[:2], opening_low, opening_high)
    # A finite interval covers the pixel in x; only the y direction repeats.
    rows = range(
        int(np.floor(low[1] / profile[1])) - 1, int(np.ceil(high[1] / profile[1])) + 1
    )
    bands = np.array(
        [
            [
                low[0] - 1,
                row * profile[1] - BAND_M / 2,
                high[0] + 1,
                row * profile[1] + BAND_M / 2,
            ]
            for row in rows
        ]
    )
    return openings, bands


def overlap(rectangles, rectangle):
    lo = np.maximum(rectangles[:, :2], rectangle[:2])
    hi = np.minimum(rectangles[:, 2:], rectangle[2:])
    keep = np.all(hi > lo, axis=1)
    return np.column_stack([lo[keep], hi[keep]])


def exact_one(at, dx, dy, profile):
    width = np.abs(dx) + np.abs(dy)
    low, high = at - width / 2, at + width / 2
    rectangles = bricks(low, high)
    openings, bands = features(low, high, profile)
    determinant = abs(np.linalg.det(np.array([dx, dy])))
    if determinant <= 1e-12 * max(1.0, np.linalg.norm(dx) * np.linalg.norm(dy)):

        def total(items):
            return sum(screen_rectangle(at, dx, dy, r[:2], r[2:]) for r in items)
    else:
        pixel = shapely.Polygon(
            [
                at - dx / 2 - dy / 2,
                at + dx / 2 - dy / 2,
                at + dx / 2 + dy / 2,
                at - dx / 2 + dy / 2,
            ]
        )

        def total(items):
            if not len(items):
                return 0.0
            boxes = shapely.box(items[:, 0], items[:, 1], items[:, 2], items[:, 3])
            return float(
                np.sum(shapely.area(shapely.intersection(boxes, pixel))) / pixel.area
            )

    opaque = 1 - total(openings) - BAND_DARKENING * total(bands)
    brick = total(rectangles)
    work = len(rectangles) + len(openings) + len(bands)
    for items, weight in ((openings, 1), (bands, BAND_DARKENING)):
        for rectangle in items:
            clipped = overlap(rectangles, rectangle)
            brick -= weight * total(clipped)
            work += len(clipped)
    return BRICK_FRACTION * opaque - brick, work


def exact(values, profile):
    results = [exact_one(at, dx, dy, profile) for at, dx, dy in values]
    field, work = np.array(results).T
    assert (
        np.isfinite(field).all()
        and field.min() >= -0.16 - 1e-8
        and field.max() <= 0.84 + 1e-8
    )
    return field, work


def controls():
    profile = np.array([4.0, 3.0, 0.0, 0.0])
    known = [
        ([0.125, 0.04], [0, 0], [0, 0], -0.16),
        ([0.0, 0.04], [0, 0], [0, 0], 0.84),
        ([0.125, 0.0], [0, 0], [0, 0], 0.84),
        ([0.125, 0.12], [0, 0], [0, 0], 0.84),
        ([0.0625, 0.12], [0, 0], [0, 0], -0.16),
        ([0.125, 0.08], [0.25, 0], [0, 0.16], 0.0),
        ([0.125, 0.04], [0.25, 0], [0, 0], -0.12),
        ([0.0625, 0.08], [0, 0.16], [0, 0], -0.035),
    ]
    for at, dx, dy, expected in known:
        actual, _ = exact_one(np.array(at), np.array(dx), np.array(dy), profile)
        np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-10)
    return len(known)
