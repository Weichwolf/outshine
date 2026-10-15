"""Closed-form and vacuum counterexamples for the independent slab reference."""

import unittest

import numpy as np
from cloud_view_reference import transfer


class ReferenceControls(unittest.TestCase):
    def setUp(self):
        self.medium = np.zeros(20, dtype="<f4")
        self.medium[:4] = [6360, 6460, 1e20, 1e20]
        self.weather = np.zeros((6, 4), dtype="<f4")
        self.weather[0] = [6360000, 2572, 600, 0.012]
        self.ray = np.array([[0, 1772, 0, 0, 0, -1, 6361.772, 0, 0.8, 0, 2.0]])

    def test_zero_coefficients_are_exact_vacuum(self):
        actual = transfer(self.ray, self.medium, self.weather, 37.0)
        np.testing.assert_array_equal(actual, [[0, 0, 0, 1, 1, 1]])

    def test_homogeneous_air_matches_closed_form(self):
        self.medium[4:7] = [0.01, 0.02, 0.03]
        beta = self.medium[4:7].astype(float)
        through = np.exp(-2.0 * beta)
        sunlight = np.array([0.75, 0.5, 0.25])
        psi = np.array([0.015625, 0.03125, 0.0625])
        emission_per_extinction = sunlight * (3.0 / (16.0 * np.pi)) + psi
        expected = np.r_[emission_per_extinction * (1.0 - through), through]
        actual = transfer(self.ray, self.medium, self.weather, 37.0)[0]
        np.testing.assert_allclose(actual, expected, rtol=1e-13, atol=1e-15)

    def test_foreground_before_atmosphere_and_cloud_is_vacuum(self):
        self.medium[4:7] = [0.01, 0.02, 0.03]
        self.weather[1, 0] = 0.65
        ray = np.array([[0, 110000, 0, 0, -1, 0, 6470, -1, 0.8, -0.8, 2.0]])
        actual = transfer(ray, self.medium, self.weather, 37.0)
        np.testing.assert_array_equal(actual, [[0, 0, 0, 1, 1, 1]])


if __name__ == "__main__":
    unittest.main()
