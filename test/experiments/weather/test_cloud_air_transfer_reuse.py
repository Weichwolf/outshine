"""Analytic controls for response factorization and angular reconstruction."""

import unittest

import numpy as np
from cloud_air_reference import SAMPLE
from cloud_air_source_reference import (
    coefficients as scalar_coefficients,
)
from cloud_air_source_reference import (
    inputs,
    medium_parameters,
    reference_ray,
)
from cloud_air_transfer_reuse import (
    axis_nodes,
    bilinear,
    clear_response,
    coefficients,
    weighted_visibility,
)


class TransferReuseTest(unittest.TestCase):
    def test_vector_coefficients_and_ground_response_match_independent_reference(self):
        rays, _ = inputs()
        medium = medium_parameters()
        sigma, direct, diffuse = coefficients(rays, medium)
        queries = np.full(len(rays), 10.0)
        integral, through, _, _ = clear_response(rays, queries, medium)
        for index, ray in enumerate(rays):
            expected = scalar_coefficients(ray, medium)
            for actual, value in zip((sigma, direct[index], diffuse), expected):
                np.testing.assert_allclose(actual, value * 1000, rtol=2e-14)
            reference = reference_ray(ray, queries[index : index + 1], medium, 0)[0]
            np.testing.assert_allclose(through[index], reference[3:], atol=1e-12)
            np.testing.assert_allclose(
                integral[index] * (direct[index] + diffuse), reference[:3], atol=1e-12
            )

    def test_weighted_visibility_retains_shadow_and_bypasses_only_eye_cloud(self):
        rays, _ = inputs()
        ray = rays[8:9]
        query = np.array([10.0])
        medium = medium_parameters()
        clear = reference_ray(ray[0], query, medium, 0)
        shadow = reference_ray(ray[0], query, medium, 0.65, eye_cover=0)
        np.testing.assert_allclose(shadow[:, 3:], clear[:, 3:], atol=1e-13)
        self.assertTrue(np.all(shadow[:, :3] < clear[:, :3]))
        samples = np.zeros(1, SAMPLE)
        samples["radiance"] = shadow[:, :3]
        visibility = weighted_visibility(samples, ray, query, medium)
        integral, _, direct, diffuse = clear_response(ray, query, medium)
        np.testing.assert_allclose(
            integral * (direct * visibility + diffuse), samples["radiance"], atol=1e-13
        )
        self.assertTrue(np.all((visibility >= 0) & (visibility <= 1)))

    def test_nonuniform_last_cell_reproduces_linear_field_and_endpoints(self):
        width, height, step = 12, 9, 5
        x, y = axis_nodes(width, step), axis_nodes(height, step)
        values = (x[None, :] * 2 + y[:, None] * 3)[..., None]
        actual = bilinear(values, width, height, step)[..., 0]
        expected = np.arange(width)[None, :] * 2 + np.arange(height)[:, None] * 3
        np.testing.assert_allclose(actual, expected, atol=1e-13)
        with self.assertRaises(ValueError):
            axis_nodes(1, step)

    def test_zero_query_is_neutral_and_invalid_medium_is_rejected(self):
        rays, _ = inputs()
        medium = medium_parameters()
        integral, through, _, _ = clear_response(rays, np.zeros(len(rays)), medium)
        np.testing.assert_array_equal(integral, 0)
        np.testing.assert_array_equal(through, 1)
        medium[2] = 8
        with self.assertRaisesRegex(ValueError, "homogeneous"):
            coefficients(rays, medium)


if __name__ == "__main__":
    unittest.main()
