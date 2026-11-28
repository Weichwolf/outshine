"""Outward-rounded bounds for projecting a bounded source onto its capture plane.

This bounds geometric displacement only. Coverage, disocclusion, material response,
capture discretization and filtering still need their own qualification.
"""

import math
from dataclasses import dataclass
from itertools import product


def point(value):
    return (value, value)


def outward(low, high):
    if not math.isfinite(low) or not math.isfinite(high):
        raise ValueError("Projection interval overflow")
    return math.nextafter(low, -math.inf), math.nextafter(high, math.inf)


def add(a, b):
    return outward(a[0] + b[0], a[1] + b[1])


def subtract(a, b):
    return outward(a[0] - b[1], a[1] - b[0])


def multiply(a, b):
    values = [x * y for x in a for y in b]
    return outward(min(values), max(values))


def divide(a, b):
    if b[0] <= 0 <= b[1]:
        raise ValueError("Projection interval crosses zero")
    reciprocal = outward(1 / b[1], 1 / b[0])
    return multiply(a, reciprocal)


def dot(a, b):
    value = point(0.0)
    for x, y in zip(a, b, strict=True):
        value = add(value, multiply(x, y))
    return value


def vector(values):
    values = tuple(float(v) for v in values)
    if len(values) != 3 or not all(map(math.isfinite, values)):
        raise ValueError("Finite three-dimensional projection vector required")
    return values


@dataclass(frozen=True)
class CapturePlane:
    lower: tuple
    upper: tuple
    centre: tuple
    toward_eye: tuple

    def __post_init__(self):
        for name in ("lower", "upper", "centre", "toward_eye"):
            object.__setattr__(self, name, vector(getattr(self, name)))
        if any(a > b for a, b in zip(self.lower, self.upper, strict=True)):
            raise ValueError("Ordered source bounds required")
        length = sum(v * v for v in self.toward_eye)
        if abs(length - 1) > 1e-9:
            raise ValueError("Unit capture direction required")


def relative_dot(relative, axis, offsets):
    result = dot(relative, axis)
    for direction, interval in offsets:
        result = subtract(result, multiply(dot(direction, axis), interval))
    return result


def corner_terms(plane, eye, basis, offsets):
    direction = tuple(map(point, plane.toward_eye))
    camera = [tuple(map(point, axis)) for axis in basis]
    projected = [dot(axis, direction) for axis in camera]
    coefficients = [
        tuple(
            subtract(multiply(projected[i], f), multiply(projected[2], a))
            for f, a in zip(camera[2], camera[i], strict=True)
        )
        for i in (0, 1)
    ]
    squared = dot(direction, direction)
    terms = []
    for corner in product(*zip(plane.lower, plane.upper, strict=True)):
        local = [
            subtract(point(p), point(c))
            for p, c in zip(corner, plane.centre, strict=True)
        ]
        offset = divide(dot(local, direction), squared)
        relative = [
            subtract(point(p), point(e)) for p, e in zip(corner, eye, strict=True)
        ]
        forward = relative_dot(relative, camera[2], offsets)
        plane_forward = subtract(forward, multiply(offset, projected[2]))
        numerators = [relative_dot(relative, axis, offsets) for axis in coefficients]
        terms.append((offset, forward, plane_forward, numerators))
    return terms


def displacement_bound(plane, eye, basis, focal_px, *, eye_offsets=()):
    eye = vector(eye)
    basis = tuple(vector(axis) for axis in basis)
    focal_px = float(focal_px)
    if len(basis) != 3 or not math.isfinite(focal_px) or focal_px <= 0:
        raise ValueError("Finite positive focal length and camera basis required")
    for i in range(3):
        for j in range(3):
            if (
                abs(
                    sum(a * b for a, b in zip(basis[i], basis[j], strict=True))
                    - (i == j)
                )
                > 1e-9
            ):
                raise ValueError("Orthonormal camera basis required")
    offsets = []
    if len(eye_offsets) > 3:
        raise ValueError("At most three bounded eye-region axes required")
    for axis, lower, upper in eye_offsets:
        axis = tuple(map(point, vector(axis)))
        lower, upper = float(lower), float(upper)
        if not all(map(math.isfinite, (lower, upper))) or lower > upper:
            raise ValueError("Ordered finite eye-region interval required")
        offsets.append((axis, (lower, upper)))
    terms = corner_terms(plane, eye, basis, offsets)
    source_near = min(t[1][0] for t in terms)
    plane_near = min(t[2][0] for t in terms)
    if min(source_near, plane_near) <= 0:
        return {"screen_l1_upper_px": None, "reason": "source_or_plane_crosses_eye"}
    height = max(max(abs(x) for x in t[0]) for t in terms)
    denominator = multiply(point(source_near), point(plane_near))
    bounds = []
    for axis in (0, 1):
        numerator = max(max(abs(x) for x in t[3][axis]) for t in terms)
        value = multiply(multiply(point(height), point(numerator)), point(focal_px))
        bounds.append(divide(value, denominator)[1])
    return {
        "axis_upper_px": tuple(bounds),
        "screen_l1_upper_px": add(point(bounds[0]), point(bounds[1]))[1],
        "minimum_forward_m": (source_near, plane_near),
        "source_corners": len(terms),
        "reason": "geometric_displacement_only",
    }
