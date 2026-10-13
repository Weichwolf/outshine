"""Independent kernel sampling and subdivision checks for physical vein relief."""

import numpy as np
from leaf_vein_field import segment_field, segment_heights
from scipy.optimize import minimize_scalar


def relief_checks():
    rng = np.random.default_rng(73)
    worst = 0.0
    for _ in range(80):
        a, b, point = rng.uniform(-0.01, 0.01, (3, 2))
        r0, r1 = rng.uniform(0.0001, 0.006, 2)

        def negative_height(t, a=a, b=b, point=point, r0=r0, r1=r1):
            centre = a + t * (b - a)
            radius = r0 + t * (r1 - r0)
            return -float(
                radius
                * max(1 - np.dot(point - centre, point - centre) / (9 * radius**2), 0)
                ** 3
            )

        grid = np.linspace(0, 1, 257)
        values = np.array([negative_height(t) for t in grid])
        best = int(np.argmin(values))
        fit = minimize_scalar(
            negative_height,
            bounds=(grid[max(0, best - 1)], grid[min(256, best + 1)]),
            method="bounded",
        )
        expected = -min(negative_height(0), negative_height(1), fit.fun)
        actual = segment_heights(point, a, b, r0, r1)
        worst = max(worst, abs(actual - expected))
        assert abs(actual - expected) < 1e-9
    a, b = np.array([0.0, 0]), np.array([0.02, 0])
    points = rng.uniform([-0.005, -0.005], [0.025, 0.005], (1000, 2))
    r0, r1 = 0.001, 0.0002
    whole = segment_heights(points, a, b, r0, r1)
    fractions = np.linspace(0, 1, 13)
    positions = a + fractions[:, None] * (b - a)
    radii = r0 + fractions * (r1 - r0)
    split, _ = segment_field(
        positions[:-1], positions[1:], radii[:-1], radii[1:], points, field="height_m"
    )
    assert np.allclose(whole, split, rtol=1e-12, atol=1e-15)
    for radius in (0.001, 0.0002):
        assert (
            abs(segment_heights(np.array([0.01, 0]), a, b, radius, radius) - radius)
            < 1e-15
        )
    for scale in (1e-4, 10000):
        value = segment_heights(
            points * scale, a * scale, b * scale, r0 * scale, r1 * scale
        )
        assert np.allclose(value / scale, whole, atol=1e-15)
    return {
        "independent_optimizer_cases": 80,
        "max_error_m": worst,
        "subdivision_points": 1000,
        "radius_height_and_scale": True,
    }
