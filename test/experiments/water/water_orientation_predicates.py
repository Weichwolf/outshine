"""Outward binary64 orientation intervals, with bounded rational fallback."""

from fractions import Fraction

import numpy as np


def _product(a, b, c, d):
    values = np.stack((a * c, a * d, b * c, b * d))
    return np.nextafter(values.min(axis=0), -np.inf), np.nextafter(
        values.max(axis=0), np.inf
    )


def _exact_orientation(a, b, c, budget, rational_c):
    budget.tick("exact_orientations")
    a, b = [tuple(Fraction(float(x)) for x in p) for p in (a, b)]
    c = rational_c if rational_c is not None else tuple(Fraction(float(x)) for x in c)
    determinant = budget.check(
        (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    )
    return int(determinant > 0) - int(determinant < 0)


def orientations(a, b, c, budget, *, exact=False, c_bounds=None, rational_c=None):
    """Signs refer to original stored A/B/C or an explicitly rational C.

    Inputs have been checked finite and within ±1e8m by the membership caller.
    Each basic operation is outward rounded; multiplication considers all four
    interval endpoint products. IEEE gradual underflow is required. No float
    determinant sign is accepted if its enclosure contains zero.
    """
    a, b, c = np.broadcast_arrays(a, b, c)
    shape = a.shape[:-1]
    count = int(np.prod(shape, dtype=np.int64))
    budget.charge("orientation_tests", count)
    result = np.zeros(shape, dtype=np.int8)
    if exact:
        uncertain = np.ones(shape, bool)
    else:
        c_low, c_high = (c, c) if c_bounds is None else c_bounds
        with np.errstate(under="ignore"):
            ab = b - a
            ab_low, ab_high = np.nextafter(ab, -np.inf), np.nextafter(ab, np.inf)
            ac_low = np.nextafter(c_low - a, -np.inf)
            ac_high = np.nextafter(c_high - a, np.inf)
            first_low, first_high = _product(
                ab_low[..., 0], ab_high[..., 0], ac_low[..., 1], ac_high[..., 1]
            )
            second_low, second_high = _product(
                ab_low[..., 1], ab_high[..., 1], ac_low[..., 0], ac_high[..., 0]
            )
            low = np.nextafter(first_low - second_high, -np.inf)
            high = np.nextafter(first_high - second_low, np.inf)
        result[low > 0] = 1
        result[high < 0] = -1
        uncertain = (low <= 0) & (high >= 0)
    for i in np.flatnonzero(uncertain.ravel()):
        result.ravel()[i] = _exact_orientation(
            a.reshape(-1, 2)[i],
            b.reshape(-1, 2)[i],
            c.reshape(-1, 2)[i],
            budget,
            rational_c,
        )
    return result


def centroid(triangle, budget):
    """A rational interior point plus an enclosure, never an unchecked float mean."""
    budget.tick("rational_centroids")
    point = tuple(
        budget.check(
            sum((Fraction(float(v)) for v in triangle[:, axis]), Fraction(0)) / 3
        )
        for axis in (0, 1)
    )
    values = np.array([float(x) for x in point])
    low, high = values.copy(), values.copy()
    for i, value in enumerate(values):
        if Fraction(float(value)) > point[i]:
            low[i] = np.nextafter(value, -np.inf)
        if Fraction(float(value)) < point[i]:
            high[i] = np.nextafter(value, np.inf)
    if any(
        not Fraction(float(a)) <= value <= Fraction(float(b))
        for a, value, b in zip(low, point, high, strict=True)
    ):
        raise ValueError("rational centroid was not enclosed")
    return point, values, (low, high)
