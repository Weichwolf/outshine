"""Independent coverage-weighted blocks, dark borders and immutable cached material levels."""

import unittest

import numpy as np
from outshine_ref.vegetation import LeafMaterialLevel, leaf_material_mips
from outshine_ref.vegetation.leaf_normals import slope_moments


class LeafMaterialTests(unittest.TestCase):
    def test_partial_coverage_mips_against_original_pixel_statistics(self):
        rng = np.random.default_rng(41)
        optical = rng.uniform(0.01, 0.3, (8, 8, 6))
        coverage = rng.uniform(0, 1, (8, 8))
        coverage[0] = 0
        slopes = rng.normal(0, 0.2, (8, 8, 2))
        rough = rng.uniform(0.1, 0.4, (8, 8))
        levels = leaf_material_mips(optical, coverage, slope_moments(slopes, rough))
        for i, level in enumerate(levels):
            block = 2**i
            resolved = level.resolve()
            for row, col in np.ndindex(level.coverage.shape):
                rows, cols = (
                    slice(row * block, (row + 1) * block),
                    slice(col * block, (col + 1) * block),
                )
                w = coverage[rows, cols].ravel()
                self.assertAlmostEqual(level.coverage[row, col], w.mean())
                if w.sum() == 0:
                    np.testing.assert_array_equal(
                        level.optical_premultiplied[row, col], 0
                    )
                    continue
                colour = optical[rows, cols].reshape(-1, 6)
                expected = w @ colour / w.sum()
                actual = np.concatenate(
                    (
                        resolved["reflection"][row, col],
                        resolved["transmission"][row, col],
                    )
                )
                np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-16)
                values = slopes[rows, cols].reshape(-1, 2)
                mean = w @ values / w.sum()
                covariance = (
                    (values - mean).T @ ((values - mean) * w[:, None]) / w.sum()
                )
                np.testing.assert_allclose(
                    resolved["slope_covariance"][row, col],
                    covariance[(0, 0, 1), (0, 1, 1)],
                    atol=4 * np.finfo(float).eps,
                )

    def test_transparent_neighbours_do_not_darken_or_tint_resolved_leaf(self):
        optical = np.zeros((2, 2, 6))
        optical[..., 0] = 0.9
        optical[0, 0] = (0.1, 0.2, 0.03, 0.4, 0.6, 0.1)
        coverage = np.array(((1, 0), (0, 0)))
        moments = slope_moments(np.zeros((2, 2, 2)), np.full((2, 2), 0.3))
        coarse = leaf_material_mips(optical, coverage, moments)[1]
        self.assertEqual(coarse.coverage[0, 0], 0.25)
        resolved = coarse.resolve()
        np.testing.assert_allclose(resolved["reflection"][0, 0], optical[0, 0, :3])
        np.testing.assert_allclose(resolved["transmission"][0, 0], optical[0, 0, 3:])

    def test_complete_zero_coverage_and_material_cache_ownership(self):
        optical = np.full((2, 2, 6), 0.2)
        moments = slope_moments(np.zeros((2, 2, 2)), np.full((2, 2), 0.3))
        levels = leaf_material_mips(optical, np.zeros((2, 2)), moments)
        for level in levels:
            self.assertTrue(np.isfinite(level.resolve()["normal"]).all())
            np.testing.assert_array_equal(level.resolve()["transmission"], 0)
            with self.assertRaises(ValueError):
                level.coverage.setflags(write=True)
        restored = LeafMaterialLevel(
            levels[0].optical_premultiplied.copy(),
            levels[0].moments_premultiplied.copy(),
            levels[0].coverage.copy(),
        )
        np.testing.assert_array_equal(restored.coverage, levels[0].coverage)

    def test_invalid_coverage_and_nonzero_empty_texels_reject(self):
        with self.assertRaises(ValueError):
            LeafMaterialLevel(np.ones((2, 2, 6)), np.zeros((2, 2, 6)), np.zeros((2, 2)))
        with self.assertRaises(ValueError):
            leaf_material_mips(
                np.zeros((2, 2, 6)), np.full((2, 2), 1.1), np.zeros((2, 2, 6))
            )
