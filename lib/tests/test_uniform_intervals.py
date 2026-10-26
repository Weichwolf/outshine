"""Independent bounded uniform groups, physical spacing and failure contracts."""

import json
import unittest
from dataclasses import replace
from fractions import Fraction
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import DistancePattern, Profile


class UniformIntervalTests(unittest.TestCase):
    def pattern(self, **values):
        return DistancePattern(
            mode="uniform", spacing=Profile(np.full(4, 0.125)), **values
        )

    def test_counts_positions_and_whorls_against_exact_rational_scalar_cases(self):
        for limit in (0, 1, 2, 3, 7, 64):
            for whorl in (1, 2, 5):
                pattern = self.pattern(limit_groups=limit, whorl_size=whorl)
                lengths = [0, 1 / 64, 1 / 4, 1 / 2, 1, 4, 16]
                for first, last in ((0, 1), (1 / 8, 7 / 8), (1 / 2, 1 / 2)):
                    tables = pattern.tables(lengths, first, last)
                    for length, table in zip(lengths, tables, strict=True):
                        count = 0
                        if length and first != last:
                            span = Fraction(last - first) * Fraction(length)
                            count = min(limit, span // Fraction(1, 8) + 1)
                        rows = []
                        for group in range(count):
                            t = Fraction(last)
                            if count > 1:
                                t -= Fraction(group, count - 1) * Fraction(last - first)
                            for member in range(whorl):
                                rows.append(
                                    (
                                        float(t),
                                        group * pattern.divergence
                                        + member * 2 * np.pi / whorl,
                                    )
                                )
                        expected = np.asarray(rows, dtype=float).reshape(-1, 2)
                        np.testing.assert_allclose(
                            table[:, 0],
                            expected[:, 0],
                            atol=4 * np.finfo(float).eps,
                            rtol=0,
                        )
                        np.testing.assert_array_equal(table[:, 1], expected[:, 1])
                        self.assertEqual(
                            len(table), pattern.capacity(length, first, last)
                        )

    def test_full_interval_and_minimum_spacing_with_nondyadic_inputs(self):
        pattern = replace(
            self.pattern(limit_groups=20), spacing=Profile(np.full(4, 0.015))
        )
        lengths = np.r_[0, 1e-8, np.linspace(0.001, 40, 301), 1e308]
        for length, table in zip(
            lengths, pattern.tables(lengths, 0.13, 0.93), strict=True
        ):
            if len(table):
                self.assertEqual(table[0, 0], 0.93)
            if len(table) > 1:
                self.assertAlmostEqual(table[-1, 0], 0.13, delta=2e-16)
                normalized = -np.diff(table[:, 0])
                self.assertTrue(np.all(normalized > 0))
                # Compare normalized distances to avoid physical overflow.
                self.assertTrue(
                    np.all(normalized >= 0.015 / length - 64 * np.finfo(float).eps)
                )

    def test_existing_closed_endpoint_tolerance_is_retained(self):
        length = 0.6265822784810127
        pattern = replace(self.pattern(), spacing=Profile(np.full(4, 0.045)))
        first, last = 0.15, 0.94
        raw = (last - first) * length / 0.045
        self.assertEqual(int(np.floor(raw)) + 1, 11)
        table = pattern.tables([length], first, last)[0]
        self.assertEqual(len(table), 12)
        self.assertEqual(pattern.capacity(length, first, last), 12)

    def test_cap_spreads_instead_of_cutting_a_sequence_and_single_stays_at_tip(self):
        pattern = self.pattern(limit_groups=3)
        np.testing.assert_array_equal(
            pattern.tables([4], 0.25, 0.75)[0][:, 0], [0.75, 0.5, 0.25]
        )
        np.testing.assert_array_equal(
            replace(pattern, limit_groups=1).tables([4], 0.25, 0.75)[0], [[0.75, 0]]
        )
        for mode in ("phyllotaxy", "interval"):
            np.testing.assert_array_equal(
                replace(pattern, mode=mode).tables([4], 0.25, 0.75)[0][:, 0],
                [0.75, 0.71875, 0.6875],
            )

    def test_budget_refusal_precedes_site_allocation_and_tables_stay_immutable(self):
        pattern = self.pattern(limit_groups=20, whorl_size=2)
        with patch(
            "outshine_ref.vegetation.placement_patterns.np.full",
            side_effect=AssertionError("early allocation"),
        ):
            self.assertIsNone(pattern.tables([2, 8], 0, 1, max_sites=1))
        table = pattern.tables([0, 8], 0, 1)[1]
        self.assertFalse(table.flags.writeable)
        with self.assertRaises(ValueError):
            table.setflags(write=True)
        self.assertEqual(len(replace(pattern, limit_groups=0).tables([8], 0, 1)[0]), 0)
        with self.assertRaisesRegex(ValueError, "site budget"):
            self.pattern(limit_groups=4096, whorl_size=2).tables([1e6], 0, 1)

    def test_recipe_reorder_and_explicit_unsupported_controls(self):
        pattern = self.pattern(limit_groups=9, whorl_size=2)
        loaded = DistancePattern.from_recipe(json.loads(json.dumps(pattern.recipe())))
        self.assertEqual(loaded.recipe(), pattern.recipe())
        tables = loaded.tables([0.04, 1, 12], 0.1, 0.9, seeds=[1, 2, 3])
        reverse = loaded.tables([12, 1, 0.04], 0.1, 0.9, seeds=[3, 2, 1])
        for a, b in zip(tables, reverse[::-1], strict=True):
            np.testing.assert_array_equal(a, b)
        for values in (
            {"spread_m": 1e-8},
            {"spacing": Profile(np.array([0.1, 0.2, 0.1, 0.2]))},
        ):
            with self.assertRaisesRegex(ValueError, "constant minimum spacing"):
                replace(pattern, **values)

    def test_unresolvable_uniform_interval_fails_without_partial_output(self):
        pattern = replace(
            self.pattern(limit_groups=4), spacing=Profile(np.full(4, 1e-30))
        )
        with self.assertRaisesRegex(ValueError, "distinct groups"):
            pattern.tables([1], np.nextafter(1.0, 0.0), 1.0)


if __name__ == "__main__":
    unittest.main()
