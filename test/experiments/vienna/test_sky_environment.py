import tempfile
import unittest
from pathlib import Path

import numpy as np
from sky_environment import (
    SKY_ENVIRONMENT_SIZE,
    diffuse_coefficients,
    load_environment,
    save_environment,
)


class SkyEnvironmentTest(unittest.TestCase):
    def test_diffuse_kernel_preserves_uniform_light_and_orients_the_sky(self):
        width, height = SKY_ENVIRONMENT_SIZE
        latitude = np.pi * ((np.arange(height) + 0.5) / height - 0.5)
        base = np.array([0.22, 0.32, 0.45])
        gradient = np.array([0.11, 0.08, 0.05])
        radiance = base + np.sin(latitude)[:, None, None] * gradient
        radiance = np.broadcast_to(radiance, (height, width, 3))

        coefficients = diffuse_coefficients(radiance)
        np.testing.assert_allclose(0.2820947918 * coefficients[0], base, atol=1e-6)
        np.testing.assert_allclose(
            0.4886025119 * coefficients[2], (2 / 3) * gradient, atol=2e-6
        )
        np.testing.assert_allclose(coefficients[[1, 3, 4, 5, 7, 8]], 0, atol=1e-6)

    def test_environment_is_tied_to_the_rendered_altitude_and_sun(self):
        radiance = np.full((*SKY_ENVIRONMENT_SIZE[::-1], 3), 0.125, dtype="f4")
        sun = (0.0, 0.6, 0.8)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "sky.npz"
            save_environment(path, radiance, sun, 156.25)
            np.testing.assert_array_equal(load_environment(path, sun, 156.25), radiance)
            with self.assertRaisesRegex(ValueError, "sun or eye altitude"):
                load_environment(path, (0.6, 0.0, 0.8), 156.25)
            with self.assertRaisesRegex(ValueError, "sun or eye altitude"):
                load_environment(path, sun, 157.25)


if __name__ == "__main__":
    unittest.main()
