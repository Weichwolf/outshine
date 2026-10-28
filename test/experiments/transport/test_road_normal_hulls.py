"""Exact subdivision identities and whole additional original road bodies."""

import math
import unittest
from fractions import Fraction as F
from unittest.mock import patch

import numpy as np
from replay_road_closed_surface import ARRAYS, original_cases
from road_closed_checks import observed_body_error
from road_closed_surface import prepare_closed
from road_normal_hulls import derivative_controls, split_half, upward
from test_road_closed_surface import straight


def polynomial(controls, t):
    degree = len(controls) - 1
    return sum(
        F(math.comb(degree, i)) * t**i * (1 - t) ** (degree - i) * c
        for i, c in enumerate(controls)
    )


class RationalNormalHulls(unittest.TestCase):
    def test_both_halves_equal_independent_exact_polynomial_everywhere_tested(self):
        for original in ((F(0), F(2), F(0)), (F(1, 3), F(2, 7), F(-4, 9), F(1, 2))):
            left, right = split_half(original)
            self.assertEqual(left[0], original[0])
            self.assertEqual(right[-1], original[-1])
            self.assertEqual(left[-1], right[0])
            for numerator in range(17):
                u = F(numerator, 16)
                self.assertEqual(polynomial(left, u), polynomial(original, u / 2))
                self.assertEqual(
                    polynomial(right, u), polynomial(original, (1 + u) / 2)
                )

    def test_known_quadratic_hull_tightens_without_losing_the_whole_domain(self):
        left, right = split_half((0, 2, 0))
        self.assertEqual(left, (F(0), F(1), F(1)))
        self.assertEqual(right, (F(1), F(1), F(0)))
        self.assertEqual(max(map(abs, left + right)), 1)
        self.assertEqual(derivative_controls([0, 1, 2, 3], 3), (F(1), F(1), F(1)))

    def test_float_limits_round_outward_and_invalid_choice_is_rejected(self):
        for value in (F(1, 7), F(1, 10), F(1, 2), F(1, 10**300)):
            result = upward(value)
            self.assertGreaterEqual(F(result), value)
            self.assertLess(F(math.nextafter(result, -math.inf)), value)
        with self.assertRaisesRegex(ValueError, "choice"):
            prepare_closed(
                straight(), 0.25, max_payload_bytes=1024, refine_normal_bound=1
            )

    def test_existing_admission_never_pays_for_refinement(self):
        with patch(
            "road_normal_hulls.refined_normal_departure",
            side_effect=AssertionError("refined"),
        ):
            a = prepare_closed(straight(), 0.25, max_payload_bytes=1024)
            b = prepare_closed(
                straight(), 0.25, max_payload_bytes=1024, refine_normal_bound=True
            )
        for key in ARRAYS:
            self.assertEqual(a[key].tobytes(), b[key].tobytes())
        self.assertEqual(a["recipe_version"], b["recipe_version"])


class OriginalRefinedNormalHulls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = original_cases()[0]

    def test_four_additional_originals_pass_whole_bodies_with_same_metric_budget(self):
        for case in self.cases:
            if case["ordinal"] not in {4947, 10805, 21884, 27428}:
                continue
            for method in ("adaptive", "uniform"):
                body = prepare_closed(
                    case["recipe"],
                    0.25,
                    max_payload_bytes=8 * 1024 * 1024,
                    method=method,
                    refine_normal_bound=True,
                )
                self.assertEqual(
                    body["recipe_version"], "outshine/direct-vertical-road-display/2"
                )
                self.assertEqual(
                    body["normal_bound_subdivision_boxes"],
                    2 * len(case["recipe"]["intervals"]),
                )
                self.assertGreater(
                    body["original_normal_offset_departure_bound_m"], 0.25
                )
                self.assertLessEqual(body["maximum_stored_bound_m"], 0.25)
                self.assertGreater(body["quality"]["signed_volume_m3"], 0)
                self.assertLessEqual(
                    observed_body_error(case["recipe"], body),
                    body["maximum_stored_bound_m"] + 1e-7,
                )

    def test_all_previous_products_remain_exact_and_the_last_source_stays_explicit(
        self,
    ):
        accepted = old_accepted = 0
        for case in self.cases:
            source = case["recipe"]
            try:
                old = prepare_closed(source, 0.25, max_payload_bytes=8 * 1024 * 1024)
            except ValueError:
                old = None
            try:
                new = prepare_closed(
                    source,
                    0.25,
                    max_payload_bytes=8 * 1024 * 1024,
                    refine_normal_bound=True,
                )
            except ValueError as error:
                self.assertEqual(case["ordinal"], 54991)
                self.assertIn("Normal-offset", str(error))
                continue
            accepted += 1
            if old is not None:
                old_accepted += 1
                for key in ARRAYS:
                    np.testing.assert_array_equal(new[key], old[key])
                for key in old.keys() - set(ARRAYS):
                    self.assertEqual(old[key], new[key])
        self.assertEqual((accepted, old_accepted), (185, 181))


if __name__ == "__main__":
    unittest.main()
