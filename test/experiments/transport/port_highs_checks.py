"""Independent analytic conic optima and delayed sparse-work counterexamples."""

import math
import unittest
from unittest.mock import patch

import numpy as np
from port_highs_cuts import fit_delayed
from scipy import sparse


def problem(target, gradient=None, equality=None, **kwargs):
    height = sparse.csr_matrix([[1.0, 0.0], [-1.0, 0.0], [0.0, 1.0], [0.0, -1.0]])
    targets = np.array([target[0], -target[0], target[1], -target[1]])
    gradient = (
        sparse.eye(2, format="csc") if gradient is None else sparse.csc_matrix(gradient)
    )
    equality = (
        sparse.csr_matrix((0, 2)) if equality is None else sparse.csr_matrix(equality)
    )
    before = gradient.data.copy()
    coefficients, report = fit_delayed(height, gradient, equality, targets, **kwargs)
    np.testing.assert_array_equal(gradient.data, before)
    return coefficients, report


class HighsCuts(unittest.TestCase):
    def test_diagonal_circle_has_known_minimax_answer(self):
        for selection in ("all", "support"):
            coefficients, report = problem([1.0, 1.0], selection=selection)
            self.assertIsNotNone(coefficients)
            np.testing.assert_allclose(
                coefficients, np.full(2, 0.99 / math.sqrt(2)), atol=2e-7, rtol=0
            )
            self.assertLessEqual(
                report["maximum_normalized_gradient_certificate"], 1 + 1e-7
            )
            self.assertLessEqual(report["iterations"], 100)
            self.assertGreater(report["cuts"], 0)

    def test_conic_boundary_is_not_replaced_by_inscribed_polygon(self):
        target = 0.99 * np.array([math.cos(math.pi / 16), math.sin(math.pi / 16)])
        self.assertGreater(
            float(target @ (target / 0.99)), 0.99 * math.cos(math.pi / 16)
        )
        coefficients, report = problem(target)
        np.testing.assert_allclose(coefficients, target, atol=1e-8, rtol=0)
        self.assertLess(report["maximum_target_deviation_m"], 1e-8)

    def test_rotated_anisotropic_norm_matches_quadratic_solution(self):
        angle = 0.73
        rotation = np.array(
            [[math.cos(angle), -math.sin(angle)], [math.sin(angle), math.cos(angle)]]
        )
        frame = rotation @ np.diag([1 / 0.08, 1 / 0.025])
        a, b, c = 1 / 0.08**2 + 1 / 0.025**2, -2 * (1 / 0.08 + 1 / 0.025), 2 - 0.99**2
        error = (-b - math.sqrt(b * b - 4 * a * c)) / (2 * a)
        coefficients, report = problem([0.08, 0.025], gradient=frame)
        np.testing.assert_allclose(
            coefficients, np.array([0.08, 0.025]) - error, atol=3e-7, rtol=0
        )
        self.assertTrue(report["valid"])

    def test_shared_contact_equality_is_retained(self):
        coefficients, report = problem([1.0, 0.0], equality=[[1.0, -1.0]])
        np.testing.assert_allclose(coefficients, [0.5, 0.5], atol=1e-8)
        self.assertLessEqual(report["point_contact_error"], 1e-7)

    def test_full_box_is_also_an_outer_relaxation(self):
        coefficients, report = problem([1.0, 1.0], initial_box="all")
        np.testing.assert_allclose(
            coefficients, [0.99 / math.sqrt(2)] * 2, atol=2e-7, rtol=0
        )
        self.assertEqual(report["initial_box_rows"], 2)

    def test_grouped_separation_keeps_all_original_cones_in_validation(self):
        gradient = np.vstack((np.eye(2), np.diag([1.0, 2.0])))
        expected = np.full(2, 0.99 / math.sqrt(5))
        for selection in ("all", "support"):
            coefficients, report = problem(
                [1.0, 1.0], gradient=gradient, selection=selection
            )
            np.testing.assert_allclose(coefficients, expected, atol=3e-7)
            self.assertGreaterEqual(report["work"]["full_cones_checked"], 2)

    def test_zero_gradient_can_keep_original_targets(self):
        coefficients, report = problem([1.0, 2.0], gradient=np.zeros((2, 2)))
        np.testing.assert_allclose(coefficients, [1.0, 2.0], atol=1e-9)
        self.assertTrue(report["valid"])

    def test_no_unchecked_result_at_zero_time_or_sparse_budget(self):
        for limits in ({"seconds": 0.0}, {"max_nonzeros": 1}):
            coefficients, report = problem([1.0, 1.0], **limits)
            self.assertIsNone(coefficients)
            self.assertFalse(report["valid"])
            self.assertTrue(report["reason"])

    def test_simplex_and_interior_point_satisfy_same_known_cone(self):
        for solver in ("simplex", "ipm"):
            coefficients, report = problem([1.0, 1.0], solver=solver)
            np.testing.assert_allclose(
                coefficients, [0.99 / math.sqrt(2)] * 2, atol=3e-7
            )
            self.assertGreater(report["work"]["submitted_api_bytes"], 0)

    def test_initial_constant_or_infeasible_epigraph_is_not_a_successful_fit(self):
        for status in ("HighsModelStatus.kTimeLimit", "HighsModelStatus.kOptimal"):
            with patch(
                "port_highs_work.SparseLP.solve", return_value=(np.zeros(3), 0, status)
            ):
                coefficients, report = problem([1.0, 1.0])
            self.assertIsNone(coefficients)
            self.assertFalse(report["valid"])


if __name__ == "__main__":
    unittest.main()
