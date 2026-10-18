"""Exact dyadic collinearity only for numerically unresolved triangle normals."""

from fractions import Fraction

import numpy as np


def exact_null_candidates(points, normals):
    candidates = np.all(normals == 0, axis=1)
    for index in np.flatnonzero(candidates):
        a, b, c = [
            tuple(Fraction.from_float(float(value)) for value in vertex)
            for vertex in points[index]
        ]
        u = tuple(y - x for x, y in zip(a, b, strict=True))
        v = tuple(y - x for x, y in zip(a, c, strict=True))
        if any(u[i] * v[j] != u[j] * v[i] for i, j in ((0, 1), (0, 2), (1, 2))):
            raise ValueError(
                "Unresolved normal cancellation on an area-bearing triangle"
            )
    return candidates
