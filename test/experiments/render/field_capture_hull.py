"""Conservative source AABB from the existing complete-capture metadata domain.

The producer must bind every original to |right|,|up| <= H, |toward| <= 2H
around Centre. First-hit texels do not establish this assertion. Derive once per
immutable descriptor, not per pose. Exact rational inversion keeps finite capture
axes honest; the resulting outward binary64 endpoints are the admission input.
"""

import math
from fractions import Fraction as F
from itertools import product

from capture_box_envelope import ceiling
from field_view_sampling import FieldGrid


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def capture_hull(grid, *, complete_original_capture_domain):
    if complete_original_capture_domain is not True:
        raise ValueError("Complete original capture-domain binding required")
    if not isinstance(grid, FieldGrid):
        raise TypeError("Existing finite FieldGrid metadata required")
    rows = tuple(
        tuple(F(float(v)) for v in axis) for axis in (*grid.axes(), grid.toward_eye)
    )
    columns = (
        cross(rows[1], rows[2]),
        cross(rows[2], rows[0]),
        cross(rows[0], rows[1]),
    )
    determinant = sum(a * b for a, b in zip(rows[0], columns[0], strict=True))
    if determinant == 0:
        raise ValueError("Invertible capture frame required")
    extents = (F(grid.half_extent_m), F(grid.half_extent_m), 2 * F(grid.half_extent_m))
    vertices = []
    for signs in product((-1, 1), repeat=3):
        corner = []
        for axis in range(3):
            value = F(grid.centre[axis]) + sum(
                column[axis] / determinant * extent * sign
                for column, extent, sign in zip(columns, extents, signs, strict=True)
            )
            low, high = -ceiling(-value), ceiling(value)
            if not math.isfinite(low) or not math.isfinite(high):
                raise ValueError("Capture-derived hull overflow")
            corner.append((low, high))
        vertices.append(tuple(corner))
    lower = tuple(min(v[i][0] for v in vertices) for i in range(3))
    upper = tuple(max(v[i][1] for v in vertices) for i in range(3))
    return {"lower": lower, "upper": upper, "vertices": tuple(vertices)}
