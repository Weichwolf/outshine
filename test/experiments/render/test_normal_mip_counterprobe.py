"""Independent exact-direction controls for finite normal-mip exclusion."""

import unittest
from fractions import Fraction as F

import numpy as np
from normal_mip_counterprobe import analyse, mip_vectors, segment_candidates


class NormalMipTests(unittest.TestCase):
    def test_interior_direction_has_exact_half_weight(self):
        result = segment_candidates((1, 0, 1), (0, 1, 1), (1, 1, 2))
        best = min(result, key=lambda r: r["unit_error"])
        self.assertEqual(best["weight"], "1/2")
        self.assertEqual(best["unit_error"], 0)
        self.assertFalse(best["outside_unchanged_normal_bound"])

    def test_orthogonal_directions_cannot_explain_observation(self):
        result = segment_candidates((0, 1, 0), (0, 0, 1), (1, 0, 0))
        self.assertTrue(all(r["outside_unchanged_normal_bound"] for r in result))
        for row in result:
            self.assertAlmostEqual(row["unit_error"], np.sqrt(2))

    def test_constant_segment_and_original_normal_bound(self):
        a = (1, 0, 0)
        within = segment_candidates(a, a, (1, F(1, 1000), 0))
        outside = segment_candidates(a, a, (1, F(1, 10), 0))
        self.assertTrue(all(not r["outside_unchanged_normal_bound"] for r in within))
        self.assertTrue(all(r["outside_unchanged_normal_bound"] for r in outside))

    def test_zero_normal_is_unresolved(self):
        self.assertIsNone(segment_candidates((0, 0, 1), (0, 0, -1), (1, 0, 0)))
        self.assertIsNone(segment_candidates((0, 0, 1), (0, 0, 1), (0, 0, 0)))

    def test_subnormal_lengths_do_not_underflow_direction_comparison(self):
        tiny = float.fromhex("0x0.0000000000001p-1022")
        result = segment_candidates((tiny, 0, tiny), (0, tiny, tiny), (1, 1, 2))
        best = min(result, key=lambda row: row["unit_error"])
        self.assertEqual(best["weight"], "1/2")
        self.assertEqual(best["unit_error"], 0)
        self.assertFalse(best["outside_unchanged_normal_bound"])

    def test_unorm_weights_and_unchanged_unit_direction(self):
        pixel = np.array([100, 150, 200, 255], np.uint8)
        levels = [np.tile(pixel, (2, 2, 1)), pixel[None, None]]
        vectors = mip_vectors(levels, 0, 0, np.eye(3))
        expected = (F(-55, 255), F(45, 255), F(145, 255))
        self.assertEqual(vectors, [expected, expected])
        before = np.ones((2, 2, 4), np.float32)
        before[:, :, :3] = [-55, 45, 145]
        after = before.copy()
        after[0, 0, :3] *= 2
        result = analyse(levels, before, after, np.eye(3))
        self.assertEqual(len(result), 1)
        self.assertTrue(result[0]["before_within_bound"])
        self.assertFalse(result[0]["all_finite_lod_outside_bound"])
        self.assertEqual(result[0]["best_model_candidate"]["unit_error"], 0)

    def test_incomplete_or_invalid_images_are_rejected(self):
        with self.assertRaises(ValueError):
            mip_vectors([np.zeros((2, 2, 4), np.uint8)], 0, 0, np.eye(3))
        with self.assertRaises(ValueError):
            mip_vectors([np.zeros((3, 3, 4), np.uint8)], 0, 0, np.eye(3))

    def test_changed_facing_is_outside_fixed_frame_hypothesis(self):
        levels = [np.full((2, 2, 4), 128, np.uint8), np.full((1, 1, 4), 128, np.uint8)]
        before = np.ones((2, 2, 4), np.float32)
        after = before.copy()
        after[0, 0, 3] = -1
        with self.assertRaises(ValueError):
            analyse(levels, before, after, np.eye(3))


if __name__ == "__main__":
    unittest.main()
