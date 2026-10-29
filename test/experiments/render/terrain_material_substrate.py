"""Current substrate recipe with optional bounded original-word lookup sources."""

import numpy as np
from terrain_material_plane import checked_at, hash_words, noise


def fade(footprint, wavelength):
    t = np.clip((footprint / wavelength - 0.25) / 0.55, 0, 1)
    return 1 - t * t * (3 - 2 * t)


def filtered(at, footprint, wavelength, words):
    scalar, gradient = np.zeros(len(at)), np.zeros_like(at)
    active = footprint < wavelength * 0.8
    if np.any(active):
        value, derivative = noise(at[active] / wavelength, words=words)
        weight = fade(footprint[active], wavelength)
        scalar[active] = (value - 0.5) * weight
        gradient[active] = derivative * (weight / wavelength)[:, None]
    return scalar, gradient


def grains(at, footprint, grain_m, height_m, words):
    tint, gradient = np.ones(len(at)), np.zeros_like(at)
    active = footprint < grain_m * 0.8
    if grain_m <= 0 or height_m <= 0 or not np.any(active):
        return tint, gradient
    points = checked_at(at[active] / grain_m)
    cell = np.floor(points).astype(np.int64)
    fraction = points - cell
    nearest, offset, colour = (
        np.full(len(points), np.inf),
        np.zeros_like(points),
        np.zeros(len(points)),
    )
    for y in (-1, 0, 1):
        for x in (-1, 0, 1):
            shift = np.array([x, y])
            bits = words(cell + shift)
            jitter = 0.25 + 0.5 * np.column_stack((bits & 255, (bits >> 8) & 255)) / 255
            delta = shift + jitter - fraction
            distance = np.sum(delta * delta, axis=-1)
            nearer = distance < nearest
            offset = np.where(nearer[:, None], delta, offset)
            colour = np.where(nearer, ((bits >> 16) & 255) / 255 - 0.5, colour)
            nearest = np.minimum(nearest, distance)
    radius, weight = np.sqrt(nearest), fade(footprint[active], grain_m)
    relief = np.maximum(1 - radius, 0)
    slope = np.divide(
        2 * height_m * weight * relief,
        grain_m * radius,
        out=np.zeros_like(radius),
        where=radius > 0,
    )
    tint[active] = 1 + 0.35 * weight * colour
    gradient[active] = offset * slope[:, None]
    return tint, gradient


def detail(at, dx, dy, recipe, contrast, *, word_sources=None):
    """Same four output components as substrateDetail, no class/litter composition."""
    at, dx, dy = (
        checked_at(at),
        np.asarray(dx, dtype=float),
        np.asarray(dy, dtype=float),
    )
    if (
        dx.shape != at.shape
        or dy.shape != at.shape
        or not np.isfinite(dx).all()
        or not np.isfinite(dy).all()
        or len(recipe) != 4
        or not np.isfinite(recipe).all()
        or min(recipe) < 0
        or not np.isfinite(contrast)
        or not 0 <= contrast <= 1
    ):
        raise ValueError(
            "matching bounded metric derivatives and substrate recipe required"
        )
    words = (hash_words,) * 3 if word_sources is None else tuple(word_sources)
    if len(words) != 3 or not all(callable(w) for w in words):
        raise ValueError("explicit coarse/fine/grain word sources required")
    grain, height, coarse, fine = map(float, recipe)
    result = np.zeros((len(at), 4))
    result[:, 0] = 1
    footprint = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    active = footprint < max(coarse, fine, grain) * 0.8
    if coarse <= 0 or fine <= 0 or not np.any(active):
        return result
    positions, sizes = at[active], footprint[active]
    c, _ = filtered(positions, sizes, coarse, words[0])
    f, fine_gradient = filtered(positions, sizes, fine, words[1])
    tint, grain_gradient = grains(positions, sizes, grain, height, words[2])
    gradient = height * fine_gradient + grain_gradient
    result[active, 0] = (1 + contrast * (0.7 * c + 0.3 * f)) * tint
    result[active, 1] = 0.06 * c + 0.04 * f
    result[active, 2:] = np.column_stack(
        (np.sum(gradient * dx[active], axis=-1), np.sum(gradient * dy[active], axis=-1))
    )
    return result
