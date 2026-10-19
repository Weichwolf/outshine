"""Exact lateral perspective controls and restricted reuse-domain checks."""

import unittest

import numpy as np
from plane_depth_budget import budget, quantized_depth


class PlaneBudgetTests(unittest.TestCase):
    def test_exact_projected_points_meet_the_conditional_target(self):
        rng = np.random.default_rng(12)
        for near, far in ((100, 200), (1000, 1100), (4000, 4100)):
            spec = budget(near, far, 1044.33155, 64, 0.5)
            depth = np.r_[near, far, rng.uniform(near, far, 1000)]
            x = rng.uniform(-30, 30, len(depth))
            plane = quantized_depth(depth, spec)
            for shift in (-64, -32, 0, 32, 64):
                actual = 1044.33155 * (x - shift) / depth
                held = 1044.33155 * (x / depth * plane - shift) / plane
                self.assertLessEqual(float(abs(actual - held).max()), 0.5 + 1e-10)

    def test_uniform_inverse_centres_cover_full_interval(self):
        spec = budget(1000, 1100, 1000, 64, 0.5)
        self.assertEqual(spec["planes"], 6)
        z = np.linspace(1000, 1100, 1001)
        held = quantized_depth(z, spec)
        self.assertEqual(len(np.unique(held)), 6)
        self.assertLessEqual(64 * 1000 * abs(1 / z - 1 / held).max(), 0.5)

    def test_zero_reuse_constant_depth_and_bad_inputs(self):
        self.assertEqual(budget(10, 10000, 1000, 0, 0.5)["planes"], 1)
        spec = budget(100, 100, 1000, 64, 0.5)
        np.testing.assert_array_equal(quantized_depth([100, 100], spec), [100, 100])
        for values in (
            (0, 1, 1000, 64, 0.5),
            (2, 1, 1000, 64, 0.5),
            (1, 2, 0, 64, 0.5),
            (1, 2, 1000, -1, 0.5),
            (1, 2, 1000, 64, float("nan")),
        ):
            with self.assertRaises(ValueError):
                budget(*values)


if __name__ == "__main__":
    unittest.main()
