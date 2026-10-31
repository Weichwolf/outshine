"""Exact whole-surface error for two bounded single-valued triangle charts.

This measures geometric/linear-varying correspondence, not shader, raster,
visibility, material, screen-space or World admission. Callers retain originals
on ValueError and keep distinct source/part/material/normal/code layers apart.
"""

from fractions import Fraction as F
from itertools import combinations

import numpy as np


def cross(a, b, p):
    return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])


def clip(subject, triangle):
    polygon = list(subject)
    for a, b in zip(triangle, (*triangle[1:], triangle[0]), strict=True):
        result = []
        if not polygon:
            break
        p = polygon[-1]
        dp = cross(a, b, p)
        for q in polygon:
            dq = cross(a, b, q)
            if (dp < 0) != (dq < 0):
                t = dp / (dp - dq)
                result.append(tuple(x + t * (y - x) for x, y in zip(p, q)))
            if dq >= 0:
                result.append(q)
            p, dp = q, dq
        polygon = list(dict.fromkeys(result))
    return polygon


def twice_area(polygon):
    if len(polygon) < 3:
        return F(0)
    return abs(
        sum(
            p[0] * q[1] - p[1] * q[0]
            for p, q in zip(polygon, (*polygon[1:], polygon[0]), strict=True)
        )
    )


def value(triangle, rows, point):
    denominator = cross(*triangle)
    weights = (
        cross(triangle[1], triangle[2], point) / denominator,
        cross(triangle[2], triangle[0], point) / denominator,
        cross(triangle[0], triangle[1], point) / denominator,
    )
    return tuple(
        sum(w * row[j] for w, row in zip(weights, rows, strict=True))
        for j in range(len(rows[0]))
    )


def chart(positions, fields, faces, axes):
    if (
        positions.dtype != np.float32
        or positions.ndim != 2
        or positions.shape[1] != 3
        or not 3 <= len(positions) <= 64
        or fields.dtype != np.float32
        or fields.ndim != 2
        or fields.shape[0] != len(positions)
        or not 0 <= fields.shape[1] <= 16
        or not np.isfinite(positions).all()
        or not np.isfinite(fields).all()
        or faces.dtype.kind not in "iu"
        or faces.ndim != 2
        or faces.shape[1] != 3
        or not 1 <= len(faces) <= 32
        or faces.min() < 0
        or faces.max() >= len(positions)
    ):
        raise ValueError("Bounded finite Float32 positions/varyings and faces required")
    rows = [tuple(F(float(v)) for v in row) for row in np.c_[positions, fields]]
    triangles, values, areas, signs = [], [], [], set()
    for face in faces:
        data = [rows[int(i)] for i in face]
        triangle = [tuple(row[i] for i in axes) for row in data]
        orientation = cross(*triangle)
        if not orientation:
            raise ValueError("Degenerate projected triangle retains original")
        signs.add(orientation > 0)
        if orientation < 0:
            triangle.reverse()
            data.reverse()
        triangles.append(tuple(triangle))
        values.append(tuple(data))
        areas.append(abs(orientation))
    if len(signs) != 1:
        raise ValueError("Inconsistent chart winding retains original")
    for a, b in combinations(range(len(triangles)), 2):
        intersection = clip(triangles[a], triangles[b])
        if twice_area(intersection):
            raise ValueError("Overlapping projected layers retain originals")
        if any(
            value(triangles[a], values[a], point)
            != value(triangles[b], values[b], point)
            for point in intersection
        ):
            raise ValueError("Discontinuous chart boundary retains originals")
    return triangles, values, areas, signs.pop()


def validate_axes(axes):
    if (
        type(axes) is not tuple
        or len(axes) != 2
        or any(type(i) is not int or i not in (0, 1, 2) for i in axes)
        or axes[0] == axes[1]
    ):
        raise ValueError("Two distinct source coordinate axes required")


def compare(original, replacement, *, axes):
    """Inputs are (positions, linear fields, indices), with identical field order.

    Every positive-area overlay cell has affine 3D/field differences. Squared
    Euclidean error is convex; its maximum is attained at a cell vertex. Exact
    coverage and single-valued continuity in both charts yield a correspondence
    over their entire common closed domain, hence a two-sided Hausdorff upper
    bound in source coordinate units. It need not be the tight Hausdorff distance.
    Numeric zero does not certify signed-zero words or native interpolation.
    """
    validate_axes(axes)
    old, old_values, old_area, old_sign = chart(*original, axes)
    new, new_values, new_area, new_sign = chart(*replacement, axes)
    if old_sign != new_sign or len(old_values[0][0]) != len(new_values[0][0]):
        raise ValueError("Matching original winding and field inventory required")
    old_covered = [F(0)] * len(old)
    new_covered = [F(0)] * len(new)
    maximum = F(0)
    field_maximum = [F(0)] * (len(old_values[0][0]) - 3)
    cells = vertices = 0
    witness = None
    for i, a in enumerate(old):
        for j, b in enumerate(new):
            polygon = clip(a, b)
            area = twice_area(polygon)
            old_covered[i] += area
            new_covered[j] += area
            cells += bool(area)
            for point in polygon:
                x, y = value(a, old_values[i], point), value(b, new_values[j], point)
                difference = tuple(u - v for u, v in zip(x, y, strict=True))
                squared = sum(d * d for d in difference[:3])
                vertices += 1
                if squared > maximum:
                    maximum, witness = squared, (i, j, point)
                for k, d in enumerate(difference[3:]):
                    field_maximum[k] = max(field_maximum[k], abs(d))
    if old_covered != old_area or new_covered != new_area:
        raise ValueError("Different complete chart domains retain originals")
    return {
        "position_error_squared": maximum,
        "field_error_absolute": tuple(field_maximum),
        "position_witness": witness,
        "triangle_pairs": len(old) * len(new),
        "positive_area_cells": cells,
        "overlay_vertex_evaluations": vertices,
    }
