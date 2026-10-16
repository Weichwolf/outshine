"""Analytic vacuum/planet and equivalent-entry checks for the sky-disc oracle."""

import unittest

import numpy as np
from sky_disc_reference import transfer


def frame(radius):
    parameters = np.zeros(48)
    parameters[23], parameters[28], parameters[29] = radius, 6360, 6460
    return parameters


class SkyDiscReferenceTest(unittest.TestCase):
    def test_empty_atmosphere_path_preserves_unattenuated_sun(self):
        lut = np.full((4, 8, 3), 0.125)
        for direction in ([1, 0, 0], [0, 1, 0], [np.sqrt(0.99), -0.1, 0]):
            result, visible = transfer(frame(6960), np.array(direction), lut)
            np.testing.assert_array_equal(result, [1, 1, 1])
            self.assertTrue(visible)

    def test_planet_occlusion_is_exact_zero_even_with_nonzero_lut(self):
        result, visible = transfer(
            frame(6960), np.array([0, -1, 0]), np.ones((4, 8, 3))
        )
        np.testing.assert_array_equal(result, [0, 0, 0])
        self.assertFalse(visible)

    def test_outside_camera_matches_relocated_atmosphere_entry(self):
        generator = np.random.default_rng(397)
        lut = generator.random((16, 32, 3))
        radius, cosine = 6960, -0.4
        direction = np.array([np.sqrt(1 - cosine**2), cosine, 0])
        origin = np.array([0, radius, 0])
        roots = np.roots(
            [1, 2 * np.dot(origin, direction), np.dot(origin, origin) - 6460**2]
        )
        entry = origin + min(roots) * direction
        entry_cosine = np.dot(entry / np.linalg.norm(entry), direction)
        relocated = np.array([np.sqrt(1 - entry_cosine**2), entry_cosine, 0])
        outside, visible = transfer(frame(radius), direction, lut)
        at_top, top_visible = transfer(frame(6460), relocated, lut)
        np.testing.assert_allclose(outside, at_top, atol=2e-14)
        self.assertTrue(visible and top_visible)


if __name__ == "__main__":
    unittest.main()
