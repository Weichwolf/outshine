"""Physical normals and filtered slopes checked against independent geometry/statistics."""

import unittest

import numpy as np
from outshine_ref.vegetation.leaf_normals import (
    mip_pyramid,
    resolve_moments,
    slope_moments,
    tangent_slopes,
)


class LeafNormalTests(unittest.TestCase):
    def test_oblique_metric_against_displaced_surface_cross_product(self):
        rng = np.random.default_rng(483)
        du, dv = rng.normal(size=(2, 128, 3))
        hu, hv = rng.uniform(-0.02, 0.02, (2, 128))
        normal = np.cross(du, dv)
        normal /= np.linalg.norm(normal, axis=1)[:, None]
        tangent = du / np.linalg.norm(du, axis=1)[:, None]
        bitangent = np.cross(normal, tangent)
        slopes = tangent_slopes(du, dv, hu, hv)
        expected = np.cross(du + hu[:, None] * normal, dv + hv[:, None] * normal)
        expected /= np.linalg.norm(expected, axis=1)[:, None]
        actual = normal + slopes[:, :1] * tangent + slopes[:, 1:] * bitangent
        actual /= np.linalg.norm(actual, axis=1)[:, None]
        np.testing.assert_allclose(actual, expected, rtol=0, atol=3e-15)

    def test_mips_against_raw_block_statistics_and_owned_base(self):
        rng = np.random.default_rng(44)
        slopes = rng.normal(0, 0.3, (16, 16, 2))
        roughness = rng.uniform(0.1, 0.7, (16, 16))
        moments = slope_moments(slopes, roughness)
        levels = mip_pyramid(moments)
        for i, level in enumerate(levels):
            block = 2**i
            normal, rough, covariance = resolve_moments(level)
            for row, col in np.ndindex(level.shape[:2]):
                subset = slopes[
                    row * block : (row + 1) * block, col * block : (col + 1) * block
                ].reshape(-1, 2)
                centred = subset - subset.mean(axis=0)
                cov = centred.T @ centred / len(subset)
                np.testing.assert_allclose(
                    covariance[row, col], cov[(0, 0, 1), (0, 1, 1)], atol=2e-16
                )
                r = roughness[
                    row * block : (row + 1) * block, col * block : (col + 1) * block
                ]
                self.assertAlmostEqual(
                    rough[row, col],
                    min((np.mean(r**4) + np.trace(cov)) ** 0.25, 1),
                    places=14,
                )
            np.testing.assert_allclose(np.linalg.norm(normal, axis=2), 1, atol=2e-16)
        moments[:] = 0
        self.assertGreater(np.max(abs(levels[0])), 0)

    def test_opposite_slopes_do_not_lose_filtered_highlight_width(self):
        slopes = np.array((((0.6, 0), (-0.6, 0)), ((-0.6, 0), (0.6, 0))))
        level = mip_pyramid(slope_moments(slopes, np.full((2, 2), 0.2)))[-1]
        normal, rough, _ = resolve_moments(level)
        np.testing.assert_array_equal(normal, (((0, 0, 1),),))
        self.assertAlmostEqual(float(rough[0, 0]), (0.2**4 + 0.6**2) ** 0.25)
        self.assertAlmostEqual(
            float(resolve_moments(level, broaden=False)[1][0, 0]), 0.2
        )

    def test_invalid_metric_moment_energy_and_dimensions_reject(self):
        for a in (
            np.zeros((2, 2, 5)),
            np.full((2, 2, 6), np.nan),
            np.full((2, 2, 6), 2),
            np.array((1, 0, 0, 0, 0, 0.1)),
        ):
            with self.assertRaises(ValueError):
                resolve_moments(a)
        with self.assertRaises(ValueError):
            tangent_slopes(((1, 0, 0),), ((2, 0, 0),), (0,), (0,))
        with self.assertRaises(ValueError):
            tangent_slopes(((1, 0, 0),), ((0, 1, 0),), (0, 0), (0,))
        with self.assertRaises(ValueError):
            slope_moments(np.zeros((3, 3, 2)), np.zeros((3, 3)))

    def test_float32_cached_moments_preserve_normal_and_roughness(self):
        slopes = np.random.default_rng(66).normal(0, 0.08, (8, 8, 2))
        moments = slope_moments(slopes, np.full((8, 8), 0.3))
        normal, rough, _ = resolve_moments(moments)
        actual_normal, actual_rough, _ = resolve_moments(
            moments.astype(np.float32).astype(float)
        )
        np.testing.assert_allclose(actual_normal, normal, atol=1e-8)
        np.testing.assert_allclose(actual_rough, rough, atol=1e-7)
