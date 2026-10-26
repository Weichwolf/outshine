"""Closed-column and prior-model checks for the variable-extinction oracle."""

import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "render"))
from air_vertical_reference import VerticalAir
from cloud_air_reference import RAY, origin_for
from cloud_air_source_reference import (
    PSI,
    SOLAR,
    inputs,
    medium_parameters,
    reference_ray,
)
from profiled_air_reference import earth_parameters, reference


class ProfiledAirReferenceTest(unittest.TestCase):
    def test_rejects_a_solar_frame_different_from_the_weather_control(self):
        ray = inputs()[0][0].copy()
        ray["ray"][2] = -0.8
        with self.assertRaisesRegex(ValueError, "declared daylight Sun"):
            reference(ray, np.array([0, 1]), earth_parameters(), 0.65)

    def test_full_vertical_optical_depth_matches_closed_exponential_and_ozone_columns(
        self,
    ):
        medium = earth_parameters()
        ray = np.zeros((), RAY)
        ray["ray"] = [6360.02, 1, 0.8, 0.8, 10000]
        queries = np.array([0, 0.001, 1, 10, 25, 40, 10000])
        height = origin_for(ray, medium[0])[1] * 0.001
        length = np.minimum(queries, medium[1] - medium[0] - height)
        expected = np.exp(
            -VerticalAir(medium.astype(float)).optical_depth(height, length)
        )
        actual = reference(ray, queries, medium, 0)
        np.testing.assert_allclose(actual[:, 3:], expected, atol=3e-14)

    def test_rayleigh_only_radiance_is_a_closed_extinction_response(self):
        medium = earth_parameters()
        medium[7:12] = 0
        ray = np.zeros((), RAY)
        ray["ray"] = [6360.02, 1, 0.8, 0.8, 10000]
        queries = np.array([0, 0.001, 1, 10, 10000])
        height = origin_for(ray, medium[0])[1] * 0.001
        length = np.minimum(queries, medium[1] - medium[0] - height)
        through = np.exp(
            -VerticalAir(medium.astype(float)).optical_depth(height, length)
        )
        phase = 3 * (1 + float(ray["ray"][3]) ** 2) / (16 * np.pi)
        expected = (SOLAR * phase + PSI) * (1 - through)
        np.testing.assert_allclose(
            reference(ray, queries, medium, 0)[:, :3], expected, atol=3e-14
        )

    def test_homogeneous_limit_matches_existing_independent_cloud_rte(self):
        rays, _ = inputs()
        medium = medium_parameters()
        query = np.array([0, 0.01, 0.1, 1, 4])
        for index in (0, 12, 24):
            ray = rays[index]
            for cover in (0, 0.65):
                actual = reference(ray, query, medium, cover)
                expected = reference_ray(ray, query, medium, cover)
                np.testing.assert_allclose(actual, expected, atol=3e-13)


if __name__ == "__main__":
    unittest.main()
