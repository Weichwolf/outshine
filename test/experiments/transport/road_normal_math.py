"""Exact rational polynomial ranges and bounded Taylor operations for normal cones."""

import itertools
from fractions import Fraction as F
from math import isqrt


def enclose(values):
    """Round outward to a bounded dyadic denominator, preserving exact inclusion."""
    scale = 1 << 96
    low, high = values
    return (
        F(low.numerator * scale // low.denominator, scale),
        F(-((-high.numerator * scale) // high.denominator), scale),
    )


def exact(value):
    return value if isinstance(value, F) else F(float(value))


def add(a, b):
    return enclose((a[0] + b[0], a[1] + b[1]))


def negative(a):
    return -a[1], -a[0]


def multiply(a, b):
    values = [x * y for x in a for y in b]
    return enclose((min(values), max(values)))


def divide(a, b):
    if b[0] <= 0 <= b[1]:
        raise ValueError("Normal interval denominator contains zero")
    return multiply(a, (1 / b[1], 1 / b[0]))


def square(a):
    values = (a[0] ** 2, a[1] ** 2)
    return enclose((F(0) if a[0] <= 0 <= a[1] else min(values), max(values)))


def sqrt_interval(a):
    if a[0] < 0:
        raise ValueError("Nonnegative squared normal length required")
    scale = 1 << 120
    low = F(isqrt(a[0].numerator * scale**2 // a[0].denominator), scale)
    high = F(isqrt(a[1].numerator * scale**2 // a[1].denominator) + 1, scale)
    return low, high


def polynomial_range(control, first, last):
    """Bernstein hull after affine reparameterization, including tiny extrapolation."""
    degree = len(control) - 1
    coefficients = []
    for index in range(degree + 1):
        values = list(map(exact, control))
        for t in [first] * (degree - index) + [last] * index:
            values = [(1 - t) * a + t * b for a, b in itertools.pairwise(values)]
        coefficients.append(values[0])
    return enclose((min(coefficients), max(coefficients)))


def trig_point(value):
    """Sin/cos enclosed by rational Taylor sums and Lagrange remainders.

    256 terms and 2^-100 remainder are fixed arithmetic-work/accuracy limits.
    For |x|>=512 no allowed remainder can meet that accuracy; retain originals.
    No platform libm accuracy assumption is used.
    """
    x = exact(value)
    if abs(x) >= 512:
        raise ValueError("Normal transcendental work budget exceeded")
    if not x:
        return (F(0), F(0)), (F(1), F(1))
    sine, cosine = x, F(1)
    s, c = sine, cosine
    for order in range(256):
        snext = -s * x * x / ((2 * order + 2) * (2 * order + 3))
        cnext = -c * x * x / ((2 * order + 1) * (2 * order + 2))
        if max(abs(snext), abs(cnext)) <= F(1, 1 << 100):
            return enclose((sine - abs(snext), sine + abs(snext))), enclose(
                (cosine - abs(cnext), cosine + abs(cnext))
            )
        sine, cosine = sine + snext, cosine + cnext
        s, c = snext, cnext
    raise ValueError("Normal transcendental work budget exceeded")


def small_trig(interval):
    if not -1 <= interval[0] <= interval[1] <= 1:
        raise ValueError("Subdivide the whole normal heading range first")
    low, high = interval
    sine = trig_point(low)[0][0], trig_point(high)[0][1]
    nearest = F(0) if low <= 0 <= high else min(abs(low), abs(high))
    furthest = max(abs(low), abs(high))
    cosine = trig_point(furthest)[1][0], trig_point(nearest)[1][1]
    return sine, cosine


def tangent(interval):
    result = []
    for x in interval:
        sine, cosine = trig_point(x)
        if cosine[0] <= 0:
            raise ValueError("Unqualified bank tangent pole")
        result.append(divide(sine, cosine))
    return result[0][0], result[1][1]
