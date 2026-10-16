"""Own smooth asymmetric growth-space controls; no canopy surface is emitted."""

import numpy as np
from colonization import crown_points


def attractors(count, seed, kind="lobed"):
    if (
        type(count) is not int
        or not 32 <= count <= 8192
        or type(seed) is not int
        or not 0 <= seed < 2**64
    ):
        raise ValueError("bounded attraction count and unsigned seed required")
    points = crown_points(8.0, 5.0, 0.23, count, seed)
    if kind == "ellipsoid":
        return points
    if kind != "lobed":
        raise ValueError("declared authored growth-space distribution required")
    centre = np.array((0, 0, 8.0 * (1 + 0.23) / 2))
    radii = np.array((2.5, 2.5, 8.0 * (1 - 0.23) / 2))
    local = (points - centre) / radii
    direction = local / np.linalg.norm(local, axis=1)[:, None]
    rng = np.random.default_rng(np.random.SeedSequence((seed, 0x43524F57)))
    axes = rng.normal(size=(9, 3))
    axes /= np.linalg.norm(axes, axis=1)[:, None]
    widths = rng.uniform(0.22, 0.6, 9)
    amplitudes = rng.uniform(-0.45, 0.35, 9)
    weight = np.exp(-(1 - direction @ axes.T) / widths**2)
    radial = 0.95 + 0.3 * np.tanh((weight @ amplitudes) / 0.3)
    altered = local * radial[:, None]
    lower, upper = local.min(axis=0), local.max(axis=0)
    altered = lower + (altered - altered.min(axis=0)) / np.ptp(altered, axis=0) * (
        upper - lower
    )
    return centre + altered * radii
