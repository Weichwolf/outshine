"""Seeded, continuous pigment patches in physical leaf coordinates."""

import numpy as np


def patch_field(points, seed):
    points = np.asarray(points, float)
    if (
        points.ndim != 2
        or points.shape[1] != 2
        or len(points) > 262144
        or not np.isfinite(points).all()
    ):
        raise ValueError("bounded finite physical leaf points required")
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
    patches = patch_field(points, seed)
    chl = chlorophyll * (1 + 0.3 * patches) * (1 - 0.25 * ridge)
    ant = anthocyanin * (1 + 0.15 * patch_field(points, seed + 1))
    return np.column_stack((chl, ant))
