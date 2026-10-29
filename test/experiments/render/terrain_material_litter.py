"""Current oriented nine-point litter filter and unchanged substrate composition."""

import numpy as np
from terrain_material_plane import checked_at, hash_words, noise
from terrain_material_substrate import detail


def footprint_field(at, dx, dy, scale_m, *, words=hash_words):
    at = checked_at(at)
    dx, dy = np.asarray(dx, dtype=float), np.asarray(dy, dtype=float)
    if (
        dx.shape != at.shape
        or dy.shape != at.shape
        or not np.isfinite(dx).all()
        or not np.isfinite(dy).all()
        or not np.isfinite(scale_m)
        or scale_m <= 0
    ):
        raise ValueError("matching finite oriented footprint and wavelength required")
    footprint = (
        np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1)) / scale_m
    )
    result = np.zeros(len(at))
    small = footprint <= 0.01
    if np.any(small):
        result[small] = noise(at[small] / scale_m, words=words)[0] - 0.5
    active = (footprint > 0.01) & (footprint < 2)
    if np.any(active):
        total = np.zeros(np.count_nonzero(active))
        positions, horizontal, vertical = (
            at[active] / scale_m,
            dx[active] / scale_m,
            dy[active] / scale_m,
        )
        for y, wy in zip(
            (-0.3872983346207417, 0, 0.3872983346207417),
            np.array([5, 8, 5]) / 18,
            strict=True,
        ):
            for x, wx in zip(
                (-0.3872983346207417, 0, 0.3872983346207417),
                np.array([5, 8, 5]) / 18,
                strict=True,
            ):
                total += (
                    wx
                    * wy
                    * noise(positions + x * horizontal + y * vertical, words=words)[0]
                )
        t = np.clip(footprint[active] - 1, 0, 1)
        result[active] = (total - 0.5) * (1 - t * t * (3 - 2 * t))
    return result


def wearing(mean, response):
    return np.column_stack(
        (mean[:3] * response[:, :1], mean[3] + response[:, 1], response[:, 2:])
    )


def compose(
    at,
    dx,
    dy,
    mean,
    recipe,
    contrast,
    litter_recipe,
    difference,
    coverage,
    *,
    base_words=None,
    litter_words=None,
    mask_words=hash_words,
):
    """Current GroundSurface before later rock/class blending, clamp and lighting."""
    mean, difference = (
        np.asarray(mean, dtype=float),
        np.asarray(difference, dtype=float),
    )
    if (
        mean.shape != (4,)
        or difference.shape != (4,)
        or not np.isfinite(mean).all()
        or not np.isfinite(difference).all()
        or not np.isfinite(coverage)
        or not 0 <= coverage <= 1
    ):
        raise ValueError(
            "finite captured optical words/difference and coverage required"
        )
    if coverage < 1:
        base = detail(at, dx, dy, recipe, contrast, word_sources=base_words)
        if coverage == 0:
            return wearing(mean, base)
    litter = detail(at, dx, dy, litter_recipe, contrast, word_sources=litter_words)
    if coverage == 1:
        return wearing(mean, litter)
    field = (
        footprint_field(at, dx, dy, recipe[2], words=mask_words)
        if recipe[2] > 0
        else np.zeros(len(at))
    )
    weight = np.clip(coverage + 2 * min(coverage, 1 - coverage) * field, 0, 1)
    first = mean - coverage * difference
    return (
        wearing(first, base) * (1 - weight[:, None])
        + wearing(first + difference, litter) * weight[:, None]
    )
