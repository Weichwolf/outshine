"""Whole-interval certificates and coupled fixed-contact profiles (#1539)."""

import unittest
from unittest.mock import patch

import numpy as np
from road_bank_profile import BankLimits, prior_controls, quadratic_minimum, residual
from road_banked_surface import BankedSurface
from road_joint_profile import height_prior_lower_bound, plan_joint
from road_surface_frames import differentials
from road_surface_model import Curve
from scipy.optimize import OptimizeResult
from test_road_surface_lod import recipe


def height_bump():
    source = recipe(knots=3)
    source["seams"][1][3] = 0.2
    return source


class JointProfileTests(unittest.TestCase):
    def test_quadratic_interior_minimum_without_sampling(self):
        np.testing.assert_allclose(
            quadratic_minimum(np.array([[1, -1, 1], [0, -1, 0], [0, 1, 0], [2, 2, 2]])),
            [0, -0.5, 0, 2],
            atol=0,
        )

    def test_valid_prior_is_retained_when_convex_coefficients_are_too_strict(self):
        source = recipe(knots=3)
        source["seams"][1][9] = np.arctan(0.05)
        curve = Curve(source)
        result, note = plan_joint(curve, BankLimits(0.04, 0.1, 0), 0.1)
        self.assertIsNotNone(result, note)
        self.assertEqual(note["selected"], "unchanged_quadratic")
        np.testing.assert_array_equal(result[0], curve.height)
        np.testing.assert_array_equal(result[1], prior_controls(curve))

    def test_real_height_change_keeps_ends_axis_and_entire_grade(self):
        source = height_bump()
        curve = Curve(source)
        limits = BankLimits(0.02, 0.1, 0.04)
        result, note = plan_joint(curve, limits, 0.2)
        self.assertIsNotNone(result, note)
        self.assertEqual(note["selected"], "coupled")
        h, q = result
        fitted = BankedSurface(source, q, height_controls=h)
        s = np.linspace(0, 10, 1001)
        old, _ = differentials(curve, s)
        new, _ = differentials(fitted, s)
        self.assertGreater(abs(old[:, :, 2]).max(), limits.grade)
        self.assertLessEqual(abs(new[:, :, 2]).max(), limits.grade + 1e-9)
        self.assertLessEqual(abs(h - curve.height).max(), 0.2 + 1e-9)
        np.testing.assert_array_equal(curve.at(s)[:, :, :2], fitted.at(s)[:, :, :2])
        np.testing.assert_array_equal(
            curve.at(np.array([0, 10])), fitted.at(np.array([0, 10]))
        )
        self.assertAlmostEqual(
            3 * (h[0, 3] - h[0, 2]) / 5, 3 * (h[1, 1] - h[1, 0]) / 5, places=12
        )

    def test_height_budget_lower_bound_stops_before_joint_solver(self):
        curve = Curve(height_bump())
        limits = BankLimits(0.02, 0.1, 0.04)
        self.assertAlmostEqual(height_prior_lower_bound(curve, limits), 0.1)
        with patch(
            "road_joint_profile.linprog",
            side_effect=AssertionError("unnecessary solver"),
        ):
            result, note = plan_joint(curve, limits, 0.05)
        self.assertIsNone(result)
        self.assertEqual(note["reason"], "height_prior_budget")

    def test_fixed_one_span_family_is_not_mistaken_for_freedom(self):
        source = recipe()
        for pose in (*source["seams"], source["intervals"][0]["middle"]):
            s = pose[0]
            pose[3] = 0.068 * s * s - 0.0034 * s * s * s
            pose[7] = 0.136 * s - 0.0102 * s * s
        result, note = plan_joint(Curve(source), BankLimits(0.35, 0.12, 0.025), 0.5)
        self.assertIsNone(result)
        self.assertEqual(note["reason"], "fixed_cubic_family")
        self.assertEqual(note["coupled_lp_calls"], 0)

    def test_near_fold_residual_is_physical_for_both_certificates(self):
        curve = Curve(recipe(curvature=0.4999999999, knots=4))
        prior = prior_controls(curve)
        candidate = prior.copy()
        candidate[1, 1] = 2e-10
        for mode in ("bernstein", "quadratic"):
            self.assertGreater(
                residual(
                    curve,
                    BankLimits(0.35, 0.12, 0.025),
                    prior,
                    candidate,
                    grade_mode=mode,
                ),
                0.24,
            )

    def test_incomplete_coupled_solver_cannot_admit_its_candidate(self):
        fake = OptimizeResult(success=False, status=1, nit=0, x=np.zeros(17))
        with patch("road_joint_profile.linprog", return_value=fake):
            result, note = plan_joint(
                Curve(height_bump()), BankLimits(0.02, 0.1, 0.04), 0.2, method="coupled"
            )
        self.assertIsNone(result)
        self.assertEqual(note["reason"], "coupled_incomplete")

    def test_adapter_height_derivative_matches_position_differences(self):
        source = recipe(curvature=0.03, height=0.001, bank_rate=0.003, knots=3)
        curve = Curve(source)
        height = curve.height.copy()
        height[0, 2] += 0.01
        fitted = BankedSurface(source, prior_controls(curve), height_controls=height)
        s = np.array([0.2, 2.7, 6.1, 9.8])
        epsilon = 1e-5
        actual, _ = differentials(fitted, s)
        observed = (fitted.at(s + epsilon) - fitted.at(s - epsilon)) / (2 * epsilon)
        np.testing.assert_allclose(actual, observed, atol=2e-10, rtol=1e-9)
        for allowance in (-1, np.nan, 1001):
            with self.assertRaises(ValueError):
                plan_joint(curve, BankLimits(0.35, 0.12, 0.025), allowance)


if __name__ == "__main__":
    unittest.main()
