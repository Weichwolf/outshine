"""Page boundaries and missing data bound reuse independently of texture interpolation."""

import unittest

import numpy as np
from terrain_litter_page_filter import lookup_counts


class LookupCounts(unittest.TestCase):
    def test_single_point_cannot_save_a_lookup(self):
        q = np.array([[1, -1, 0, 0, 0, 0]], dtype=float)
        np.testing.assert_array_equal(lookup_counts(q, 128), [[1, 1]])

    def test_eight_taps_share_two_mip_pages(self):
        q = np.array([[4, 4, 0.9, 0, 0, 0.1]], dtype=float)
        np.testing.assert_array_equal(lookup_counts(q, 128), [[16, 2]])

    def test_negative_page_boundary_requires_both_sides(self):
        q = np.array([[0, -4, 0.9, 0, 0, 0.1]], dtype=float)
        np.testing.assert_array_equal(lookup_counts(q, 128), [[16, 4]])

    def test_missing_first_page_and_faded_samples_do_no_extra_work(self):
        q = np.array([[4, 4, 0.9, 0, 0, 0.1], [1e8, -1e8, 2, 0, 0, 0]])
        np.testing.assert_array_equal(lookup_counts(q, 128, set()), [[1, 1], [0, 0]])


if __name__ == "__main__":
    unittest.main()
