"""Whole-face, objective and complete hardware-contact counterexamples."""

import unittest

import numpy as np
import shapely
from camera_height_inputs import fixed_bed_facets, lumped_weights
from camera_height_solve import audit, fit


class CameraHeights(unittest.TestCase):
    def setUp(self):
        self.xy = np.array([[0.0, 0.0], [20, 0], [20, 20], [0, 20]])
        self.faces = np.array([[0, 1, 2], [0, 2, 3]])
        self.frames = np.broadcast_to(np.eye(2) / 0.025, (2, 2, 2))

    def arrays(self, target, fixed=()):
        return {
            "xy": self.xy,
            "faces": self.faces,
            "targets": np.array(target),
            "frames": self.frames,
            "fixed": np.array(fixed, dtype=int),
            "weights": lumped_weights(self.xy, self.faces),
        }

    def test_identical_affine_prior_and_contacts(self):
        data = self.arrays([5.0, 5.1, 5.3, 5.2], (0,))
        for mode in ("minimax", "least_squares"):
            z, report = fit(data, 5, mode)
            self.assertTrue(report["valid"])
            np.testing.assert_allclose(z, data["targets"], atol=1e-6)
            np.testing.assert_array_equal(data["targets"], [5.0, 5.1, 5.3, 5.2])

    def test_entire_triangle_grade_not_only_edges(self):
        data = self.arrays([0.0, 0.5, 1.0, 0.5])
        check = audit(
            self.xy,
            self.faces,
            data["targets"],
            self.frames,
            data["targets"],
            data["fixed"],
            0,
            data["weights"],
        )
        self.assertFalse(check["valid"])
        for mode in ("minimax", "least_squares"):
            z, report = fit(data, 0, mode)
            self.assertIsNotNone(z)
            self.assertTrue(report["independent_check"]["valid"])

    def test_fixed_triangle_is_whole_hardware_support(self):
        footprint = np.array([shapely.box(2, 2, 18, 18)], object)
        fixed, report = fixed_bed_facets(self.xy, self.faces, footprint)
        np.testing.assert_array_equal(fixed, [0, 1, 2, 3])
        self.assertEqual(report["fixed_facets"], 2)
        for mode in ("minimax", "least_squares"):
            z, result = fit(self.arrays([8.0, 9.0, 10.0, 11.0], fixed), 5, mode)
            self.assertTrue(result["valid"])
            np.testing.assert_allclose(z, 5, atol=1e-7)

    def test_partial_footprint_and_pair_budget_rejected(self):
        outside = np.array([shapely.box(19, 19, 21, 21)], object)
        with self.assertRaisesRegex(ValueError, "whole verified"):
            fixed_bed_facets(self.xy, self.faces, outside)
        inside = np.array([shapely.box(2, 2, 18, 18)], object)
        with self.assertRaisesRegex(ValueError, "pair_budget"):
            fixed_bed_facets(self.xy, self.faces, inside, maximum_pairs=1)

    def test_lumped_weight_integrates_constant_area(self):
        weights = lumped_weights(self.xy, self.faces)
        np.testing.assert_allclose(weights, [4 / 3, 2 / 3, 4 / 3, 2 / 3])
        with self.assertRaises(ValueError):
            lumped_weights(self.xy, np.array([[0, 0, 1]]))

    def test_quadratic_triangle_matches_analytic_projection(self):
        xy = np.array([[0.0, 0.0], [20.0, 0.0], [0.0, 20.0]])
        faces = np.array([[0, 1, 2]])
        data = {
            "xy": xy,
            "faces": faces,
            "targets": np.array([0.0, 0.8, 0.8]),
            "frames": np.array([np.eye(2) / 0.025]),
            "fixed": np.array([], dtype=int),
            "weights": lumped_weights(xy, faces),
        }
        delta = (0.8 - 0.5 / np.sqrt(2)) / 3
        expected = [2 * delta, 0.8 - delta, 0.8 - delta]
        heights, report = fit(data, 0, "least_squares")
        self.assertTrue(report["valid"])
        np.testing.assert_allclose(heights, expected, atol=1e-5)

    def test_thin_face_and_tiny_solver_budget_keep_whole_gate(self):
        data = self.arrays([0.0, 2.0, 3.0, 4.0], (0,))
        data["xy"] = self.xy * [1, 1e-4]
        data["weights"] = lumped_weights(data["xy"], data["faces"])
        for mode in ("minimax", "least_squares"):
            heights, report = fit(data, 0, mode, seconds=1e-12)
            if heights is not None:
                self.assertTrue(report["independent_check"]["valid"])
                self.assertLessEqual(abs(heights[0]), 1e-7)
            else:
                self.assertFalse(report["valid"])


if __name__ == "__main__":
    unittest.main()
