"""Independent endpoint, derivative, optimum-envelope and mesh approximation checks."""

import unittest

import numpy as np
from outshine_ref.transport import (
    VerticalTransition,
    minimum_transition_length,
    plan_vertical_transition,
    sample_vertical_transition,
)
from scipy.integrate import quad


class TransitionChecks(unittest.TestCase):
    def test_minimum_attains_integrated_grade_envelope(self):
        for h in (0.1, 2.0, 6.0, 20.0):
            length = minimum_transition_length(h, 0.08, 0.002)
            envelope = lambda s, length=length: min(
                0.002 * s, 0.08, 0.002 * (length - s)
            )
            rise = quad(
                envelope,
                0,
                length,
                points=[min(40.0, length / 2), max(length - 40.0, length / 2)],
            )[0]
            self.assertAlmostEqual(rise, h, places=10)
            profile, _ = plan_vertical_transition(153.0, 153.0 + h, length)
            self.assertIsNotNone(profile)
            failed, report = plan_vertical_transition(0.0, h, length * 0.999)
            self.assertIsNone(failed)
            self.assertEqual(report["reason"], "insufficient_profile_length")

    def test_both_methods_preserve_contacts_and_whole_interval_limits(self):
        for delta in (-6.0, 0.0, 6.0):
            for method in ("parabolic", "cubic"):
                length = max(
                    1.0, minimum_transition_length(delta, 0.08, 0.002, method=method)
                )
                profile, report = plan_vertical_transition(
                    153.0, 153.0 + delta, length, method=method
                )
                samples = np.linspace(0.0, length, 1001)
                np.testing.assert_allclose(
                    profile.evaluate([0.0, length]), [153.0, 153.0 + delta], atol=1e-10
                )
                np.testing.assert_allclose(
                    profile.evaluate([0.0, length], 1), 0.0, atol=1e-12
                )
                self.assertLessEqual(
                    abs(profile.evaluate(samples, 1)).max(), 0.08 + 1e-12
                )
                self.assertLessEqual(
                    abs(profile.evaluate(samples, 2)).max(), 0.002 + 1e-12
                )
                self.assertTrue(report["valid"])
                for i in range(len(profile.breaks) - 2):
                    a, b, c, d = profile.coefficients[i]
                    s = profile.breaks[i + 1] - profile.breaks[i]
                    self.assertAlmostEqual(
                        a + s * (b + s * (c + s * d)),
                        profile.coefficients[i + 1, 0],
                        places=10,
                    )
                    self.assertAlmostEqual(
                        b + s * (2 * c + s * 3 * d),
                        profile.coefficients[i + 1, 1],
                        places=12,
                    )

    def test_adaptive_chord_bound_and_budget(self):
        for method in ("parabolic", "cubic"):
            profile, _ = plan_vertical_transition(0.0, 6.0, 160.0, method=method)
            table, report = sample_vertical_transition(profile, height_error_m=0.003)
            query = np.linspace(0, 160, 10001)
            error = abs(
                profile.evaluate(query) - np.interp(query, table[:, 0], table[:, 1])
            )
            self.assertLessEqual(
                error.max(), report["maximum_height_chord_bound_m"] + 1e-12
            )
            self.assertLessEqual(report["maximum_height_chord_bound_m"], 0.003)
            absent, note = sample_vertical_transition(profile, max_vertices=2)
            self.assertIsNone(absent)
            self.assertEqual(note["reason"], "profile_vertex_budget_exhausted")

    def test_long_profiles_spread_curvature_instead_of_concentrating_it_at_ports(self):
        profile, _ = plan_vertical_transition(0.0, 6.0, 10000.0)
        samples, _ = sample_vertical_transition(profile)
        self.assertEqual(len(profile.breaks), 3)
        self.assertAlmostEqual(profile.second_derivative_bound, 4 * 6 / 10000**2)
        self.assertLessEqual(len(samples), 27)
        np.testing.assert_array_equal(profile.breaks, [0.0, 5000.0, 10000.0])

    def test_fixed_length_uses_smallest_feasible_peak_second_derivative(self):
        for h, length in ((6.0, 160.0), (20.0, 300.0)):
            profile, _ = plan_vertical_transition(0.0, h, length)
            a = profile.second_derivative_bound
            area = lambda bound, length=length, profile=profile: quad(
                lambda s: min(bound * s, 0.08, bound * (length - s)),
                0,
                length,
                points=profile.breaks[1:-1],
            )[0]
            self.assertAlmostEqual(area(a), h, places=9)
            self.assertLess(area(a * 0.999), h)

    def test_owned_coefficients_and_invalid_requests(self):
        profile, _ = plan_vertical_transition(0.0, 6.0, 160.0)
        with self.assertRaises(ValueError):
            profile.coefficients.setflags(write=True)
        for query in ([-1.0], [161.0], [np.nan]):
            with self.assertRaises(ValueError):
                profile.evaluate(query)
        with self.assertRaises(ValueError):
            profile.evaluate([0.0, 1.0], max_queries=1)
        for values in ((0.0, 6.0, -1.0), (0.0, np.inf, 160.0)):
            with self.assertRaises(ValueError):
                plan_vertical_transition(*values)

    def test_declared_limits_cannot_hide_interior_extrema_or_bad_contacts(self):
        with self.assertRaises(ValueError):
            VerticalTransition([0.0, 100.0], [[0.0, 0.0, 1e308, -1e308]], 1.0, 1.0)
        with self.assertRaises(ValueError):
            VerticalTransition([0.0, 100.0], [[0.0, 0.0, 0.003, -0.00002]], 0.1, 0.006)
        with self.assertRaises(ValueError):
            VerticalTransition([0.0, 10.0], [[0.0, 0.1, 0.0, 0.0]], 0.1, 0.0)
        with self.assertRaises(ValueError):
            VerticalTransition(
                [0.0, 5.0, 10.0], [[0.0, 0.0, 0.0, 0.0], [1.0, 0.0, 0.0, 0.0]], 0.0, 0.0
            )

    def test_input_and_sampling_caps_precede_numeric_conversion_or_allocation(self):
        class Poison:
            def __len__(self):
                return 100000

            def __array__(self, *args):
                raise AssertionError("numeric conversion before size cap")

        profile, _ = plan_vertical_transition(0.0, 6.0, 160.0)
        with self.assertRaises(ValueError):
            profile.evaluate(Poison())
        with self.assertRaises(ValueError):
            VerticalTransition(Poison(), [], 0.0, 0.0)
        absent, note = sample_vertical_transition(profile, height_error_m=1e-300)
        self.assertIsNone(absent)
        self.assertEqual(note["reason"], "profile_vertex_budget_exhausted")
        for kwargs in ({"grade": float("nan")}, {"second_derivative": 1e-300}):
            with self.assertRaises(ValueError):
                plan_vertical_transition(0.0, 6.0, 160.0, **kwargs)


if __name__ == "__main__":
    unittest.main()
