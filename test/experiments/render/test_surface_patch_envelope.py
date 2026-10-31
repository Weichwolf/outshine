"""Analytic complete-domain counterexamples, including missing interior support."""

import unittest
from fractions import Fraction as F

import numpy as np
from surface_patch_envelope import compare_envelope


def square(first=0):
    p = np.array([[first, 0, 0], [1, 0, 0], [1, 1, 0], [first, 1, 0]], np.float32)
    return p, p[:, :2].copy(), np.array([[0, 1, 2], [0, 2, 3]], np.uint32)


class SurfaceEnvelope(unittest.TestCase):
    def test_shrunk_boundary_has_exact_two_sided_geometric_and_field_bound(self):
        result = compare_envelope(square(), square(0.125), axes=(0, 1))
        self.assertEqual(result["position_error_squared"], F(1, 64))
        self.assertEqual(result["field_error_absolute"][0], F(1, 8))
        # One target triangle per convex cell may also move y; this is an upper
        # bound, not the globally closest correspondence at every interior point.
        self.assertLessEqual(result["field_error_absolute"][1], F(1, 8))
        self.assertEqual(result["original_outside_candidate"]["chart_area"], F(1, 8))
        self.assertEqual(result["candidate_outside_original"]["chart_area"], 0)

    def test_subnormal_boundary_displacement_is_not_dropped(self):
        delta = np.nextafter(np.float32(0), np.float32(1))
        result = compare_envelope(square(), square(delta), axes=(0, 1))
        self.assertEqual(result["position_error_squared"], F(2) ** -298)

    def test_changed_diagonal_with_equal_domains_keeps_zero_bound(self):
        old = square()
        new = (*old[:2], np.array([[0, 1, 3], [1, 2, 3]], np.uint32))
        result = compare_envelope(old, new, axes=(0, 1))
        self.assertEqual(result["position_error_squared"], 0)
        self.assertEqual(result["original_outside_candidate"]["cells"], 0)
        self.assertEqual(result["candidate_outside_original"]["cells"], 0)

    def test_hole_between_original_vertices_is_not_missed(self):
        p = np.array(
            [
                [0, 0, 0],
                [3, 0, 0],
                [3, 3, 0],
                [0, 3, 0],
                [1, 1, 0],
                [2, 1, 0],
                [2, 2, 0],
                [1, 2, 0],
            ],
            np.float32,
        )
        ring = np.array(
            [
                [0, 1, 5],
                [0, 5, 4],
                [1, 2, 6],
                [1, 6, 5],
                [2, 3, 7],
                [2, 7, 6],
                [3, 0, 4],
                [3, 4, 7],
            ],
            np.uint32,
        )
        full = p, p[:, :2], np.array([[0, 1, 2], [0, 2, 3]], np.uint32)
        result = compare_envelope(full, (p, p[:, :2], ring), axes=(0, 1))
        self.assertGreaterEqual(result["position_error_squared"], F(1, 4))
        self.assertEqual(result["original_outside_candidate"]["chart_area"], 1)
        self.assertEqual(result["candidate_outside_original"]["chart_area"], 0)

    def test_work_exhaustion_cannot_return_a_valid_partial_bound(self):
        with self.assertRaisesRegex(ValueError, "capacity exceeded"):
            compare_envelope(square(), square(0.125), axes=(0, 1), max_work=1)


if __name__ == "__main__":
    unittest.main()
