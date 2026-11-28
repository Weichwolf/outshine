"""Metric smooth material masks averaged over a one-dimensional pixel footprint."""

import numpy as np


def filtered_edge_weight(distance_m, reach_m, half_footprint_m):
    """Return float32 coverage toward the positive side of a signed boundary.

    The unfiltered mask is smoothstep(-reach, reach, distance). The footprint
    is a uniform interval along the boundary normal. This is not an exact
    integral over a curved boundary or an arbitrarily oriented square pixel.
    Inputs broadcast, are finite and float32-representable, and use metres; reach and footprint are
    nonnegative. No geometry, class, material colour, or caller storage changes.
    """
    with np.errstate(over="ignore"):
        distance, reach, half = np.broadcast_arrays(
            *[
                np.asarray(value, dtype=np.float32)
                for value in (distance_m, reach_m, half_footprint_m)
            ]
        )
    if not all(np.isfinite(value).all() for value in (distance, reach, half)):
        raise ValueError("material boundary inputs must be finite")
    if np.any(reach < 0) or np.any(half < 0):
        raise ValueError("material reach and half footprint must be nonnegative")
    result = np.where(distance > 0, 1.0, np.where(distance < 0, 0.0, 0.5)).astype(
        np.float32
    )
    hard = (reach == 0) & (half > 0)
    result[hard] = np.clip(0.5 + 0.5 * distance[hard] / half[hard], 0.0, 1.0)
    inside = (reach > 0) & (np.abs(distance) + half <= reach)
    u = 0.5 - 0.5 * np.abs(distance[inside]) / reach[inside]
    radius = 0.5 * half[inside] / reach[inside]
    negative = u * u * (3.0 - 2.0 * u) + radius * radius * (1.0 - 2.0 * u)
    result[inside] = np.where(distance[inside] > 0, 1.0 - negative, negative)
    crossing = (reach > 0) & (half > 0) & ~inside & (np.abs(distance) < reach + half)
    edge = half[crossing] - np.abs(distance[crossing])
    width = reach[crossing]
    u = np.clip(0.5 + 0.5 * edge / width, 0.0, 1.0)
    area = np.where(edge >= width, edge, 2.0 * width * u * u * u * (1.0 - 0.5 * u))
    negative = area / (2.0 * half[crossing])
    result[crossing] = np.where(distance[crossing] > 0, 1.0 - negative, negative)
    return result
