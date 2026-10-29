"""View-independent raw rock fields; current view-dependent GLSL response separate."""

import numpy as np
from ground_gradients import noise_gradient
from rock_direction import BEDDING, FRACTURES

MATRICES = (np.eye(3), BEDDING, FRACTURES)


def raw_noise(world_m, matrix, wavelength_m, *, noise_query=noise_gradient):
    """Ideal Double existing noise value/world gradient, not GPU word identity."""
    world = np.asarray(world_m, dtype=np.float64)
    matrix = np.asarray(matrix, dtype=np.float64)
    if (
        world.ndim < 2
        or world.shape[-1] != 3
        or world.size > 3 * 262144
        or matrix.shape != (3, 3)
        or not np.isfinite(world).all()
        or not np.isfinite(matrix).all()
        or not np.isfinite(wavelength_m)
        or wavelength_m <= 0
    ):
        raise ValueError(
            "bounded finite metric points, transform and wavelength required"
        )
    transform = matrix / wavelength_m
    value, gradient = noise_query(world @ transform.T)
    return np.concatenate(((value - 0.5)[..., None], gradient @ transform), axis=-1)


def rock_fields(world_m, coarse_m, fine_m, *, noise_queries=None):
    queries = (noise_gradient,) * 3 if noise_queries is None else noise_queries
    if len(queries) != 3:
        raise ValueError("one explicit noise query per original rock octave required")
    return tuple(
        raw_noise(world_m, matrix, wavelength, noise_query=query)
        for matrix, wavelength, query in zip(
            MATRICES, (12 * coarse_m, coarse_m, fine_m), queries, strict=True
        )
    )


def visibility(square_size):
    t = np.clip((np.sqrt(square_size) - 0.25) / 0.55, 0.0, 1.0)
    return 1.0 - t * t * (3.0 - 2.0 * t)


def rock_response(fields, world_dx, world_dy, *, coarse_m, fine_m, height_m, contrast):
    """Current groundRock/rockCrevice algebra after the original raw noise calls.

    No new NDF approximation: footprint fades and the crevice variance are kept
    at query time. Mean gradients or Beckmann moments are not equivalent GGX.
    """
    if len(fields) != 3:
        raise ValueError("the three bound rock octave fields are required")
    dx, dy = (np.asarray(v, dtype=np.float64) for v in (world_dx, world_dy))
    shape = np.shape(fields[0])[:-1]
    if (
        not shape
        or np.prod(shape) > 262144
        or any(np.shape(f) != (*shape, 4) or not np.isfinite(f).all() for f in fields)
        or dx.shape != (*shape, 3)
        or dy.shape != dx.shape
        or not np.isfinite(dx).all()
        or not np.isfinite(dy).all()
        or not all(np.isfinite(v) for v in (coarse_m, fine_m, height_m, contrast))
        or min(coarse_m, fine_m) <= 0
        or height_m < 0
        or not 0 <= contrast <= 1
    ):
        raise ValueError(
            "matching bounded fields, metric derivatives and recipe required"
        )
    filtered = []
    for index, (field, matrix, wavelength) in enumerate(
        zip(fields, MATRICES, (12 * coarse_m, coarse_m, fine_m), strict=True)
    ):
        tx, ty = (v @ matrix.T / wavelength for v in (dx, dy))
        weight = visibility(
            np.maximum(np.sum(tx * tx, axis=-1), np.sum(ty * ty, axis=-1))
        )
        phase = np.stack(
            (
                np.sum(field[..., 1:] * dx, axis=-1),
                np.sum(field[..., 1:] * dy, axis=-1),
            ),
            axis=-1,
        )
        if index < 2:
            square = 0.07**2 + np.sum(phase * phase, axis=-1) / 12
            pulse = (
                0.07 / np.sqrt(square) * np.exp(-(field[..., 0] ** 2) / (2 * square))
            )
            phase *= (pulse * field[..., 0] / square)[..., None]
        filtered.append(
            np.concatenate((phase, field[..., :1]), axis=-1) * weight[..., None]
        )
    macro, coarse, fine = filtered
    albedo_scale = np.clip(
        1
        + contrast * (0.4 * macro[..., 2] + 0.5 * coarse[..., 2] + 0.1 * fine[..., 2]),
        0.72,
        1.28,
    )
    roughness_offset = 0.14 * macro[..., 2] + 0.06 * coarse[..., 2]
    derivatives = (
        min(12 * coarse_m * 0.025, 3) * macro[..., :2]
        + min(coarse_m * 0.04, 0.4) * coarse[..., :2]
        + height_m * fine[..., :2]
    )
    return np.concatenate(
        (albedo_scale[..., None], roughness_offset[..., None], derivatives), axis=-1
    )
