"""Independent finite-sample and exact-rational depth-order counterexamples."""

import math
import unittest
from fractions import Fraction
from itertools import product

import numpy as np
from field_composition import (
    OriginalBounds,
    compose_points,
    depth_order,
    forward_interval,
)
from field_view_integrals import box_integrals, summed_field


class CompositionTests(unittest.TestCase):
    def test_covariance_is_preserved_only_before_reduction(self):
        front = np.array([[[1.0, 1, 1, 1], [1, 1, 1, 1], [0, 0, 0, 0], [0, 0, 0, 0]]])
        back = np.array([[[1.0, 1, 1, 1], [1, 1, 1, 1], [0, 0, 0, 1], [0, 0, 0, 1]]])
        combined = compose_points(front, back)
        correct = combined.mean((0, 1))
        wrong = front.mean((0, 1)) + (1 - front[:, :, 3].mean()) * back.mean((0, 1))
        np.testing.assert_array_equal(correct, [0.5, 0.5, 0.5, 1])
        np.testing.assert_array_equal(wrong, [0.75, 0.75, 0.75, 1])
        field = summed_field(combined, dtype=np.float64)
        actual = box_integrals(field, np.array([[0.0, 0]]), np.array([[4.0, 1]])) / 4
        np.testing.assert_array_equal(actual[0], correct)

    def test_joint_coverage_keeps_actual_holes_in_both_groups(self):
        front = np.array([[[3.0, 2, 1, 1], [0, 0, 0, 0]]])
        same_mask = np.array([[[0.2, 0.4, 0.6, 1], [0, 0, 0, 0]]])
        opposite_mask = same_mask[:, ::-1]
        np.testing.assert_array_equal(
            compose_points(front, same_mask).mean((0, 1)), [1.5, 1, 0.5, 0.5]
        )
        np.testing.assert_array_equal(
            compose_points(front, opposite_mask).mean((0, 1)), [1.6, 1.2, 0.8, 1]
        )
        self.assertEqual(same_mask[:, :, 3].mean(), opposite_mask[:, :, 3].mean())

    def test_binary_hdr_composition_preserves_dtype_inputs_and_exact_owner(self):
        rng = np.random.default_rng(1715)
        for dtype in (np.float32, np.float64):
            a, b = [rng.uniform(0, 100, (7, 13, 4)).astype(dtype) for _ in range(2)]
            for image in (a, b):
                image[:, :, 3] = rng.integers(0, 2, (7, 13))
                image[image[:, :, 3] == 0] = 0
            before = [image.copy() for image in (a, b)]
            expected = np.where(a[:, :, 3:4] == 1, a, b)
            actual = compose_points(a, b)
            self.assertEqual(actual.dtype, dtype)
            np.testing.assert_array_equal(actual, expected)
            for image, old in zip((a, b), before, strict=True):
                np.testing.assert_array_equal(image, old)
            self.assertFalse(np.shares_memory(actual, a) or np.shares_memory(actual, b))

    def test_filtered_coverage_is_not_mislabelled_as_point_samples(self):
        a = np.zeros((1, 1, 4), np.float32)
        for invalid in (
            [1, 1, 1, 0.5],
            [1, 0, 0, 0],
            [math.nan, 0, 0, 1],
            [-1, 0, 0, 1],
        ):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                compose_points(a, np.array([[invalid]], np.float32))
        with self.assertRaises(ValueError):
            compose_points(a, a.astype(np.float64))
        with self.assertRaises(ValueError):
            compose_points(a, np.zeros((2, 1, 4), np.float32))
        with self.assertRaises(ValueError):
            compose_points(np.zeros((2, 2, 4), np.float32), a, texels_most=3)


class OriginalHullTests(unittest.TestCase):
    def test_bounds_enclose_exact_rational_interior_and_corners(self):
        bounds = OriginalBounds(
            (-1e9, -9, -1e-200), (1e9, 13, 1e-200), "complete-originals"
        )
        eye, forward = (3e8, -2e8, 1e8), (0.5, math.sqrt(0.5), 0.5)
        low, high = map(Fraction, forward_interval(bounds, eye, forward))
        points = [*product(*zip(bounds.lower, bounds.upper, strict=True)), (3, 2, 0)]
        for p in points:
            actual = sum(
                (Fraction(x) - Fraction(e)) * Fraction(f)
                for x, e, f in zip(p, eye, forward, strict=True)
            )
            self.assertLessEqual(low, actual)
            self.assertGreaterEqual(high, actual)

    def test_source_rear_surface_cannot_be_replaced_by_first_hit_range(self):
        front = OriginalBounds((-1, -1, 10), (1, 1, 30), "front-complete")
        back = OriginalBounds((-1, -1, 20), (1, 1, 21), "neighbour-complete")
        self.assertEqual(
            depth_order(front, back, (0, 0, 0), (0, 0, 1)),
            ("originals", "complete_hulls_overlap"),
        )
        # A range of only the front object's visible first hits falsely separates.
        first_hit = OriginalBounds((-1, -1, 10), (1, 1, 11), "not-a-whole-source-hull")
        self.assertEqual(
            depth_order(first_hit, back, (0, 0, 0), (0, 0, 1))[0], "front_before_back"
        )

    def test_strict_order_reverses_but_touch_and_eye_crossing_stay_originals(self):
        a = OriginalBounds((-1, -1, 10), (1, 1, 12), "a")
        b = OriginalBounds((-1, -1, 20), (1, 1, 21), "b")
        self.assertEqual(
            depth_order(a, b, (0, 0, 0), (0, 0, 1))[0], "front_before_back"
        )
        self.assertEqual(
            depth_order(b, a, (0, 0, 0), (0, 0, 1))[0], "back_before_front"
        )
        self.assertEqual(depth_order(a, a, (0, 0, 0), (0, 0, 1))[0], "originals")
        self.assertEqual(
            depth_order(a, b, (0, 0, 11), (0, 0, 1)), ("originals", "hull_crosses_eye")
        )

    def test_bounds_own_coordinates_and_invalid_inputs_are_not_admitted(self):
        values = [0, 0, 1]
        bounds = OriginalBounds(values, (1, 1, 2), "a")
        values[0] = -50
        self.assertEqual(bounds.lower, (0, 0, 1))
        for lower, binding in (
            ((2, 0, 0), "a"),
            ((math.nan, 0, 0), "a"),
            ((0, 0, 0), ""),
        ):
            with (
                self.subTest(lower=lower, binding=binding),
                self.assertRaises(ValueError),
            ):
                OriginalBounds(lower, (1, 1, 1), binding)
        with self.assertRaises(ValueError):
            forward_interval(bounds, (0, 0, 0), (0, 0, 2))


if __name__ == "__main__":
    unittest.main()
