"""Independent physical planes, tangent metrics, and direct moment statistics."""

import numpy as np
from blade_surface import Blade
from leaf_normal_moments import (
    mip_pyramid,
    resolve_moments,
    slope_moments,
    tangent_slopes,
)


def direct_pyramid(base):
    size = len(base)
    return [
        base.reshape(size // block, block, size // block, block, 6).mean((1, 3))
        for block in (2**i for i in range(size.bit_length()))
    ]


def normal_checks():
    rng = np.random.default_rng(482)
    u, v = rng.uniform(0.05, 0.95, (2, 128))
    errors = []
    for bend, fold, twist in ((0, 0, 0), (0.32, 0.13, 0.1), (-0.6, 0.3, -0.4)):
        blade = Blade(bend=bend, fold=fold, twist=twist)
        _, n, _, du, dv = blade.surface_jet(v, 2 * u - 1, np.where(u < 0.5, -1, 1))
        hu, hv = rng.uniform(-0.002, 0.002, (2, len(u)))
        slopes = tangent_slopes(du, dv, hu, hv)
        t = du / np.linalg.norm(du, axis=1)[:, None]
        b = np.cross(n, t)
        predicted = n + slopes[:, :1] * t + slopes[:, 1:] * b
        predicted /= np.linalg.norm(predicted, axis=1)[:, None]
        actual = np.cross(du + hu[:, None] * n, dv + hv[:, None] * n)
        actual /= np.linalg.norm(actual, axis=1)[:, None]
        errors.append(float(np.max(abs(actual - predicted))))
        assert np.max(abs(actual - predicted)) < 1e-13
    planar = Blade(bend=0, fold=0, twist=0)
    p, _, _, du, dv = planar.surface_jet(v, 2 * u - 1, np.where(u < 0.5, -1, 1))
    gradient = np.array([0.13, -0.07, 0])
    slopes = tangent_slopes(du, dv, du @ gradient, dv @ gradient)
    expected = np.array([-0.13, 0.07, 1])
    expected /= np.linalg.norm(expected)
    normals = np.column_stack((slopes, np.ones(len(p))))
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    assert np.max(abs(normals - expected)) < 1e-13
    slopes = rng.normal(0, 0.12, (64, 64, 2))
    roughness = rng.uniform(0.08, 0.6, (64, 64))
    base = slope_moments(slopes, roughness)
    pyramid, direct = mip_pyramid(base), direct_pyramid(base)
    maximum = max(float(np.max(abs(a - b))) for a, b in zip(pyramid, direct))
    assert maximum < 1e-13
    for level, moments in enumerate(pyramid):
        block = 2**level
        normal, rough, cov = resolve_moments(moments)
        for row, col in np.ndindex(moments.shape[:2]):
            values = slopes[
                row * block : (row + 1) * block, col * block : (col + 1) * block
            ].reshape(-1, 2)
            reference = (
                np.cov(values, rowvar=False, bias=True)
                if block > 1
                else np.zeros((2, 2))
            )
            assert np.max(abs(cov[row, col] - reference[(0, 0, 1), (0, 1, 1)])) < 1e-13
        assert np.allclose(np.linalg.norm(normal, axis=2), 1, atol=1e-14, rtol=0)
        assert np.all(rough >= resolve_moments(moments, broaden=False)[1] - 1e-14)
    return {
        "metric_probe_count": 384,
        "metric_max_error": max(errors),
        "planar_height_gradient": True,
        "direct_moment_max_error": maximum,
        "independent_covariance_blocks": sum(len(l) ** 2 for l in pyramid),
    }
