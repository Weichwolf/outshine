"""Independent column invariance, linear visibility and explicit fallback controls."""

import unittest

import numpy as np
from cloud_field import Layer, optical_depth
from cloud_sun_reuse import SolarLevel, build_field, column_points, sun_frame


class SolarReuseTest(unittest.TestCase):
    def test_full_column_is_invariant_below_the_inner_shell(self):
        generator = np.random.default_rng(353)
        xz = generator.uniform(-19800, 19800, (256, 2))
        height = generator.uniform(20, 2200, len(xz))
        layer = Layer(cover=0.65)
        radius = layer.radius_m
        horizontal2 = np.sum(xz * xz, axis=1)
        root = np.sqrt((radius + height) ** 2 - horizontal2)
        y = (height * (2 * radius + height) - horizontal2) / (root + radius)
        receivers = np.c_[xz[:, 0], y, xz[:, 1]]
        for cosine in (0.8, 0.2, 0.02):
            sun, u, v = sun_frame([-np.sqrt(1 - cosine * cosine), cosine, 0])
            projected = np.c_[receivers @ u, receivers @ v]
            column = column_points(projected, sun, u, v, layer)
            expected = optical_depth(receivers, sun, layer, 6, 37, maximum_m=1e8)
            actual = optical_depth(column, sun, layer, 6, 37, maximum_m=1e8)
            np.testing.assert_allclose(actual, expected, atol=1e-8, rtol=1e-10)

    def test_visibility_blends_transmitted_power(self):
        first, second = np.exp(-np.array([0, 2.0]))
        level = SolarLevel(
            np.zeros(2), np.ones(2), np.array([[first, second], [first, second]])
        )
        value, _ = level.sample(np.array([[0.5, 0.5]]))
        self.assertAlmostEqual(float(value[0]), (first + second) / 2)
        self.assertGreater(float(value[0]), np.exp(-1))

    def test_uncovered_and_inside_cloud_points_use_the_actual_direct_column(self):
        layer = Layer(cover=0.65)
        sun = np.array([-0.6, 0.8, 0])
        field = build_field(layer, sun, 37, [0, 1772, 0], dimension=33)
        points = np.array([[50000, 20, 0], [0, 2800, 0]])
        counts = {"points": 0, "direct_points": 0, "texture_samples": 0}
        actual = field.transfer(points, counts)
        expected = np.exp(-optical_depth(points, sun, layer, 6, 37, maximum_m=1e8))
        np.testing.assert_allclose(actual, expected, atol=1e-14, rtol=0)
        self.assertEqual(counts["direct_points"], 2)
        self.assertEqual(counts["texture_samples"], 0)
        sun[0] = 0
        self.assertAlmostEqual(field.sun[0], -0.6)

    def test_private_map_contract_rejects_invalid_solar_frames(self):
        for value in ([0, -1, 0], [0, 2, 0], [np.nan, 1, 0]):
            with self.assertRaises(ValueError):
                sun_frame(value)


if __name__ == "__main__":
    unittest.main()
