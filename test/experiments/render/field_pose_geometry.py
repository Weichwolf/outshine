"""Metadata-only geometric displacement for a whole source and its support plane.

Uses the #1663 perspective-difference identity and its directed interval kernels.
Projection rows consume camera (right, up, FORWARD, 1), with forward positive.
This grants no sampled coverage, lighting, depth or GPU arithmetic authority.
"""

import math
from itertools import product

from field_view_projection import (
    add,
    divide,
    dot,
    multiply,
    point,
    subtract,
    vector,
)


def operands(eye, basis, projection, raster):
    eye = vector(eye)
    basis = tuple(vector(axis) for axis in basis)
    if len(basis) != 3 or any(
        abs(sum(a * b for a, b in zip(x, y, strict=True)) - (i == j)) > 1e-9
        for i, x in enumerate(basis)
        for j, y in enumerate(basis)
    ):
        raise ValueError("Orthonormal camera basis required")
    projection = tuple(tuple(float(v) for v in row) for row in projection)
    if len(projection) != 4 or any(
        len(row) != 4 or not all(map(math.isfinite, row)) for row in projection
    ):
        raise ValueError(
            "Finite 4x4 projection in positive-forward coordinates required"
        )
    raster = tuple(raster)
    if len(raster) != 2 or any(
        type(v) is not int or not 1 <= v <= 2**24 for v in raster
    ):
        raise ValueError(
            "Positive integer width and height within binary32 integer range required"
        )
    return eye, basis, tuple(tuple(map(point, r)) for r in projection), raster


def clip_point(relative, basis, projection, *, direction=False):
    camera = [dot(relative, tuple(map(point, axis))) for axis in basis]
    return tuple(
        dot(row, (*camera, point(0.0 if direction else 1.0))) for row in projection
    )


def magnitude(interval):
    return max(abs(interval[0]), abs(interval[1]))


def hull_vertices(plane, source_hull):
    if source_hull is None:
        return tuple(
            tuple(map(point, p))
            for p in product(*zip(plane.lower, plane.upper, strict=True))
        )
    vertices = tuple(
        tuple(tuple(float(x) for x in interval) for interval in p) for p in source_hull
    )
    if len(vertices) != 8 or any(
        len(p) != 3
        or any(
            len(v) != 2
            or not all(map(math.isfinite, v))
            or not plane.lower[i] <= v[0] <= v[1] <= plane.upper[i]
            for i, v in enumerate(p)
        )
        for p in vertices
    ):
        raise ValueError(
            "Eight finite interval vertices inside the bound source AABB required"
        )
    return vertices


def projection_bound(
    plane, eye, basis, projection, raster, *, normal=None, source_hull=None
):
    """Bound p versus q=p-d*dot(p-c,n)/dot(d,n) over the complete source AABB.

    plane is the existing CapturePlane descriptor. Its centre lies on the actual
    physical support, d is the capture ray direction, and n can describe a fitted
    tilted support. A negative result retains the whole group's original owner.
    The finite supplied matrix is geometric data, not an unrounded ideal lens.
    Optional source_hull encloses the vertices of a bound complete-source convex
    hull; this preserves capture-frame correlation instead of expanding its AABB.
    """
    eye, basis, projection, raster = operands(eye, basis, projection, raster)
    return _projection_bound(
        plane,
        eye,
        raster,
        lambda position, direction=False: clip_point(
            position, basis, projection, direction=direction
        ),
        normal=normal,
        source_hull=source_hull,
    )


def _projection_bound(plane, eye, raster, clip, *, normal=None, source_hull=None):
    direction = tuple(map(point, plane.toward_eye))
    normal = tuple(map(point, vector(plane.toward_eye if normal is None else normal)))
    denominator = dot(direction, normal)
    if denominator[0] <= 0 <= denominator[1]:
        raise ValueError("Capture rays must intersect the support plane")
    projected_direction = clip(direction, direction=True)
    terms = []
    for corner in hull_vertices(plane, source_hull):
        local = tuple(
            subtract(p, point(c)) for p, c in zip(corner, plane.centre, strict=True)
        )
        height = divide(dot(local, normal), denominator)
        relative = tuple(
            subtract(p, point(e)) for p, e in zip(corner, eye, strict=True)
        )
        original = clip(relative)
        support = tuple(
            subtract(v, multiply(height, d))
            for v, d in zip(original, projected_direction, strict=True)
        )
        numerators = tuple(
            subtract(
                multiply(projected_direction[i], original[3]),
                multiply(projected_direction[3], original[i]),
            )
            for i in (0, 1)
        )
        terms.append((height, original, support, numerators))
    minimum_w = tuple(min(t[i][3][0] for t in terms) for i in (1, 2))
    if min(minimum_w) <= 0:
        return {
            "screen_l1_upper_px": None,
            "depth_inside": False,
            "reason": "source_or_support_crosses_projective_eye",
        }
    height = max(magnitude(t[0]) for t in terms)
    divisor = multiply(point(minimum_w[0]), point(minimum_w[1]))
    axes = []
    for axis, pixels in enumerate(raster):
        numerator = max(magnitude(t[3][axis]) for t in terms)
        value = multiply(multiply(point(height), point(numerator)), point(pixels / 2))
        axes.append(divide(value, divisor)[1])
    depth_inside = all(
        t[i][2][0] >= 0 and subtract(t[i][3], t[i][2])[0] >= 0
        for t in terms
        for i in (1, 2)
    )
    return {
        "axis_upper_px": tuple(axes),
        "screen_l1_upper_px": add(point(axes[0]), point(axes[1]))[1],
        "minimum_w": minimum_w,
        "depth_inside": depth_inside,
        "source_corners": len(terms),
        "reason": "geometric_displacement_only",
    }


def geometric_candidate(result, limit_px):
    limit_px = float(limit_px)
    if not math.isfinite(limit_px) or limit_px < 0:
        raise ValueError("Finite nonnegative geometric error budget required")
    error = result["screen_l1_upper_px"]
    return result["depth_inside"] and error is not None and error <= limit_px


def projection_matrix_bound(
    plane, clip_from_field, raster, *, normal=None, source_hull=None
):
    """Same geometric bound using an already enclosed field-to-clip matrix.

    No conversion to a nominal local camera or scalar matrix loses the input
    intervals. This is geometric data, not packed GPU matrix authority.
    """
    matrix = tuple(
        tuple(tuple(float(v) for v in entry) for entry in row)
        for row in clip_from_field
    )
    raster = tuple(raster)
    if (
        len(matrix) != 4
        or any(len(row) != 4 for row in matrix)
        or any(
            len(v) != 2 or not all(map(math.isfinite, v)) or v[0] > v[1]
            for row in matrix
            for v in row
        )
        or len(raster) != 2
        or any(type(v) is not int or not 1 <= v <= 2**24 for v in raster)
    ):
        raise ValueError(
            "Finite ordered 4x4 clip intervals and bounded raster required"
        )

    def clip(position, direction=False):
        p = (*position, point(0.0 if direction else 1.0))
        return tuple(dot(row, p) for row in matrix)

    return _projection_bound(
        plane, (0.0, 0.0, 0.0), raster, clip, normal=normal, source_hull=source_hull
    )
