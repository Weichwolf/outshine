"""One rational Bernstein half-split before display admission, not a fit."""

import math
from fractions import Fraction
from itertools import pairwise


def split_half(coefficients):
    """Return exact Bernstein coefficients on the two closed half intervals."""
    levels = [
        tuple(
            value if isinstance(value, Fraction) else Fraction(float(value))
            for value in coefficients
        )
    ]
    while len(levels[-1]) > 1:
        values = levels[-1]
        levels.append(tuple((a + b) / 2 for a, b in pairwise(values)))
    return tuple(level[0] for level in levels), tuple(
        level[-1] for level in reversed(levels)
    )


def upward(value):
    result = float(value)
    return math.nextafter(result, math.inf) if Fraction(result) < value else result


def derivative_controls(values, length):
    values = tuple(Fraction(float(value)) for value in values)
    length = Fraction(float(length))
    return tuple(3 * (b - a) / length for a, b in pairwise(values))


def refined_normal_departure(curve):
    """Cover each original span by exactly two full-width normal hulls.

    Subdivision and coefficient extrema are rational, rounded outward for the
    existing floating normal-bound arithmetic. No universal libm proof is added.
    """
    maximum = 0.0
    for height, bank, length, curvature in zip(
        curve.height, curve.bank, curve.lengths, curve.curvature_max, strict=True
    ):
        scale = 1 - Fraction(curve.width) * Fraction(float(curvature))
        if scale <= 0:
            raise ValueError("Folded full-width normal-offset recipe")
        minimum = float(scale)
        if Fraction(minimum) > scale:
            minimum = math.nextafter(minimum, -math.inf)
        heights = split_half(derivative_controls(height, length))
        banks = split_half(bank)
        rates = split_half(derivative_controls(bank, length))
        for h, b, db in zip(heights, banks, rates, strict=True):
            h1, bank_max, b1 = (upward(max(map(abs, values))) for values in (h, b, db))
            if bank_max >= math.pi / 2 or minimum <= 0:
                raise ValueError("Refined normal hull unavailable")
            q = math.tan(bank_max)
            q1 = b1 / math.cos(bank_max) ** 2
            p = (h1 + curve.width * q1) / minimum
            radius = math.hypot(p, q)
            if not math.isfinite(radius):
                raise ValueError("Unbounded refined normal departure")
            norm = math.hypot(1, radius)
            distance = (
                curve.thickness * (radius / norm) * math.sqrt(2 * (norm / (norm + 1)))
            )
            maximum = max(
                maximum, math.nextafter(distance, math.inf) if distance else 0.0
            )
    return maximum
