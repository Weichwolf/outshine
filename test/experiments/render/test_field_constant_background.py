"""Linearity with a truly constant ordered background, and its necessary limits."""

import unittest
from fractions import Fraction

import numpy as np
from field_external_depth import over_constant_background


class ConstantBackgroundTests(unittest.TestCase):
    def test_opaque_and_transmitting_background_match_exact_rational_means(self):
        front = np.array(
            [[[3.0, 1, 0, 1], [0, 0, 0, 0], [0.5, 0.25, 0, 0.5], [0, 0, 0, 0]]]
        )
        for background in (
            np.array([2.0, 1, 0, 1]),
            np.array([0.25, 0.125, 0, 0.5]),
            np.zeros(4),
        ):
            mean = front.mean(1, keepdims=True)
            result = over_constant_background(mean, background)
            exact = [
                sum(
                    Fraction(float(v[i]))
                    + (1 - Fraction(float(v[3]))) * Fraction(float(background[i]))
                    for v in front[0]
                )
                / 4
                for i in range(4)
            ]
            np.testing.assert_array_equal(result[0, 0], list(map(float, exact)))
            np.testing.assert_array_equal(mean, front.mean(1, keepdims=True))
            self.assertFalse(
                np.shares_memory(result, mean) or np.shares_memory(result, background)
            )

    def test_same_background_mean_does_not_imply_constant_samples(self):
        front = np.array([[[2.0, 0, 0, 1], [0, 0, 0, 0]]])
        a = np.array([[[0.0, 1, 0, 1], [0, 0, 0, 0]]])
        b = a[:, ::-1]
        np.testing.assert_array_equal(a.mean(1), b.mean(1))
        means = [(front + (1 - front[:, :, 3:]) * back).mean(1) for back in (a, b)]
        self.assertFalse(np.array_equal(*means))
        wrong = over_constant_background(front.mean(1, keepdims=True), a.mean((0, 1)))
        self.assertFalse(np.array_equal(wrong[:, 0], means[0]))
        self.assertFalse(np.array_equal(wrong[:, 0], means[1]))

    def test_numeric_profiles_and_invalid_data_remain_explicit(self):
        for dtype in (np.float32, np.float64):
            mean = np.array([[[3, 1, 2, 0.5]]], dtype=dtype)
            background = np.array([2, 4, 1, 1], dtype=dtype)
            actual = over_constant_background(mean, background)
            np.testing.assert_array_equal(actual, [[[4, 3, 2.5, 1]]])
            self.assertEqual(actual.dtype, dtype)
            for bad in (
                np.array([1, 0, 0, 0], dtype),
                np.full(4, np.nan, dtype),
                np.ones(3, dtype),
                np.array([0, 0, 0, 1.1], dtype),
            ):
                with self.assertRaises(ValueError):
                    over_constant_background(mean, bad)
        with self.assertRaises(ValueError):
            over_constant_background(np.zeros((2, 2, 4)), np.zeros(4), pixels_most=3)


if __name__ == "__main__":
    unittest.main()
