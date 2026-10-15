"""Independent normal-marginal, spatial and saved-parameter counterexamples."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture
from scipy.special import ndtr
from species_spatial import FourierMarks


class SpatialMarksChecks(unittest.TestCase):
    def test_known_sine_cosine_gaussian_and_uniform_transform(self):
        field = FourierMarks(((1, 0, 0),), ((2, -3),))
        p = np.array(((0, 0, 0), (np.pi / 2, 0, 0), (np.pi, 0, 0)))
        np.testing.assert_allclose(field.gaussian(p), (2, -3, -2), atol=1e-15)
        np.testing.assert_allclose(field.uniforms(p, (0.5,) * 3, 1), ndtr((2, -3, -2)))
        np.testing.assert_array_equal(
            field.uniforms(p, (0, 0.1, 0.9), 0), (0, 0.1, 0.9)
        )
        u = field.uniforms(p, (0, 0.5, np.nextafter(1, 0)), 0.5)
        self.assertTrue(np.isfinite(u).all() and np.all((0 <= u) & (u < 1)))

    def test_stream_matrix_and_chunk_permutation_prepared_replay(self):
        p = np.random.default_rng(8).normal(size=(41, 3)) * 1000 + (
            4000000,
            800000,
            4800000,
        )
        field = FourierMarks.prepare(59, count=64)
        expected = field.gaussian(p)
        np.testing.assert_array_equal(expected, field.gaussian(p, "matrix"))
        np.testing.assert_array_equal(expected[::-1], field.gaussian(p[::-1]))
        np.testing.assert_array_equal(expected[::5], field.gaussian(p[::5]))
        recipe = json.loads(json.dumps(field.recipe()))
        with patch.object(
            FourierMarks, "prepare", side_effect=AssertionError("rebuild")
        ):
            cached = FourierMarks.from_recipe(recipe)
            np.testing.assert_array_equal(cached.gaussian(p), expected)
        with self.assertRaises(ValueError):
            cached.waves.setflags(write=True)

    def test_pointwise_uniform_and_categorical_marginals_over_coefficient_ensemble(
        self,
    ):
        # One fixed world point; test the coefficient ensemble, not correlated rows.
        waves = FourierMarks.prepare(4, count=32).waves
        point = np.array(((4094000.0, 775000.0, 4810000.0),))
        theta = point @ waves.T
        feature = np.stack((np.cos(theta[0]), np.sin(theta[0])), axis=1) / np.sqrt(32)
        self.assertAlmostEqual(float(np.sum(feature * feature)), 1.0, places=14)
        coeff = np.random.default_rng(902).normal(size=(32768, 32, 2))
        z = np.sum(coeff * feature, axis=(1, 2))
        u = ndtr(z)
        # Fixed ~5-sigma finite-sample bounds, separate from real-stand calibration.
        for q in (0.1, 0.25, 0.5, 0.75, 0.9):
            self.assertLess(abs(np.mean(u < q) - q), 0.015)
        mix = SpeciesHeightMixture.prepare(
            ("a", "b", "c"), (0, 1), ((1, 3, 6), (1, 3, 6))
        )
        fractions = np.bincount(mix.choose(0, u), minlength=3) / len(u)
        np.testing.assert_allclose(fractions, (0.1, 0.3, 0.6), atol=0.015, rtol=0)

    def test_covariance_identity_continuity_and_global_coordinates(self):
        field = FourierMarks.prepare(6, length_m=60, count=1024)
        d = np.array(((0, 0, 0), (60, 0, 0), (0, 60, 0), (0, 0, 60), (600, 0, 0)))
        kernel = np.mean(np.cos(d @ field.waves.T), axis=1)
        self.assertEqual(kernel[0], 1)
        np.testing.assert_allclose(kernel[1:4], np.exp(-0.5), atol=0.08, rtol=0)
        self.assertLess(abs(kernel[-1]), 0.08)
        # ECEF has no longitude seam or pole. Tiny adjacent positions stay close.
        p = np.array(((-6378137, 0, 0), (0, 0, 6356752)), dtype=float)
        self.assertLess(np.max(abs(field.gaussian(p) - field.gaussian(p + 1e-4))), 1e-4)
        with self.assertRaises(ValueError):
            field.gaussian(((1e20, 0, 0),))
        with self.assertRaises(ValueError):
            field.uniforms(p, (0, 1))


if __name__ == "__main__":
    unittest.main()
