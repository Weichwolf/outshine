"""Outward arithmetic plus exact dyadic boundary decisions for shadow controls."""

from fractions import Fraction

import numpy as np
from facing_intervals import length, point, sub


def containment(points, sphere):
    points = np.asarray(points, dtype=float)
    sphere = np.asarray(sphere, dtype=float)
    low, high = length(sub(point(points), point(sphere[:3])))
    outside = low > sphere[3]
    ambiguous = np.flatnonzero((low <= sphere[3]) & (high >= sphere[3]))
    centre = [Fraction(float(x)) for x in sphere[:3]]
    radius2 = Fraction(float(sphere[3])) ** 2
    for i in ambiguous:
        distance2 = sum(
            (Fraction(float(x)) - c) ** 2 for x, c in zip(points[i], centre)
        )
        outside[i] = distance2 > radius2
    return {
        "outside": int(outside.sum()),
        "exact_boundary_queries": len(ambiguous),
        "references": len(points),
        "minimum_margin_lower": float((sphere[3] - high).min())
        if len(points)
        else None,
    }


def shader_points(points, model, shift=(0, 0, 0)):
    """One sequential Float32 execution, not a universal GPU reassociation proof."""
    model = np.asarray(model, dtype=np.float32).copy()
    model[:3, 3] += np.asarray(shift, dtype=np.float32)
    points = np.column_stack((points, np.ones(len(points)))).astype(np.float32)
    result = np.zeros((len(points), 4), dtype=np.float32)
    for column in range(4):
        result += points[:, column : column + 1] * model[:, column]
    return result


def ball_violations(inner, outer):
    """Exact represented-ball inclusion; no sqrt or rounded radius subtraction."""
    centre = [Fraction(float(x)) for x in outer[:3]]
    radius = Fraction(float(outer[3]))
    violations = 0
    for sphere in inner:
        gap = radius - Fraction(float(sphere[3]))
        squared = sum((Fraction(float(x)) - c) ** 2 for x, c in zip(sphere[:3], centre))
        violations += int(gap < 0 or squared > gap * gap)
    return violations
