"""Independent affine examples, original-prior contradictions and bounded failure."""

import unittest
from fractions import Fraction
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from constructive_earthwork_plane import affine_prior_conflict, fit_plane


class ConstructivePlaneTests(unittest.TestCase):
    def setUp(self):
        self.xy = np.array([[-4.0, -3], [4, -3], [4, 3], [-4, 3], [0, 0]])
        self.height = 166 + self.xy @ [0.01, -0.005]
        self.limits = {"maximum_gradient": 0.025, "maximum_earthwork_m": 0.5}

    def test_independent_plane_is_recovered_and_input_order_is_canonical(self):
        plane, report = fit_plane(self.xy, self.height, **self.limits)
        self.assertEqual(report["state"], "candidate")
        np.testing.assert_allclose(
            plane.evaluate(self.xy), self.height, rtol=0, atol=1e-8
        )
        np.testing.assert_allclose(plane.gradient, [0.01, -0.005], rtol=0, atol=1e-8)
        other, _ = fit_plane(self.xy[::-1], self.height[::-1], **self.limits)
        self.assertEqual(plane, other)

    def test_original_prior_failure_is_not_relabelled_as_a_source_fit(self):
        heights = self.height.copy()
        heights[4] += 2
        self.assertIsNotNone(affine_prior_conflict(self.xy, heights, 0.025, 0.5))
        plane, report = fit_plane(self.xy, heights, **self.limits)
        self.assertIsNone(plane)
        self.assertEqual(report["reason"], "construction_earthwork_budget")
        # A separately requested construction budget changes no observation.
        before = heights.copy()
        constructed, _ = fit_plane(
            self.xy, heights, maximum_gradient=0.025, maximum_earthwork_m=2
        )
        self.assertIsNotNone(constructed)
        np.testing.assert_array_equal(heights, before)

    def test_unavailable_solver_never_returns_a_zero_plane(self):
        with patch(
            "constructive_earthwork_plane.linprog",
            return_value=SimpleNamespace(success=False, status=1),
        ):
            plane, report = fit_plane(self.xy, self.height, **self.limits)
        self.assertIsNone(plane)
        self.assertEqual(report["solver_status"], 1)

    def test_observation_capacity_precedes_program_allocation(self):
        with (
            patch(
                "constructive_earthwork_plane.linprog",
                side_effect=AssertionError("solve"),
            ),
            self.assertRaises(ValueError),
        ):
            fit_plane(self.xy, self.height, observations_most=4, **self.limits)

    def test_conflicting_duplicate_observations_and_nonfinite_data_are_rejected(self):
        for xy, height in (
            (np.vstack((self.xy, self.xy[0])), np.r_[self.height, 9]),
            (self.xy, self.height * np.nan),
        ):
            with self.subTest(shape=xy.shape), self.assertRaises(ValueError):
                fit_plane(xy, height, **self.limits)

    def test_no_pair_witness_does_not_claim_general_feasibility(self):
        self.assertIsNone(affine_prior_conflict(self.xy, self.height, 0.025, 0.5))

    def test_steep_observations_keep_the_requested_finite_gradient(self):
        height = 166 + self.xy @ [0.1, 0.1]
        plane, report = fit_plane(
            self.xy, height, maximum_gradient=0.025, maximum_earthwork_m=2
        )
        squared = sum(Fraction(g) ** 2 for g in plane.gradient)
        self.assertLessEqual(squared, Fraction(0.025 * (1 + 1e-9)) ** 2)
        self.assertGreater(report["maximum_observed_cut_fill_m"], 0.5)
        np.testing.assert_allclose(
            report["maximum_observed_cut_fill_m"],
            abs(plane.evaluate(self.xy) - height).max(),
            rtol=0,
            atol=1e-12,
        )

    def test_success_status_cannot_admit_an_invalid_solver_plane(self):
        invalid = SimpleNamespace(success=True, status=0, x=np.array([166, 1.0, 1, 0]))
        with patch("constructive_earthwork_plane.linprog", return_value=invalid):
            plane, report = fit_plane(self.xy, self.height, **self.limits)
        self.assertIsNone(plane)
        self.assertEqual(report["reason"], "unqualified_plane_gradient")


if __name__ == "__main__":
    unittest.main()
