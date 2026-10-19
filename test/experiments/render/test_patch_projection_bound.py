"""Independent surface-interior and projection checks for constructive bounds."""

import unittest
from fractions import Fraction
from unittest.mock import patch

import numpy as np
import trimesh
from patch_projection_bound import (
    directed_correspondence,
    projection_bound,
    root_interval,
)


class PatchProjectionBoundTests(unittest.TestCase):
    def setUp(self):
        self.triangle = np.array([[[0, 0, 100], [2, 0, 100], [0, 2, 100]]], np.float32)

    def test_outward_roots_include_exact_fraction_and_do_not_invent_zero_error(self):
        for value in (
            Fraction(0),
            Fraction(4),
            Fraction(1, 3),
            Fraction(1, 10**80),
            Fraction(10**80 + 1),
        ):
            low, high = root_interval(value)
            self.assertLessEqual(low * low, value)
            self.assertGreaterEqual(high * high, value)
        self.assertEqual(root_interval(Fraction(4)), (2, 2))

    def test_identical_triangle_has_exact_zero_bound(self):
        result = directed_correspondence(self.triangle, self.triangle)
        self.assertEqual(result["squared_m"], 0)
        pixels = projection_bound(
            result["squared_m"],
            self.triangle,
            self.triangle,
            np.zeros(3),
            6,
            100,
            20,
            20,
            1,
        )
        self.assertTrue(pixels["within"])
        self.assertEqual(pixels["upper_px"], 0)

    def test_coincident_vertices_do_not_certify_the_missing_surface_interior(self):
        original = self.triangle[0]
        tiny = np.array([[[0, 0, 0], [0.1, 0, 0], [0, 0.1, 0]]], np.float32)
        target = np.concatenate([tiny + corner for corner in original])
        result = directed_correspondence(self.triangle, target, depth=2)
        self.assertGreater(result["squared_m"], Fraction(1, 2))
        centre = self.triangle.mean(1)
        triangles = np.repeat(target, len(centre), axis=0)
        nearest = trimesh.triangles.closest_point(
            triangles, np.repeat(centre, len(target), axis=0)
        )
        self.assertGreater(np.linalg.norm(nearest - centre, axis=1).min(), 0.5)

    def test_dense_interior_distances_and_moving_eye_projections_stay_below_bounds(
        self,
    ):
        target = self.triangle + np.array([1, 0, 0], np.float32)
        result = directed_correspondence(self.triangle, target, depth=2)
        self.assertEqual(result["squared_m"], 1)
        report = projection_bound(
            result["squared_m"], self.triangle, target, np.zeros(3), 6, 100, 20, 20, 2
        )
        self.assertTrue(report["within"])
        rng = np.random.default_rng(867)
        weights = rng.dirichlet([1, 1, 1], 4096)
        points = weights @ self.triangle[0]
        # An independent known correspondence for the translated analytic pair.
        mapped = points + [1, 0, 0]
        eyes = rng.normal(size=points.shape)
        eyes *= 6 / np.linalg.norm(eyes, axis=1)[:, None]
        a, b = points - eyes, mapped - eyes
        inside = (abs(a[:, :2] / a[:, 2:]) <= 0.1).all(1)
        delta = 100 * np.linalg.norm(
            a[inside, :2] / a[inside, 2:] - b[inside, :2] / b[inside, 2:], axis=1
        )
        self.assertGreater(len(delta), 1000)
        self.assertLessEqual(delta.max(), float(report["upper_px"]))
        nearer = projection_bound(
            result["squared_m"], self.triangle, target, np.zeros(3), 0, 100, 20, 20, 2
        )
        self.assertGreaterEqual(report["upper_px"], nearer["upper_px"])

    def test_budget_is_checked_before_query_operand_allocation(self):
        source = np.repeat(self.triangle, 512, axis=0)
        with patch("trimesh.triangles.closest_point", side_effect=AssertionError):
            result = directed_correspondence(source, self.triangle, depth=2)
        self.assertIsNone(result["squared_m"])
        self.assertEqual(result["reason"], "pair_budget")

    def test_unreliable_proposals_and_near_projection_domain_are_unresolved(self):
        zero = np.zeros((1, 3, 3), np.float32)
        result = directed_correspondence(self.triangle, zero)
        self.assertIsNone(result["squared_m"])
        report = projection_bound(
            Fraction(1), self.triangle, self.triangle, [0, 0, 100], 6, 100, 20, 20, 1
        )
        self.assertFalse(report["within"])
        self.assertIsNone(report["upper_px"])
        with self.assertRaises(ValueError):
            directed_correspondence(self.triangle.astype(float), self.triangle)

    def test_pixel_gate_uses_rational_bound_without_an_acceptance_epsilon(self):
        target = self.triangle + np.array([1, 0, 0], np.float32)
        exact = projection_bound(
            Fraction(1), self.triangle, target, np.zeros(3), 6, 100, 20, 20, 1
        )["upper_px"]
        for allowed, expected in (
            (np.nextafter(float(exact), 0), False),
            (np.nextafter(float(exact), np.inf), True),
        ):
            result = projection_bound(
                Fraction(1), self.triangle, target, np.zeros(3), 6, 100, 20, 20, allowed
            )
            self.assertEqual(result["within"], expected)


if __name__ == "__main__":
    unittest.main()
