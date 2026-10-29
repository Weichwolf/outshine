"""Whole-position-box bounds conditional on the existing #990 expression family.

Packed model/view/shift operands and normal-or-zero vertex storage are required.
The returned intervals qualify neither a compiler/producer nor GPU rasterization.
"""

import math
from fractions import Fraction

import numpy as np
from capture_source_support import checked_bounds
from vertex_projection.verify_envelope import family_limits

TINY = float(np.finfo(np.float32).tiny)


def directed(value, up):
    return np.where(value == 0, value, np.nextafter(value, np.inf if up else -np.inf))


def ceiling(value):
    rounded = float(value)
    return math.nextafter(rounded, math.inf) if Fraction(rounded) < value else rounded


def carried(value, shape):
    if (
        not isinstance(value, np.ndarray)
        or value.dtype != np.float32
        or value.shape != shape
    ):
        raise ValueError("Exact packed binary32 operands with matching shape required")
    if not np.isfinite(value).all() or np.any((value != 0) & (abs(value) < TINY)):
        raise ValueError("The existing family requires finite normal-or-zero operands")
    return value.astype(np.float64)


def coefficients(model, view, shift):
    """Ideal affine coefficients plus absolute original-monomial coefficients."""
    lower, upper, mass = [np.zeros((4, 4), float) for _ in range(3)]
    for j in range(4):
        # Products of two supplied binary32 numbers are exact binary64.
        product = view[:, j, None] * model[j, None, :]
        lower = directed(lower + product, False)
        upper = directed(upper + product, True)
        mass = directed(mass + abs(product), True)
    for j in range(3):
        product = view[:, j] * shift[j]
        lower[:, 3] = directed(lower[:, 3] + product, False)
        upper[:, 3] = directed(upper[:, 3] + product, True)
        mass[:, 3] = directed(mass[:, 3] + abs(product), True)
    return lower, upper, mass


def ideal_boxes(low, high, coefficient_low, coefficient_high):
    lower, upper = np.zeros((len(low), 4)), np.zeros((len(low), 4))
    for row in range(4):
        for column in range(4):
            a, b = coefficient_low[row, column], coefficient_high[row, column]
            candidates = [
                a * low[:, column],
                a * high[:, column],
                b * low[:, column],
                b * high[:, column],
            ]
            term_low, term_high = (
                np.minimum.reduce(candidates),
                np.maximum.reduce(candidates),
            )
            lower[:, row] = directed(lower[:, row] + directed(term_low, False), False)
            upper[:, row] = directed(upper[:, row] + directed(term_high, True), True)
    return lower, upper


def box_envelope(
    lower, upper, model, view, shift, *, positions_normal_or_zero, bounds_most=4096
):
    lower, upper = checked_bounds(lower, upper, bounds_most)
    if positions_normal_or_zero is not True:
        raise ValueError(
            "Normal-or-zero position storage must be established by its producer"
        )
    model, view, shift = (
        carried(model, (4, 4)),
        carried(view, (4, 4)),
        carried(shift, (3,)),
    )
    for endpoints in (lower, upper):
        with np.errstate(over="ignore", invalid="ignore"):
            stored = endpoints.astype(np.float32).astype(np.float64)
        if not np.array_equal(endpoints, stored) or np.any(
            (endpoints != 0) & (abs(endpoints) < TINY)
        ):
            raise ValueError("Stored binary32 normal-or-zero position bounds required")
    maximum = max(
        1.0, *(float(abs(a).max(initial=0)) for a in (lower, upper, model, view, shift))
    )
    gamma, growth, noise = family_limits(Fraction(maximum))
    if 20 * Fraction(maximum) ** 3 * growth + noise >= Fraction(
        float(np.finfo(np.float32).max)
    ):
        raise ValueError(
            "Existing expression-family intermediate overflow bound exceeded"
        )
    p_low = np.column_stack((lower, np.ones(len(lower))))
    p_high = np.column_stack((upper, np.ones(len(upper))))
    c_low, c_high, mass_coefficients = coefficients(model, view, shift)
    ideal_low, ideal_high = ideal_boxes(p_low, p_high, c_low, c_high)
    point_magnitude = np.maximum(abs(p_low), abs(p_high))
    mass = np.zeros((len(lower), 4))
    for row in range(4):
        for column in range(4):
            term = directed(
                mass_coefficients[row, column] * point_magnitude[:, column], True
            )
            mass[:, row] = directed(mass[:, row] + term, True)
    error = directed(directed(ceiling(gamma) * mass, True) + ceiling(noise), True)
    clip_low = directed(ideal_low - error, False)
    clip_high = directed(ideal_high + error, True)
    if not np.isfinite(clip_low).all() or not np.isfinite(clip_high).all():
        raise ValueError("Binary64 box envelope overflow")
    return {
        "lower": clip_low,
        "upper": clip_high,
        "ideal_lower": ideal_low,
        "ideal_upper": ideal_high,
        "error_upper": error,
        "positions_normal_or_zero_assumed": True,
        "producer_expression_family_qualified": False,
        "raster_qualified": False,
    }


def conditionally_inside(envelope):
    low, high = envelope["lower"], envelope["upper"]
    w = low[:, 3]
    return (
        (w > 0)
        & (low[:, 0] >= -w)
        & (high[:, 0] <= w)
        & (low[:, 1] >= -w)
        & (high[:, 1] <= w)
        & (low[:, 2] >= 0)
        & (high[:, 2] <= w)
    )
