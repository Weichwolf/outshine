"""Controls independent of the spatial source fitting and union traversal."""

import unittest

import numpy as np
from cloud_air_source_reference import (
    coefficients,
    inputs,
    medium_parameters,
    reference_ray,
)


class SourceReferenceTest(unittest.TestCase):
    def test_null_cloud_matches_closed_form_with_actual_entry_and_ground(self):
        rays, queries = inputs()
        medium = medium_parameters()
        for index in (2, 6, 24, 25):
            ray = rays[index]
            distances = queries[int(ray["begin"]) : int(ray["end"])]
            measured = reference_ray(ray, distances, medium, 0)
            # Derive the eye's sphere intersections independently of the reference helper.
            radius, cosine = map(float, ray["ray"][:2])
            projection = radius * cosine
            entry = max(0, -projection - np.sqrt(projection**2 + 6460**2 - radius**2))
            exit_km = -projection + np.sqrt(projection**2 + 6460**2 - radius**2)
            ground_discriminant = projection**2 + 6360**2 - radius**2
            if cosine < 0 and ground_discriminant >= 0:
                exit_km = min(exit_km, -projection - np.sqrt(ground_discriminant))
            path_m = (
                np.maximum(0, np.minimum(distances.astype(float), exit_km) - entry)
                * 1000
            )
            sigma, direct, diffuse = coefficients(ray, medium)
            through = np.exp(-path_m[:, None] * sigma)
            expected = (direct + diffuse) * -np.expm1(-path_m[:, None] * sigma) / sigma
            np.testing.assert_allclose(
                measured[:, :3], expected, atol=2e-13, rtol=1e-10
            )
            np.testing.assert_allclose(measured[:, 3:], through, atol=2e-13, rtol=1e-10)

    def test_procedural_shadow_reference_refines_before_the_source_fit(self):
        rays, _ = inputs()
        medium = medium_parameters()
        distances = np.array([0, 0.25, 1, 3, 10])
        low = reference_ray(rays[8], distances, medium, 0.65, 16)
        high = reference_ray(rays[8], distances, medium, 0.65, 32)
        clear = reference_ray(rays[8], distances, medium, 0)
        np.testing.assert_allclose(low, high, atol=1e-9, rtol=0)
        np.testing.assert_allclose(high[:, 3:], clear[:, 3:], atol=1e-13, rtol=0)
        self.assertTrue(np.all(high[1:, :3] < clear[1:, :3]))

    def test_reference_rejects_nonhomogeneous_coefficients(self):
        rays, _ = inputs()
        medium = medium_parameters()
        medium[2] = 8
        with self.assertRaisesRegex(ValueError, "homogeneous"):
            reference_ray(rays[2], np.array([0, 1]), medium, 0)


if __name__ == "__main__":
    unittest.main()
