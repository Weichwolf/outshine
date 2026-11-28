"""Own seeded continuous pigment patches in physical leaf coordinates."""

import numpy as np

from .material_arrays import real_array, rows


def patch_field(points, seed):
    points = rows(points, 2)
    if type(seed) is not int or not 0 <= seed < 2**64:
        raise ValueError("UInt64 pigment seed required")
    rng = np.random.default_rng(seed)
    directions = rng.normal(size=(12, 2))
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    wavelengths = rng.uniform(0.008, 0.04, 12)
    phase = rng.uniform(0, 2 * np.pi, 12)
    value = np.zeros(len(points))
    for direction, wavelength, angle in zip(directions, wavelengths, phase):
        value += np.sin(
            2 * np.pi * np.sum(points * direction, axis=1) / wavelength + angle
        )
    return np.tanh(value / np.sqrt(6))


def pigment_coordinates(points, ridge, seed, chlorophyll, anthocyanin):
    points, ridge = rows(points, 2), real_array(ridge)
    state = real_array((chlorophyll, anthocyanin))
    if (
        ridge.shape != (len(points),)
        or np.any((ridge < 0) | (ridge > 1))
        or state.shape != (2,)
        or np.any(state < 0)
        or type(seed) is not int
        or not 0 <= seed < 2**64 - 1
    ):
        raise ValueError(
            "matching unit ridge, nonnegative pigment state and UInt64 seed pair required"
        )
    patches = patch_field(points, seed)
    chl = chlorophyll * (1 + 0.3 * patches) * (1 - 0.25 * ridge)
    ant = anthocyanin * (1 + 0.15 * patch_field(points, seed + 1))
    result = np.column_stack((chl, ant))
    if not np.isfinite(result).all():
        raise ValueError("finite pigment coordinates required")
    return result
