"""Outward-rounded capsule-distance enclosures for conservative fork volume blocks."""

import numpy as np


def lower(value):
    return np.nextafter(value, -np.inf)


def upper(value):
    return np.nextafter(value, np.inf)


def sum_three(value, rounding):
    return rounding(rounding(value[:, 0] + value[:, 1]) + value[:, 2])


def distance_bounds(field, centres):
    local_low = lower(centres - field.origin)
    local_high = upper(centres - field.origin)
    low, high = [], []
    for endpoint in field.segment_data[0]:
        dot_low = sum_three(
            np.minimum(lower(local_low * endpoint), lower(local_high * endpoint)), lower
        )
        dot_high = sum_three(
            np.maximum(upper(local_low * endpoint), upper(local_high * endpoint)), upper
        )
        squares = np.broadcast_to(endpoint**2, centres.shape)
        denominator_low = float(sum_three(lower(squares[:1]), lower)[0])
        denominator_high = float(sum_three(upper(squares[:1]), upper)[0])
        if np.all(endpoint == 0):
            parameter_low, parameter_high = (
                np.zeros(len(centres)),
                np.zeros(len(centres)),
            )
        elif denominator_low <= 0:
            parameter_low, parameter_high = (
                np.zeros(len(centres)),
                np.ones(len(centres)),
            )
        else:
            parameter_low = np.clip(
                lower(
                    np.minimum(dot_low / denominator_low, dot_low / denominator_high)
                ),
                0,
                1,
            )
            parameter_high = np.clip(
                upper(
                    np.maximum(dot_high / denominator_low, dot_high / denominator_high)
                ),
                0,
                1,
            )
        product_low = np.minimum(
            lower(parameter_low[:, None] * endpoint),
            lower(parameter_high[:, None] * endpoint),
        )
        product_high = np.maximum(
            upper(parameter_low[:, None] * endpoint),
            upper(parameter_high[:, None] * endpoint),
        )
        delta_low, delta_high = (
            lower(local_low - product_high),
            upper(local_high - product_low),
        )
        crossed = (delta_low <= 0) & (delta_high >= 0)
        square_low = np.where(
            crossed,
            0,
            np.maximum(0, np.minimum(lower(delta_low**2), lower(delta_high**2))),
        )
        square_high = np.maximum(upper(delta_low**2), upper(delta_high**2))
        low.append(
            np.maximum(0, lower(np.sqrt(np.maximum(0, sum_three(square_low, lower)))))
        )
        high.append(upper(np.sqrt(np.maximum(0, sum_three(square_high, upper)))))
    return np.column_stack(low), np.column_stack(high)


def block_intervals(field, minima, maxima):
    minima, maxima = np.asarray(minima, dtype=float), np.asarray(maxima, dtype=float)
    if (
        minima.shape != maxima.shape
        or minima.ndim != 2
        or minima.shape[1] != 3
        or not np.isfinite(minima).all()
        or not np.isfinite(maxima).all()
        or np.any(minima > maxima)
    ):
        raise ValueError("finite ordered spatial boxes required")
    centres = minima + (maxima - minima) / 2
    reach = np.maximum(upper(np.abs(minima - centres)), upper(np.abs(maxima - centres)))
    radius = upper(np.sqrt(np.maximum(0, sum_three(upper(reach**2), upper))))
    distance_low, distance_high = distance_bounds(field, centres)
    distance_low = np.maximum(0, lower(distance_low - radius[:, None]))
    distance_high = upper(distance_high + radius[:, None])
    radii_squared = np.asarray(field.radii) ** 2
    scores_low = lower(lower(radii_squared) - upper(distance_high**2))
    scores_high = upper(upper(radii_squared) - np.maximum(0, lower(distance_low**2)))
    value_low = np.max(scores_low, axis=1)
    if len(field.radii) == 1:
        bonus = 0.0
    else:
        fraction = upper(1 - lower(1 / len(field.radii)))
        bonus = upper(upper(field.blend * fraction) / 2)
    value_high = upper(np.max(scores_high, axis=1) + bonus)
    magnitude = (
        np.maximum(
            np.max(np.abs(scores_low), axis=1), np.max(np.abs(scores_high), axis=1)
        )
        + field.blend
    )
    guard = upper(4096 * np.finfo(float).eps * len(field.radii) ** 2 * magnitude)
    return lower(value_low - guard), upper(value_high + guard)
