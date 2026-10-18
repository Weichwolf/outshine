"""Analytic counterexamples for storage, slopes, pins and independent datums."""

import unittest

import numpy as np
from profile_precision import encode, gradients, inspect


class ProfilePrecision(unittest.TestCase):
    def setUp(self):
        self.faces = np.array([[0, 1, 2]])
        self.points = np.array([[0, 0, 500], [1, 0, 499.9988], [0, 1, 500.0024]])
        self.flow = np.array([[2, 1]]) / np.sqrt(5)
        self.rows = np.empty((0, 5))

    def check_points(self, points, **kwargs):
        return inspect(self.points, points, self.faces, self.flow, self.rows, **kwargs)

    def test_analytic_saturated_direction_is_feasible_before_rounding(self):
        np.testing.assert_allclose(
            gradients(self.points, self.faces), [[-0.0012, 0.0024]], atol=1e-13
        )
        self.assertTrue(
            self.check_points(self.points)["passes_finite_precision_checks"]
        )

    def test_height_rounding_can_create_uphill_on_an_oblique_direction(self):
        points, _ = encode(self.points, "absolute32", np.zeros(3))
        result = self.check_points(points)
        self.assertFalse(result["passes_finite_precision_checks"])
        self.assertGreater(result["maximum_grade_violation_m_per_m"], 1e-5)

    def test_subtracting_datum_before_cast_preserves_this_analytic_face(self):
        points, _ = encode(self.points, "local32", np.array([0, 0, 500]))
        self.assertTrue(self.check_points(points)["passes_finite_precision_checks"])
        # Adding the ASL datum in Float32 discards the gained precision again.
        points = points.astype(np.float32).astype(float)
        self.assertFalse(self.check_points(points)["passes_finite_precision_checks"])

    def test_large_xy_translation_can_collapse_a_valid_face(self):
        original = self.points + [2**26, 2**26, 0]
        points, _ = encode(original, "absolute32", np.zeros(3))
        report = inspect(original, points, self.faces, self.flow, self.rows)
        self.assertEqual(report["reason"], "decoded_face_topology_changed")
        self.assertEqual(report["collapsed_faces"], 1)

    def test_locally_subtracted_translation_preserves_that_face(self):
        original = self.points + [2**26, 2**26, 0]
        points, _ = encode(original, "local32", np.array([2**26, 2**26, 500]))
        self.assertTrue(
            inspect(original, points, self.faces, self.flow, self.rows)[
                "passes_finite_precision_checks"
            ]
        )

    def test_topology_reversal_is_rejected_before_gradients(self):
        points = self.points.copy()
        points[[1, 2], :2] = points[[2, 1], :2]
        self.assertEqual(self.check_points(points)["reversed_faces"], 1)

    def test_a_hard_pin_is_not_relaxed_by_a_valid_gradient(self):
        points = self.points.copy()
        points[:, 2] += 0.001
        report = self.check_points(points, pins={0: 500.0})
        self.assertGreater(report["maximum_pin_error_m"], 0.0009)
        self.assertFalse(report["passes_finite_precision_checks"])

    def test_additional_source_direction_is_checked_independently(self):
        rows = np.array([[0, 0, 1, -0.1, 0]])
        report = inspect(self.points, self.points, self.faces, self.flow, rows)
        self.assertEqual(report["face_violations"], 0)
        self.assertEqual(report["source_face_violations"], 1)

    def test_separate_datums_do_not_preserve_a_shared_seam_height(self):
        seam = np.array([[0.0, 0.0, 500.000123]])
        first, _ = encode(seam, "local32", np.array([0, 0, 500]))
        second, _ = encode(seam, "local32", np.array([0, 0, 400]))
        self.assertGreater(abs(first[0, 2] - second[0, 2]), 1e-7)

    def test_unrepresentable_float32_has_an_explicit_reason(self):
        original = self.points.copy()
        original[1, 0] = 1e40
        points, _ = encode(original, "absolute32", np.zeros(3))
        self.assertEqual(
            inspect(original, points, self.faces, self.flow, self.rows)["reason"],
            "nonfinite_decoded_coordinates",
        )

    def test_mixed_representations_preserve_their_float64_axes_exactly(self):
        origin = np.array([0.123456789, -0.987654321, 499.123456789])
        xy64, _ = encode(self.points, "xy64-z32", origin)
        z64, _ = encode(self.points, "xy32-z64", origin)
        np.testing.assert_array_equal(xy64[:, :2], self.points[:, :2])
        np.testing.assert_array_equal(z64[:, 2], self.points[:, 2])

    def test_split_storage_retains_this_face_but_saves_no_coordinate_bytes(self):
        points, payload = encode(self.points, "split32", np.array([0, 0, 500]))
        self.assertTrue(self.check_points(points)["passes_finite_precision_checks"])
        self.assertGreater(payload, self.points.nbytes)


if __name__ == "__main__":
    unittest.main()
