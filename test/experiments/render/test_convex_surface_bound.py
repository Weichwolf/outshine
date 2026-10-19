"""Continuous witnesses, diagonal changes, hidden interior defects and budgets."""

import unittest
from copy import deepcopy
from unittest.mock import patch

import numpy as np
from check_convex_surface_bound import check_bound
from convex_surface_bound import directed_bound
from far_product_cut import projection_error


class ConvexSurfaceTests(unittest.TestCase):
    def setUp(self):
        self.a = np.array(
            [[[0.0, 0, 0], [1, 0, 0], [1, 1, 0]], [[0, 0, 0], [1, 1, 0], [0, 1, 0]]]
        )
        self.b = np.array(
            [[[0.0, 0, 0], [1, 0, 0], [0, 1, 0]], [[1, 0, 0], [1, 1, 0], [0, 1, 0]]]
        )

    def test_changed_diagonal_has_zero_surface_error_despite_large_vertex_motion(self):
        for a, b in ((self.a, self.b), (self.b, self.a)):
            result = directed_bound(a, b, tolerance_m=0.00001)
            self.assertTrue(result.complete)
            self.assertLess(result.upper_m, 1e-12)
            audit = check_bound(a, b, result)
            self.assertGreater(audit["covering_leaf_patches"], 0)

    def test_exact_matches_ignore_orientation_but_not_coordinates(self):
        result = directed_bound(self.a, self.a[:, ::-1], tolerance_m=0, max_tests=0)
        self.assertTrue(result.complete)
        self.assertEqual(result.upper_m, 0)
        self.assertEqual(
            check_bound(self.a, self.a[:, ::-1], result)["exact_triangles"], 2
        )

    def test_parallel_offset_is_bounded_in_both_directions(self):
        shifted = self.b + np.array([0, 0, 0.125])
        for a, b in ((self.a, shifted), (shifted, self.a)):
            result = directed_bound(a, b, tolerance_m=0.126)
            self.assertTrue(result.complete)
            self.assertGreaterEqual(result.upper_m, 0.125)
            check_bound(a, b, result)

    def test_vertex_only_false_positive_at_interior_hole_does_not_pass(self):
        # The outer triangle vertices all occur in target triangles. Its centre is absent.
        a = np.array([[[0.0, 0, 0], [3, 0, 0], [0, 3, 0]]])
        b = np.array(
            [
                [[0.0, 0, 0], [0.1, 0, 0], [0, 0.1, 0]],
                [[3, 0, 0], [2.9, 0, 0], [3, 0.1, 0]],
                [[0, 3, 0], [0.1, 3, 0], [0, 2.9, 0]],
            ]
        )
        result = directed_bound(a, b, tolerance_m=0.01, max_tests=20000, max_depth=8)
        self.assertFalse(result.complete)
        self.assertIsNone(result.upper_m)

    def test_budget_and_depth_fail_closed(self):
        for kwargs in ({"max_tests": 0}, {"max_depth": 0}):
            result = directed_bound(self.a, self.b, tolerance_m=0.01, **kwargs)
            self.assertFalse(result.complete)
            self.assertIsNone(result.upper_m)
            with self.assertRaises(ValueError):
                check_bound(self.a, self.b, result)

    def test_missing_leaf_nonconvex_weight_and_false_bound_are_detected(self):
        result = directed_bound(self.a, self.b, tolerance_m=0.01)
        bad = deepcopy(result)
        bad.leaves.pop()
        with self.assertRaises(AssertionError):
            check_bound(self.a, self.b, bad)
        bad = deepcopy(result)
        bad.leaves[0][3][0, 0] = -1
        with self.assertRaises(AssertionError):
            check_bound(self.a, self.b, bad)
        bad = deepcopy(result)
        bad.upper_m = 0
        with self.assertRaises(AssertionError):
            check_bound(self.a, self.b, bad)

    def test_unsigned_weight_overflow_cannot_fake_convexity(self):
        result = directed_bound(self.a, self.b, tolerance_m=0.01)
        i, path, j, w, upper = result.leaves[0]
        forged = w.astype(np.uint64)
        forged[0] = [2**64 - 1, 1, 1 << 30]
        result.leaves[0] = (i, path, j, forged, upper)
        with self.assertRaises(AssertionError):
            check_bound(self.a, self.b, result)

    def test_empty_sets_and_invalid_inputs(self):
        empty = np.empty((0, 3, 3))
        self.assertTrue(directed_bound(empty, self.a, tolerance_m=0).complete)
        self.assertFalse(directed_bound(self.a, empty, tolerance_m=1).complete)
        with self.assertRaises(ValueError):
            directed_bound(self.a * float("nan"), self.b, tolerance_m=1)

    def test_existing_reference_error_must_be_added_before_lod_acceptance(self):
        box = np.array([[0.0, 0, 800], [10, 10, 810]])
        observer = np.array([[-64.0, -64, -64], [64, 64, 64]])
        profile = {"focal_px": 1044.0, "width": 1280, "height": 720}
        self.assertLess(
            projection_error(box, observer, displacement_m=0.4, **profile), 1
        )
        total = np.nextafter(0.25 + 0.4, np.inf)
        self.assertGreater(
            projection_error(box, observer, displacement_m=total, **profile), 1
        )

    def test_failed_nearest_point_proposal_cannot_create_a_false_bound(self):
        target = self.b + np.array([0, 0, 0.25])
        with patch(
            "convex_surface_bound.trimesh.triangles.points_to_barycentric",
            side_effect=lambda triangles, points: np.full((len(triangles), 3), np.nan),
        ):
            result = directed_bound(self.a, target, tolerance_m=0.01, max_tests=5000)
        self.assertFalse(result.complete)
        self.assertIsNone(result.upper_m)


if __name__ == "__main__":
    unittest.main()
