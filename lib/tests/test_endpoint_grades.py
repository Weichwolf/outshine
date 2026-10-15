"""Independent fixed-grade contact, feasibility and whole-polynomial checks."""

import json
import unittest

import numpy as np
from outshine_ref.transport import (
    EndpointTransition,
    plan_endpoint_transition,
    sample_vertical_transition,
)


class EndpointGradeChecks(unittest.TestCase):
    def test_affine_contact_is_retained_at_both_nonzero_ends(self):
        for method in ("envelope", "hermite"):
            curve, report = plan_endpoint_transition(
                7, 9, 100, 0.02, 0.02, method=method
            )
            self.assertTrue(report["valid"], report)
            np.testing.assert_allclose(curve.evaluate([0, 100]), [7, 9], atol=1e-12)
            np.testing.assert_allclose(
                curve.evaluate([0, 100], 1), [0.02, 0.02], atol=1e-12
            )

    def test_exact_acceleration_edge_and_negative_height(self):
        for sign in (-1, 1):
            curve, report = plan_endpoint_transition(
                -30,
                -30 + sign * 0.1,
                10,
                0,
                sign * 0.02,
                grade=0.08,
                second_derivative=0.002,
            )
            self.assertTrue(report["valid"], report)
            x = np.linspace(0, 10, 31)
            np.testing.assert_allclose(
                curve.evaluate(x), -30 + sign * 0.001 * x**2, atol=1e-12
            )
            np.testing.assert_allclose(
                curve.evaluate(x, 1), sign * 0.002 * x, atol=1e-12
            )

    def test_matching_affine_surface_gets_no_gratuitous_bends(self):
        grade = -0.0024644059037959402
        length, start = 110.17154138126546, 166.006591796875
        for method in ("envelope", "hermite"):
            profile, note = plan_endpoint_transition(
                start, start + length * grade, length, grade, grade, method=method
            )
            self.assertTrue(note["direct_affine_contact"])
            json.dumps(note)
            self.assertEqual(len(profile.coefficients), 1)
            np.testing.assert_array_equal(
                profile.evaluate([0, length / 2, length], 2), [0, 0, 0]
            )

    def test_nonzero_end_grades_make_length_feasibility_nonmonotone(self):
        valid = []
        for length in (0.01, 0.1, 0.4):
            _, report = plan_endpoint_transition(
                0, 0.001, length, 0.1, 0.1, grade=0.2, second_derivative=1
            )
            valid.append(report["valid"])
        self.assertEqual(valid, [True, False, True])

    def test_reachable_envelope_can_succeed_where_single_cubic_fails(self):
        for method, valid in (("envelope", True), ("hermite", False)):
            curve, report = plan_endpoint_transition(
                0, 0.001, 0.4, 0.1, 0.1, grade=0.2, second_derivative=1, method=method
            )
            self.assertEqual(report["valid"], valid, report)
            self.assertEqual(curve is not None, valid)

    def test_constrained_inputs_fail_with_distinct_local_reasons(self):
        for end, length, g0, g1, reason in (
            (0, 10, 0.09, 0, "endpoint_grade_out_of_bounds"),
            (0, 10, -0.08, 0.08, "endpoint_grade_rate_infeasible"),
            (1, 10, 0, 0, "height_outside_reachable_grade_envelope"),
        ):
            curve, report = plan_endpoint_transition(0, end, length, g0, g1)
            self.assertIsNone(curve)
            self.assertEqual(report["reason"], reason)

    def test_interval_bounds_chords_and_owned_bytes(self):
        for ends in ((0.03, -0.01), (-0.02, 0.04), (0.0, 0.0)):
            curve, report = plan_endpoint_transition(153, 155, 130, *ends)
            self.assertTrue(report["valid"], report)
            table, note = sample_vertical_transition(curve, height_error_m=0.001)
            self.assertIsNotNone(table)
            x = np.linspace(0, 130, 4097)
            self.assertLessEqual(np.max(abs(curve.evaluate(x, 1))), 0.08 + 1e-10)
            self.assertLessEqual(np.max(abs(curve.evaluate(x, 2))), 0.002 + 1e-12)
            self.assertLessEqual(
                np.max(abs(curve.evaluate(x) - np.interp(x, table[:, 0], table[:, 1]))),
                note["maximum_height_chord_bound_m"] + 1e-12,
            )
            with self.assertRaises(ValueError):
                curve.coefficients[0, 0] = 0
            self.assertIsNone(sample_vertical_transition(curve, max_vertices=2)[0])

    def test_mutation_and_query_budgets(self):
        breaks = np.array([0.0, 10.0])
        coefficients = np.array([[0.0, 0.02, 0.0, 0.0]])
        curve = EndpointTransition(breaks, coefficients, 0.02, 0.02, 0.08, 0.002)
        breaks[-1], coefficients[0, 1] = 3, 99
        np.testing.assert_allclose(curve.evaluate([0, 10]), [0, 0.2])
        with self.assertRaises(ValueError):
            curve.breaks.setflags(write=True)
        for query, kwargs in (([0, 1], {"max_queries": 1}), ([-1], {}), ([11], {})):
            with self.assertRaises(ValueError):
                curve.evaluate(query, **kwargs)

    def test_interior_grade_extremum_and_c1_mismatch_are_rejected(self):
        with self.assertRaises(ValueError):
            EndpointTransition([0, 10], [[0, 0, 0.03, -0.002]], 0, 0, 0.1, 0.06)
        with self.assertRaises(ValueError):
            EndpointTransition(
                [0, 10, 20],
                [[0, 0.02, 0, 0], [0.2, 0.03, 0, 0]],
                0.02,
                0.03,
                0.08,
                0.002,
            )

    def test_nonfinite_and_out_of_domain_inputs(self):
        for inputs in ((0, np.nan, 10, 0, 0), (0, 0, 0, 0, 0), (0, 0, 10, np.inf, 0)):
            with self.assertRaises(ValueError):
                plan_endpoint_transition(*inputs)
        for kwargs in ({"grade": 0}, {"second_derivative": 0}, {"method": "unknown"}):
            with self.assertRaises(ValueError):
                plan_endpoint_transition(0, 0, 10, 0, 0, **kwargs)

    def test_all_four_endpoint_sign_choices_and_exact_grade_plateau(self):
        for g0 in (-0.08, 0.08):
            for g1 in (-0.08, 0.08):
                height = (g0 + g1) * 50
                curve, report = plan_endpoint_transition(0, height, 100, g0, g1)
                self.assertTrue(report["valid"], report)
                np.testing.assert_allclose(
                    curve.evaluate([0, 100], 1), [g0, g1], atol=1e-10
                )


if __name__ == "__main__":
    unittest.main()
