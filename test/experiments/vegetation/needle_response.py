"""Bounded diffuse normal-distribution study; no visibility or transport model."""

import numpy as np


def source_samples(normals, weights):
    normals, weights = np.asarray(normals, float), np.asarray(weights, float)
    if (
        normals.ndim != 2
        or normals.shape[1] != 3
        or weights.shape != (len(normals),)
        or len(normals) > 1_048_576
        or not np.isfinite(normals).all()
        or not np.isfinite(weights).all()
        or np.any(weights < 0)
        or np.any(abs(np.linalg.norm(normals, axis=1) - 1) > 1e-10)
    ):
        raise ValueError("bounded unit source normals and nonnegative weights required")
    return normals, weights


def light_direction(direction):
    direction = np.asarray(direction, float)
    if (
        direction.shape != (3,)
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-10
    ):
        raise ValueError("finite unit light direction required")
    return direction


def exact_response(normals, weights, direction):
    """Positive cosine before averaging; weights already include camera support."""
    normals, weights = source_samples(normals, weights)
    return float(weights @ np.maximum(normals @ light_direction(direction), 0))


def mean_response(normals, weights, direction):
    normals, weights = source_samples(normals, weights)
    direction = light_direction(direction)
    mean = weights @ normals
    length = np.linalg.norm(mean)
    return (
        float(weights.sum() * max(mean @ direction, 0) / length) if length > 0 else 0.0
    )


def quadratic_moments(normals, weights):
    """Nine scalars for degree0..2; unit-normal trace removes a tenth scalar."""
    normals, weights = source_samples(normals, weights)
    x, y, z = normals.T
    return np.r_[
        weights.sum(),
        weights @ normals,
        weights @ (x * y),
        weights @ (y * z),
        weights @ (z * x),
        weights @ (x * x - y * y),
        weights @ (3 * z * z - 1),
    ]


def quadratic_response(moments, direction):
    """Degree2 Legendre projection of max(cosine,0), without output clamping."""
    moments = np.asarray(moments, float)
    if moments.shape != (9,) or not np.isfinite(moments).all():
        raise ValueError("nine finite degree0..2 moments required")
    x, y, z = light_direction(direction)
    mass, xy, yz, zx, delta, axial = moments[[0, 4, 5, 6, 7, 8]]
    zz = (axial + mass) / 3
    xx = (mass - zz + delta) / 2
    yy = (mass - zz - delta) / 2
    squared = (
        x * x * xx
        + y * y * yy
        + z * z * zz
        + 2 * (x * y * xy + y * z * yz + z * x * zx)
    )
    return float(3 / 32 * mass + 0.5 * (moments[1:4] @ [x, y, z]) + 15 / 32 * squared)
