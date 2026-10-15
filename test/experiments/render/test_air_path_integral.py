"""Independent closed forms and physical invariants for the finite-air comparison."""

import dataclasses
import unittest
from pathlib import Path

import numpy as np
from air_path_integral import (
    engine_clear_air,
    gauss_path,
    height_along,
    midpoint_path,
    source,
    transmittance,
)

ROOT = Path(__file__).resolve().parents[3]


class AirPathContracts(unittest.TestCase):
    def setUp(self):
        self.air = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")

    def test_vacuum_is_identity(self):
        vacuum = dataclasses.replace(
            self.air, rayleigh_per_km=(0, 0, 0), ozone_per_km=(0, 0, 0)
        )
        for method in (gauss_path, midpoint_path):
            light, through = method(vacuum, 1.0, 0.5, 0.7, 0, 20.0, 16)
            np.testing.assert_array_equal(light, np.zeros(3))
            np.testing.assert_array_equal(through, np.ones(3))

    def test_collinear_homogeneous_sun_has_closed_form(self):
        beta = 0.02
        air = dataclasses.replace(
            self.air,
            rayleigh_height_km=np.inf,
            rayleigh_per_km=(beta, beta, beta),
            ozone_per_km=(0, 0, 0),
        )
        height, distance = 1.0, 10.0
        top_path = air.top_km - air.bottom_km - height
        expected = beta * (3.0 / (8.0 * np.pi)) * np.exp(-beta * top_path) * distance
        for count in (4, 8, 16, 32):
            gauss, through = gauss_path(air, height, 1.0, 1.0, 1.0, distance, count)
            midpoint, _ = midpoint_path(air, height, 1.0, 1.0, 1.0, distance, count)
            half_step_depth = beta * distance / count * 0.5
            midpoint_expected = expected * np.sinh(half_step_depth) / half_step_depth
            np.testing.assert_allclose(gauss, expected, rtol=1e-11, atol=1e-14)
            np.testing.assert_allclose(through, np.exp(-beta * distance), rtol=1e-13)
            np.testing.assert_allclose(
                midpoint, midpoint_expected, rtol=1e-11, atol=1e-14
            )

    def test_short_segment_tends_to_local_source(self):
        arguments = (self.air, 0.492, 0.2, 0.7, 0.5)
        _, local = source(*arguments, np.array([0.0]))
        for method in (gauss_path, midpoint_path):
            radiance, _ = method(*arguments, 1e-6, 16)
            np.testing.assert_allclose(radiance / 1e-6, local[0], rtol=1e-6)

    def test_segment_transmittance_composes(self):
        radius = self.air.bottom_km + 0.5
        cosine, a, b = 0.2, 0.7, 2.3
        first = transmittance(self.air, radius, cosine, a)
        end_radius = self.air.bottom_km + height_along(self.air, radius, cosine, a)
        end_cosine = (radius * cosine + a) / end_radius
        second = transmittance(self.air, end_radius, end_cosine, b)
        whole = transmittance(self.air, radius, cosine, a + b)
        np.testing.assert_allclose(first * second, whole, rtol=1e-11, atol=1e-13)

    def test_ozone_column_matches_triangle_area(self):
        air = dataclasses.replace(self.air, rayleigh_per_km=(0, 0, 0))
        distance = air.ozone_centre_km + air.ozone_width_km
        actual = transmittance(air, air.bottom_km, 1.0, distance, samples=256)
        expected = np.exp(-np.array(air.ozone_per_km) * air.ozone_width_km)
        np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-6)


if __name__ == "__main__":
    unittest.main()
