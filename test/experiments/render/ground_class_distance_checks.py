"""Independent range and degenerate-segment controls for the distance comparison."""

import hashlib
import unittest

import numpy as np

from ground_class_distance import compare, expand_ranges
from ground_class_work_checks import fixture


class DistanceComparison(unittest.TestCase):
    def test_empty_and_reused_ranges(self):
        np.testing.assert_array_equal(
            expand_ranges(np.array([3, 99, 0]), np.array([2, 0, 1])), [3, 4, 0]
        )
        self.assertEqual(len(expand_ranges(np.array([], dtype=int), np.array([], dtype=int))), 0)

    def test_independent_distances_to_degenerate_segments(self):
        result = compare(fixture(), 4)
        # Cell centres (2,2) and (10,2), with every segment reduced to the origin.
        expected = np.sqrt(np.array([8, 8, 104], dtype="<f4"))
        self.assertEqual(result["distance_sha256"], hashlib.sha256(expected.tobytes()).hexdigest())
        self.assertEqual(result["original_roots"], 10)
        self.assertEqual(result["candidate_roots_including_empty_seeds"], 3)

    def test_zero_reference_seeds_preserve_the_original_sentinel(self):
        words = fixture()
        words[36] = words[40] = words[44] = 0
        result = compare(words, 4)
        expected = np.full(3, np.float32(1e30))
        self.assertEqual(result["distance_sha256"], hashlib.sha256(expected.tobytes()).hexdigest())
        self.assertEqual(result["original_roots"], 0)
        self.assertEqual(result["candidate_roots_including_empty_seeds"], 3)


if __name__ == "__main__":
    unittest.main()
