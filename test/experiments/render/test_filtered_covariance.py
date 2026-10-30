"""A real covariance bound is useful only with range, weights and ordering."""

import unittest

import numpy as np
from compare_filtered_covariance import adaptive, background, filtered, reduced


class FilteredCovarianceTests(unittest.TestCase):
    def test_binary_correlation_attains_bound_despite_same_background_mean(self):
        a = np.zeros((4, 4, 4))
        a[:2, :, 3] = 1
        first = np.zeros_like(a)
        first[:2, :, :3] = 1
        first[..., 3] = 1
        second = first[::-1].copy()
        approximate, bound = filtered(reduced(a), *background(first))
        other, other_bound = filtered(reduced(a), *background(second))
        np.testing.assert_array_equal(approximate, other)
        np.testing.assert_array_equal(bound, other_bound)
        for b in (first, second):
            actual = reduced(a + (1 - a[..., 3:]) * b)
            np.testing.assert_array_equal(abs(approximate - actual), bound)
        np.testing.assert_array_equal(bound[0, 0, :3], [0.25] * 3)

    def test_constant_background_has_zero_covariance_error(self):
        a = np.zeros((4, 4, 4))
        a[:3, :, 3] = 1
        b = np.broadcast_to([3, 2, 1, 1], a.shape).copy()
        approximate, bound = filtered(reduced(a), *background(b))
        np.testing.assert_array_equal(bound, np.zeros_like(bound))
        np.testing.assert_array_equal(approximate, reduced(a + (1 - a[..., 3:]) * b))

    def test_adaptive_ranges_preserve_all_outputs_including_clear_and_opaque(self):
        a = np.zeros((4, 12, 4))
        a[:, :4, 3] = 1
        a[:2, 4:8, 3] = 1
        b = np.arange(a.size, dtype=float).reshape(a.shape) / a.size
        b[..., 3] = 1
        expected = filtered(reduced(a), *background(b))
        actual = adaptive(reduced(a), b)
        for value, target in zip(actual, expected, strict=True):
            np.testing.assert_array_equal(value, target)


if __name__ == "__main__":
    unittest.main()
