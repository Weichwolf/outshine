"""Whole-width, fixed-contact and incomplete-solver controls for #1533."""

import unittest
from unittest.mock import patch

import numpy as np
from road_bank_profile import BankLimits, plan_bank, prior_controls
from road_banked_surface import BankedSurface
from road_surface_frames import differentials
from road_surface_lod import prepare
from road_surface_model import Curve
from scipy.optimize import OptimizeResult
from test_road_surface_lod import recipe


def bump():
    source = recipe(knots=3)
    source["seams"][1][9] = np.arctan(0.05)
    return source


class BankProfileTests(unittest.TestCase):
    def test_inner_bank_adjustment_preserves_outer_sections_and_c1(self):
        source = bump()
        limits = BankLimits(0.02, 0.1, 0.04)
        curve = Curve(source)
        control, note = plan_bank(curve, limits)
        self.assertIsNotNone(control, note)
        self.assertEqual(note["selected"], "linear")
        fitted = BankedSurface(source, control)
        stations = np.linspace(0, 10, 1001)
        old, _ = differentials(curve, stations)
        new, _ = differentials(fitted, stations)
        self.assertGreater(np.max(abs(old[:, :, 2])), limits.grade)
        self.assertLessEqual(np.max(abs(new[:, :, 2])), limits.grade + 1e-10)
        np.testing.assert_array_equal(
            curve.at(stations)[:, :, :2], fitted.at(stations)[:, :, :2]
        )
        np.testing.assert_allclose(
            curve.at(np.array([0, 10])), fitted.at(np.array([0, 10])), atol=1e-14
        )
        left = fitted.crossfall(np.array([0]), np.array([1.0]), derivative=1)
        right = fitted.crossfall(np.array([1]), np.array([0.0]), derivative=1)
        np.testing.assert_allclose(left, right, atol=1e-12)
        self.assertLessEqual(
            np.max(abs(control - prior_controls(curve))), limits.change + 1e-10
        )
        plan = prepare(fitted, 0.01)
        self.assertLessEqual(plan["maximum_stored_bound_m"], 0.01)

    def test_global_shortcut_fails_fidelity_when_piecewise_fit_is_possible(self):
        curve = Curve(bump())
        limits = BankLimits(0.02, 0.1, 0.04)
        control, note = plan_bank(curve, limits, method="global")
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "global_unqualified")
        fitted, note = plan_bank(curve, limits, method="linear")
        self.assertIsNotNone(fitted, note)

    def test_valid_prior_avoids_solver(self):
        curve = Curve(recipe())
        with (
            patch(
                "road_bank_profile.linprog", side_effect=AssertionError("not needed")
            ),
            patch(
                "road_bank_profile.inequalities",
                side_effect=AssertionError("no sparse matrix needed"),
            ),
        ):
            control, note = plan_bank(curve, BankLimits(0.1, 0.05, 0.01))
        np.testing.assert_array_equal(control, prior_controls(curve))
        self.assertEqual(note["selected"], "prior")
        self.assertEqual(note["lp_calls"], 0)

    def test_conservative_polynomial_program_can_defer_a_valid_profile(self):
        source = bump()
        curve = Curve(source)
        samples, _ = differentials(curve, np.linspace(0, 10, 1001))
        self.assertLess(np.max(abs(samples[:, :, 2])), 0.04)
        control, note = plan_bank(curve, BankLimits(0.04, 0.1, 0))
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "linear_infeasible")

    def test_short_fixed_edges_fail_before_solver(self):
        source = recipe()
        length = 0.005
        source["range_m"] = [0, length]
        source["intervals"][0]["range_m"] = [0, length]
        source["seams"][1][9] = 0.033
        for pose, s in zip(
            (*source["seams"], source["intervals"][0]["middle"]),
            (0, length, length / 2),
            strict=True,
        ):
            pose[:4] = [s, s, 0, 0]
        curve = Curve(source)
        with patch(
            "road_bank_profile.linprog", side_effect=AssertionError("impossible")
        ):
            control, note = plan_bank(curve, BankLimits(0.35, 0.12, 0.025))
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "fixed_edge_displacement")
        self.assertEqual(note["lp_calls"], 0)

    def test_unalterable_center_grade_is_not_hidden_by_bank(self):
        control, note = plan_bank(Curve(recipe(height=0.1)), BankLimits(0.1, 0.1, 0.01))
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "fixed_center_grade")

    def test_unfinished_solver_with_candidate_is_still_pending(self):
        fake = OptimizeResult(success=False, status=1, nit=3, x=np.zeros(9))
        with patch("road_bank_profile.linprog", return_value=fake):
            control, note = plan_bank(
                Curve(bump()), BankLimits(0.02, 0.1, 0.04), method="linear"
            )
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "linear_incomplete")

    def test_source_span_budget_and_fixed_crossfall(self):
        curve = Curve(bump())
        control, note = plan_bank(curve, BankLimits(0.1, 0.1, 0.01, max_spans=1))
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "source_span_budget")
        source = recipe(bank_rate=0.02)
        control, note = plan_bank(Curve(source), BankLimits(0.1, 0.01, 0.01))
        self.assertIsNone(control)
        self.assertEqual(note["reason"], "fixed_crossfall")

    def test_curved_banked_adapter_matches_independent_position_differences(self):
        source = recipe(curvature=0.04, height=0.001, bank_rate=0.005)
        curve = Curve(source)
        fitted = BankedSurface(source, prior_controls(curve))
        stations = np.array([0.1, 2.3, 6.1, 9.9])
        ds, _ = differentials(fitted, stations)
        epsilon = 1e-5
        expected = (fitted.at(stations + epsilon) - fitted.at(stations - epsilon)) / (
            2 * epsilon
        )
        np.testing.assert_allclose(ds, expected, atol=2e-10, rtol=1e-9)

    def test_limits_and_control_shape_are_checked(self):
        for values in ((0, 0.1, 0.1), (0.1, np.nan, 0.1), (0.1, 0.1, -1)):
            with self.assertRaises(ValueError):
                BankLimits(*values)
        with self.assertRaises(ValueError):
            BankedSurface(recipe(), [[0, 0, 0]])


if __name__ == "__main__":
    unittest.main()
