"""Independent integral and composition counterexamples for shared eye media."""

import unittest

import numpy as np
from cloud_air_reference import cloud_cells, direction_for, origin_for
from cloud_air_source_reference import (
    coefficients,
    inputs,
    medium_parameters,
    reference_ray,
)
from cloud_field import Layer
from linear_medium import cell, prefix


class LinearMediumTest(unittest.TestCase):
    def test_reference_clock_default_and_explicit_state_are_consistent(self):
        rays, _ = inputs()
        ray, medium = rays[12], medium_parameters()
        queries = np.array([0, 0.02, 0.1, 0.4, 1, 4])
        default = reference_ray(ray, queries, medium, 0.65)
        explicit = reference_ray(ray, queries, medium, 0.65, seconds=37)
        stationary = reference_ray(ray, queries, medium, 0.65, seconds=0)
        np.testing.assert_array_equal(default, explicit)
        self.assertGreater(float(np.max(abs(default - stationary))), 0.001)

    def test_stationary_cloud_extinction_matches_declared_cell_integrals(self):
        rays, _ = inputs()
        ray, medium = rays[12], medium_parameters()
        origin, direction = origin_for(ray, medium[0]), direction_for(ray)
        queries = np.array([0.0, 0.02, 0.1, 0.4, 1])
        sigma, _, _ = coefficients(ray, medium)
        tau = sigma * queries[:, None] * 1000
        for begin, end, extinction, _ in cloud_cells(
            Layer(cover=0.65), origin, direction, 1e7, seconds=0
        ):
            tau += extinction * np.clip(queries[:, None] * 1000 - begin, 0, end - begin)
        actual = reference_ray(ray, queries, medium, 0.65, seconds=0)
        np.testing.assert_allclose(actual[:, 3:], np.exp(-tau), atol=2e-14)

    def test_cell_matches_independent_gauss_including_vacuum_and_opaque(self):
        x, weights = np.polynomial.legendre.leggauss(96)
        a, b = np.array([0.2, 1.0, 3.0]), np.array([2.0, 0.4, 0.1])
        for width in (1e-9, 1e-3, 0.1, 1, 100):
            for distance in (0, 0.25 * width, width):
                sigma = np.array([0, 0.01, 1.0])
                points = distance * (x + 1) / 2
                source = a + (b - a) * points[:, None] / width
                expected = (
                    distance
                    / 2
                    * np.sum(
                        weights[:, None] * source * np.exp(-points[:, None] * sigma),
                        axis=0,
                    )
                )
                actual, through = cell(sigma, a, b, width, distance)
                np.testing.assert_allclose(actual, expected, rtol=2e-11, atol=1e-13)
                np.testing.assert_allclose(
                    through, np.exp(-sigma * distance), atol=1e-15
                )

    def test_overlapping_media_must_add_before_integration(self):
        air_sigma = np.array([0.01, 0.02, 0.03])
        cloud_sigma = 2.0
        air_source = np.array([0.01, 0.02, 0.04])
        cloud_source = np.array([0.2, 0.3, 0.4])
        correct, through = cell(
            air_sigma + cloud_sigma,
            air_source + cloud_source,
            air_source + cloud_source,
            3,
            3,
        )
        air, air_t = cell(air_sigma, air_source, air_source, 3, 3)
        cloud, cloud_t = cell(np.full(3, cloud_sigma), cloud_source, cloud_source, 3, 3)
        self.assertGreater(float(np.max(abs(cloud + cloud_t * air - correct))), 0.001)
        self.assertGreater(float(np.max(abs(air + air_t * cloud - correct))), 0.001)
        np.testing.assert_allclose(through, air_t * cloud_t, atol=1e-15)

    def test_sparse_repeated_queries_do_not_change_the_partition(self):
        edges = np.array([0.0, 0.01, 0.4, 1, 100])
        sigma = np.array([[0.01, 0.02, 0.03], [3, 3, 3], [1, 2, 3], [0, 0, 0]])
        a = np.array([[0.0, 1, 2], [1, 2, 3], [0.1, 0.2, 0.3], [0, 0, 0]])
        b = a * 0.7
        queries = np.linspace(0, 100, 1001)
        at = np.repeat(np.arange(0, 1001, 7), 2)
        actual = prefix(edges, sigma, a, b, queries)
        np.testing.assert_array_equal(
            actual[at], prefix(edges, sigma, a, b, queries[at])
        )
        np.testing.assert_array_equal(actual[0], [0, 0, 0, 1, 1, 1])

    def test_restricting_a_linear_cell_preserves_transfer_composition(self):
        sigma = np.array([0.01, 0.2, 3.0])
        a, b = np.array([0.1, 0.3, 0.6]), np.array([1.0, 0.7, 0.2])
        split, width = 0.34, 5.0
        middle = a + (b - a) * split / width
        first, first_t = cell(sigma, a, middle, split, split)
        last, last_t = cell(sigma, middle, b, width - split, width - split)
        total, total_t = cell(sigma, a, b, width, width)
        np.testing.assert_allclose(first + first_t * last, total, rtol=3e-13)
        np.testing.assert_allclose(first_t * last_t, total_t, atol=1e-15)


if __name__ == "__main__":
    unittest.main()
