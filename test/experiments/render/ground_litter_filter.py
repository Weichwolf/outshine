"""A bounded separable box filter for the existing cubic value-noise field."""

import numpy as np

from rock_direction import value


def axis_weights(centre, width):
    half = width * 0.5
    first = np.floor(centre - half).astype(np.int64)
    weights = np.zeros(centre.shape + (4,))
    if width <= 0.01:
        first = np.floor(centre).astype(np.int64)
        fraction = centre - first
        smooth = fraction * fraction * (3.0 - 2.0 * fraction)
        weights[..., 0] = 1.0 - smooth
        weights[..., 1] = smooth
        return first, weights
    for cell in range(3):
        low = np.clip(centre - half - first - cell, 0.0, 1.0)
        high = np.clip(centre + half - first - cell, 0.0, 1.0)
        primitive_low = low ** 3 - 0.5 * low ** 4
        primitive_high = high ** 3 - 0.5 * high ** 4
        upper = (primitive_high - primitive_low) / width
        weights[..., cell] += (high - low) / width - upper
        weights[..., cell + 1] += upper
    assert np.max(np.abs(weights.sum(axis=-1) - 1.0)) < 1e-12
    assert weights.min() > -1e-12
    return first, weights


def field(at, footprint, scale):
    return rectangular_field(at, np.array([footprint, footprint]), scale)


def rectangular_field(at, footprint, scale):
    width = np.asarray(footprint) / scale
    largest = np.max(width)
    if largest >= 2.0:
        return np.zeros(at.shape[:-1])
    first_x, weights_x = axis_weights(at[..., 0] / scale, width[0])
    first_y, weights_y = axis_weights(at[..., 1] / scale, width[1])
    total = np.zeros(at.shape[:-1])
    for y in range(4):
        for x in range(4):
            weight = weights_x[..., x] * weights_y[..., y]
            if np.any(weight):
                key = np.stack([first_x + x, first_y + y, np.zeros_like(first_x)], axis=-1)
                total += weight * value(key)
    transition = np.clip(largest - 1.0, 0.0, 1.0)
    visibility = 1.0 - transition * transition * (3.0 - 2.0 * transition)
    return (total - 0.5) * visibility
