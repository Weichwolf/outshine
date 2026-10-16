"""Analytic range, source-integration and near-eye controls (#353)."""

import unittest
from itertools import pairwise

import numpy as np
from cloud_air_depth import linear_source, optical_mass, reconstruct
from cloud_air_depth_cases import solar_transfer
from cloud_air_transfer_reuse import cubic
from cloud_field import Layer, optical_depth
from cloud_radiance import SUN


class AirDepthTest(unittest.TestCase):
    def test_constant_source_including_zero_extinction(self):
        sigma = np.array([0, 0.02, 0.1])
        nodes = np.array([0, 0.001, 1, 10, 1000])
        queries = np.r_[0, np.geomspace(1e-9, 1000, 1001)]
        actual = linear_source(nodes, np.full(len(nodes), 0.03), queries, sigma)
        np.testing.assert_allclose(
            actual, 0.03 * optical_mass(queries, sigma), rtol=3e-13, atol=1e-14
        )

    def test_linear_source_matches_independent_gauss_integrals(self):
        sigma = np.array([0, 0.02, 0.1])
        nodes = np.array([0.0, 1e-6, 0.01, 1, 4, 200])
        source = np.array([0.3, 0.3, 0.1, 0.6, 0.2, 1])
        queries = np.unique(np.r_[nodes, 0.1, 0.5, 3, 20, 50])
        x, w = np.polynomial.legendre.leggauss(64)
        expected = []
        for query in queries:
            value = np.zeros(3)
            for a, b in pairwise(nodes):
                end = min(b, query)
                if end <= a:
                    continue
                s = 0.5 * (end + a) + 0.5 * (end - a) * x
                c = np.interp(s, nodes, source)
                value += (
                    0.5
                    * (end - a)
                    * np.sum(
                        w[:, None] * c[:, None] * np.exp(-s[:, None] * sigma), axis=0
                    )
                )
            expected.append(value)
        np.testing.assert_allclose(
            linear_source(nodes, source, queries, sigma),
            expected,
            rtol=2e-12,
            atol=2e-13,
        )

    def test_plateaus_and_edges_preserve_physical_integral_bounds(self):
        nodes = np.array([0.0, 0.1, 0.3, 3, 5, 200])
        source = np.array([0.0, 0, 1, 1, 0, 0])
        queries = np.linspace(0, 200, 10001)
        sigma = np.array([0, 0.01, 0.2])
        actual = linear_source(nodes, source, queries, sigma)
        self.assertGreaterEqual(float(actual.min()), 0)
        self.assertLessEqual(
            float(np.max(actual - optical_mass(queries, sigma))), 1e-13
        )
        self.assertGreaterEqual(float(np.diff(actual, axis=0).min()), -1e-13)
        image = np.broadcast_to(np.array([0, 0, 1, 1, 1])[None, :, None], (4, 5, 1))
        signed = cubic(image, 33, 25, 8)
        self.assertEqual(float(signed.min()), -0.0732421875)
        self.assertEqual(float(signed.max()), 1.0732421875)

    def test_true_near_zero_limit_removes_false_visible_source(self):
        nodes = np.array([0.0, 1.0])
        sigma = np.array([0.01, 0.02, 0.03])
        mass = 0.02 * optical_mass(nodes, sigma)
        queries = np.array([0.0, 1e-6])
        source = np.full(2, 0.02)
        expected = 0.02 * optical_mass(queries, sigma)
        actual = reconstruct(nodes, mass, source, queries, sigma, "visibility")
        np.testing.assert_allclose(actual, expected, atol=1e-20)
        legacy = reconstruct(nodes, mass, source, queries, sigma, "legacy-visibility")
        self.assertGreater(float(np.min(legacy[1] / expected[1])), 49)
        np.testing.assert_array_equal(actual[0], 0)

    def test_sparse_and_repeated_queries_share_the_same_curve(self):
        nodes = np.array([0.0, 0.5, 3, 10])
        source = np.array([0.3, 0.9, 0.1, 1])
        sigma = np.array([0.01, 0.02, 0.03])
        dense = np.linspace(0, 10, 101)
        selected = np.repeat(np.arange(0, len(dense), 5), 2)
        np.testing.assert_array_equal(
            linear_source(nodes, source, dense, sigma)[selected],
            linear_source(nodes, source, dense[selected], sigma),
        )
        with self.assertRaisesRegex(ValueError, "zero to one"):
            linear_source(nodes, source + 1, dense, sigma)
        with self.assertRaisesRegex(ValueError, "increase strictly"):
            linear_source(np.array([0, 0, 1]), np.ones(3), dense, sigma)

    def test_sparse_solar_reference_preserves_the_weather_definition(self):
        points = np.array([[0.0, 20, 0], [0, 2800, 0], [0, 110000, 0], [150000, 20, 0]])
        for seconds in (0, 37):
            expected = np.exp(
                -optical_depth(
                    points, SUN, Layer(cover=0.65), 6, seconds, maximum_m=1e8
                )
            )
            np.testing.assert_array_equal(
                solar_transfer(points, 0.65, seconds), expected
            )


if __name__ == "__main__":
    unittest.main()
