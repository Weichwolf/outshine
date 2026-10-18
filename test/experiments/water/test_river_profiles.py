import unittest

import numpy as np

from river_profiles import RiverProfile


class RiverProfiles(unittest.TestCase):
    def test_affine_flow_reproduces_a_straight_uniform_slope(self):
        x = np.arange(0, 101, 10, dtype=float)
        nodes = np.stack((np.c_[x[:-1], np.zeros(10)], np.c_[x[1:], np.zeros(10)]), axis=1)
        levels = 400 + nodes[:, :, 0] * .01
        queries = np.array([[49, 0], [49, 25], [51, -25], [79, 20]])
        profile = RiverProfile(nodes, levels)
        expected = 400 + queries[:, 0] * .01
        got, _ = profile.sample(queries, "blend-affine")
        np.testing.assert_allclose(got, expected, rtol=0, atol=1e-12)
        clamped, _ = profile.sample(queries, "blend-clamped")
        self.assertGreater(float(np.max(np.abs(clamped - expected))), 1e-4)

    def test_nearest_branch_switch_is_a_real_height_jump(self):
        segments = [[[-100, -10], [100, -10]], [[-100, 10], [100, 10]]]
        profile = RiverProfile(segments, [[400, 400], [410, 410]])
        points = [[0, -1e-6], [0, 1e-6]]
        nearest, _ = profile.sample(points, "nearest")
        blended, _ = profile.sample(points, "blend-affine")
        self.assertAlmostEqual(float(abs(nearest[1] - nearest[0])), 10)
        self.assertLess(float(abs(blended[1] - blended[0])), 1e-5)

    def test_flat_water_stays_flat_and_source_order_does_not_matter(self):
        segments = np.array([[[0, 0], [100, 0]], [[100, 0], [100, 100]]], dtype=float)
        levels = np.full((2, 2), 123.0)
        points = [[90, 10], [110, -10], [105, 70]]
        a, _ = RiverProfile(segments, levels).sample(points, "blend-affine")
        b, _ = RiverProfile(segments[::-1, ::-1], levels[::-1, ::-1]).sample(points, "blend-affine")
        np.testing.assert_allclose(a, 123, rtol=0, atol=1e-12)
        np.testing.assert_array_equal(a, b)

    def test_reference_exhaustion_is_local_and_explicit(self):
        segments = [[[0, 0], [10, 0]], [[0, 1], [10, 1]], [[1000, 0], [1010, 0]]]
        profile = RiverProfile(segments, [[0, 0], [1, 1], [2, 2]])
        height, count = profile.sample([[5, .5], [1005, 0]], "blend-affine", reference_limit=1)
        self.assertTrue(np.isnan(height[0]))
        self.assertGreater(count[0], 1)
        self.assertEqual(height[1], 2)
        self.assertEqual(count[1], 1)

    def test_invalid_source_geometry_is_rejected(self):
        for segments, levels in [([], []), ([[[0, 0], [0, 0]]], [[0, 1]]),
                                 ([[[0, 0], [1, 0]]], [[0, np.nan]])]:
            with self.subTest(segments=segments), self.assertRaises(ValueError):
                RiverProfile(segments, levels)


if __name__ == "__main__":
    unittest.main()
