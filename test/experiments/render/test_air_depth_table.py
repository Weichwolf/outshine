"""Independent depth-interpolation controls for the proposed finite-air volume."""

import dataclasses
import unittest
from pathlib import Path

import numpy as np
from air_path_depth_table import build_column, depth_limit, sample_column
from air_path_integral import engine_clear_air

ROOT = Path(__file__).resolve().parents[3]


class DepthTableContracts(unittest.TestCase):
    def setUp(self):
        self.air = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")

    def test_collinear_homogeneous_radiance_is_linear_in_physical_distance(self):
        beta, height = 0.02, 1.0
        air = dataclasses.replace(
            self.air,
            rayleigh_height_km=np.inf,
            rayleigh_per_km=(beta, beta, beta),
            ozone_per_km=(0, 0, 0),
        )
        coefficient = (
            beta
            * 3.0
            / (8.0 * np.pi)
            * np.exp(-beta * (air.top_km - air.bottom_km - height))
        )
        for mapping in ("linear", "quadratic"):
            column = build_column(air, (height, 1.0, 1.0, 1.0), 32, mapping)
            for distance in (0.0, 0.001, 0.01, 0.5, 10.0):
                radiance, _ = sample_column(column, distance, mapping)
                np.testing.assert_allclose(
                    radiance, coefficient * distance, rtol=1e-11, atol=1e-14
                )

    def test_vacuum_storage_and_interpolation_are_identity(self):
        air = dataclasses.replace(
            self.air, rayleigh_per_km=(0, 0, 0), ozone_per_km=(0, 0, 0)
        )
        column = build_column(
            air, (0.5, -0.2, 0.7, 0.0), 32, "quadratic", half_storage=True
        )
        for distance in (0.0, 0.001, column[2] * 0.5, column[2], column[2] * 2.0):
            radiance, through = sample_column(column, distance, "quadratic")
            np.testing.assert_array_equal(radiance, np.zeros(3))
            np.testing.assert_array_equal(through, np.ones(3))

    def test_ground_top_and_declared_range_bound_each_column(self):
        height = 0.5
        self.assertAlmostEqual(depth_limit(self.air, height, -1.0, 200.0), height)
        self.assertAlmostEqual(
            depth_limit(self.air, height, 1.0, 200.0),
            self.air.top_km - self.air.bottom_km - height,
        )
        self.assertEqual(depth_limit(self.air, height, 0.0, 200.0), 200.0)


if __name__ == "__main__":
    unittest.main()
