"""Bounded original 24-bit noise lattice, retaining the current cubic response."""

from dataclasses import dataclass

import numpy as np
from rock_direction import value
from terrain_material_grid import frozen


def checked_points(at):
    points = np.asarray(at, dtype=np.float64)
    if (
        points.ndim < 2
        or points.shape[-1] != 3
        or points.size > 3 * 262144
        or not np.isfinite(points).all()
        or np.any(points < -(2**31))
        or np.any(points >= 2**31 - 1)
    ):
        raise ValueError("bounded finite native-lattice points required")
    return points


@dataclass(frozen=True, slots=True)
class NoiseLattice:
    first_xyz: tuple[int, int, int]
    words_zyx: np.ndarray

    @property
    def retained_bytes(self):
        return self.words_zyx.nbytes

    def corners(self, at):
        points = checked_points(at)
        cell = np.floor(points).astype(np.int64)
        local = cell - self.first_xyz
        if np.any(local < 0) or np.any(local + 1 >= self.words_zyx.shape[::-1]):
            raise ValueError("query outside the explicitly prepared original lattice")
        x, y, z = np.moveaxis(local, -1, 0)
        taps = tuple(
            self.words_zyx[z + dz, y + dy, x + dx].astype(np.float64) / 16777215.0
            for dz in (0, 1)
            for dy in (0, 1)
            for dx in (0, 1)
        )
        return points - cell, taps


def prepare(normalized_bounds, *, word_limit=262144):
    """Prepare a finite original lattice box; query bounds include both endpoints."""
    bounds = np.asarray(normalized_bounds, dtype=np.float64)
    if (
        bounds.shape != (2, 3)
        or not np.isfinite(bounds).all()
        or np.any(bounds[0] > bounds[1])
        or np.any(bounds < -(2**31))
        or np.any(bounds >= 2**31 - 1)
        or type(word_limit) is not int
        or not 1 <= word_limit <= 262144
    ):
        raise ValueError("bounded ordered normalized native-lattice box required")
    first, last = np.floor(bounds).astype(np.int64)
    size = last - first + 2
    if np.prod(size, dtype=object) > word_limit:
        raise ValueError("original lattice word budget")
    z, y, x = np.indices(tuple(size[::-1]), dtype=np.int64)
    cells = np.stack((x, y, z), axis=-1) + first
    words = np.rint(value(cells) * 16777215.0).astype(np.uint32)
    return NoiseLattice(tuple(int(v) for v in first), frozen(words))


def evaluate(lattice, at):
    """Current surfaceNoise3 interpolation/gradient from original integer values.

    Double reference arithmetic only. Integer lattice words are exact; different
    arithmetic ordering is not a claim of GPU bit identity or hardware sampling.
    """
    return interpolate(*lattice.corners(at))


def analytic(at):
    """Same current cubic evaluation with original analytic corner hashes."""
    points = checked_points(at)
    cell = np.floor(points).astype(np.int64)
    taps = tuple(
        value(cell + [dx, dy, dz]) for dz in (0, 1) for dy in (0, 1) for dx in (0, 1)
    )
    return interpolate(points - cell, taps)


def interpolate(fraction, taps):
    """One unchanged cubic reconstruction shared by the equal-quality alternatives."""
    slope = 6 * fraction * (1 - fraction)
    f = fraction * fraction * (3 - 2 * fraction)
    a0, a1, b0, b1, c0, c1, d0, d1 = taps

    def mix(a, b, t):
        return a * (1 - t) + b * t

    a, b, c, d = (mix(taps[i], taps[i + 1], f[..., 0]) for i in (0, 2, 4, 6))
    low, high = mix(a, b, f[..., 1]), mix(c, d, f[..., 1])
    gradient = np.stack(
        (
            mix(
                mix(a1 - a0, b1 - b0, f[..., 1]),
                mix(c1 - c0, d1 - d0, f[..., 1]),
                f[..., 2],
            )
            * slope[..., 0],
            mix(b - a, d - c, f[..., 2]) * slope[..., 1],
            (high - low) * slope[..., 2],
        ),
        axis=-1,
    )
    return mix(low, high, f[..., 2]), gradient
