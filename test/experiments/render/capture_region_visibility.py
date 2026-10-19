"""Exact single-triangle umbra control for capture reuse (#844).

Keep original geometry unless a supplied opaque triangle covers the whole target
from every eye in a closed ball. This controls primary visibility only; it does
not certify sampled capture patches, transparency, shadows or native arithmetic.
"""

import math
from fractions import Fraction

import numpy as np


def _vector(value):
    return tuple(Fraction(float(x)) for x in value)


def _subtract(a, b):
    return tuple(x - y for x, y in zip(a, b, strict=True))


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b, strict=True))


def covered_from_ball(
    blocker, target, eye, radius_m, *, opaque=False, cull_backfaces=False
):
    """Sufficient exact predicate on finite Float64 triangle/eye coordinates.

    Every supporting side-plane expression is affine in eye and target position.
    Its minimum on an eye ball is g-r*|n|. Positive g and g²>r²*|n|²
    prove strict containment without rounding a square root. Checking the three
    target vertices proves containment of their complete convex triangle.
    """
    for value, shape in ((blocker, (3, 3)), (target, (3, 3)), (eye, (3,))):
        if not isinstance(value, np.ndarray) or value.shape != shape:
            raise ValueError("bounded Float64 triangle/eye arrays required")
        if value.dtype != np.float64 or not np.isfinite(value).all():
            raise ValueError("finite Float64 geometry required")
    if not math.isfinite(radius_m) or radius_m < 0:
        raise ValueError("finite nonnegative eye radius required")
    report = {"covered": False, "exact_plane_tests": 0}
    if opaque is not True:
        return dict(report, reason="opaque_input_required")
    a, b, c = map(_vector, blocker)
    points = list(map(_vector, target))
    origin, radius = _vector(eye), Fraction(float(radius_m))
    normal = _cross(_subtract(b, a), _subtract(c, a))
    signed = _dot(normal, _subtract(origin, a))
    if signed == 0:
        return dict(report, reason="degenerate_blocker_or_eye_on_plane")
    if cull_backfaces and signed < 0:
        return dict(report, reason="blocker_back_facing")
    sign = 1 if signed > 0 else -1

    def positive_over_ball(margin, direction):
        report["exact_plane_tests"] += 1
        return margin > 0 and margin * margin > radius * radius * _dot(
            direction, direction
        )

    if not positive_over_ball(abs(signed), normal):
        return dict(report, reason="observer_ball_crosses_blocker_plane")
    for point in points:
        report["exact_plane_tests"] += 1
        if sign * _dot(normal, _subtract(point, a)) >= 0:
            return dict(report, reason="target_not_strictly_behind_blocker")
    for left, right in ((a, b), (b, c), (c, a)):
        edge = _subtract(right, left)
        for point in points:
            side_normal = _cross(edge, _subtract(point, left))
            margin = sign * _dot(side_normal, _subtract(origin, left))
            if not positive_over_ball(margin, side_normal):
                return dict(report, reason="target_not_strictly_inside_umbra")
    return dict(report, covered=True, reason="exact_single_opaque_triangle_umbra")
