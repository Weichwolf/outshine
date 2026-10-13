"""Complete botanical site sequences, physical limits and immutable cached products."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import DistancePattern, Profile


class DistancePatternTests(unittest.TestCase):
    def test_constant_internodes_and_whorls_have_closed_form_positions(self):
        pattern = DistancePattern(
            spacing=Profile(np.full(4, 0.125)), whorl_size=3, divergence=0.7
        )
        tables = pattern.tables([0, 0.5, 1], 0.25, 0.75)
        for length, table in zip((0, 0.5, 1), tables, strict=True):
            groups = 0 if length == 0 else int(0.5 * length / 0.125) + 1
            positions = np.repeat(0.75 - np.arange(groups) * 0.125 / (length or 1), 3)
            angles = np.repeat(np.arange(groups) * 0.7, 3)
            angles += np.tile(np.arange(3) * (2 * np.pi / 3), groups)
            np.testing.assert_array_equal(table, np.column_stack((positions, angles)))

    def test_profiled_recurrence_matches_independent_linear_interpolation(self):
        values = np.array([0.037, 0.091, 0.054, 0.062])
        pattern = DistancePattern(spacing=Profile(values), whorl_size=2)
        lengths = np.linspace(0.03, 2.3, 73)
        tables = pattern.tables(lengths, 0.13, 0.93)
        for length, table in zip(lengths, tables, strict=True):
            positions, current = [], 0.93
            while current >= 0.13 - 32 * np.finfo(float).eps:
                current = max(current, 0.13)
                positions.append(current)
                x = current * 3
                index = min(int(x), 2)
                spacing = values[index] * (1 - (x - index))
                spacing += values[index + 1] * (x - index)
                current -= spacing / length
            np.testing.assert_allclose(table[:, 0], np.repeat(positions, 2), atol=2e-14)
            self.assertLessEqual(len(table), pattern.capacity(length, 0.13, 0.93))

    def test_preflight_rejects_a_budget_before_allocating_sites(self):
        pattern = DistancePattern()
        with patch(
            "outshine_ref.vegetation.placement_patterns.np.full",
            side_effect=AssertionError("site buffer allocated before budget refusal"),
        ):
            self.assertIsNone(pattern.tables([1, 2], 0, 1, max_sites=0))
        with self.assertRaisesRegex(ValueError, "site budget"):
            replace(pattern, whorl_size=64).tables([100], 0, 1)

    def test_sparse_parents_have_complete_shared_immutable_empty_tables(self):
        lengths = np.zeros(65536)
        lengths[-1] = 1
        pattern = DistancePattern(spacing=Profile(np.full(4, 1 / 4095)))
        tables = pattern.tables(lengths, 0, 1, max_sites=4096)
        self.assertEqual(sum(map(len, tables)), 4096)
        self.assertIs(tables[0], tables[-2])
        np.testing.assert_allclose(
            tables[-1][:, 0], 1 - np.arange(4096) / 4095, atol=2e-15
        )
        for table in (tables[0], tables[-1]):
            self.assertFalse(table.flags.writeable)
            with self.assertRaises(ValueError):
                table.setflags(write=True)
            with self.assertRaises(ValueError):
                table.base.setflags(write=True)

    def test_seeded_spread_survives_parent_reordering_and_recipe_reload(self):
        pattern = DistancePattern(whorl_size=3, spread_m=0.015)
        tables = pattern.tables([0.4, 0.9], 0.1, 0.95, seeds=[17, 23])
        loaded = DistancePattern.from_recipe(pattern.recipe())
        reverse = loaded.tables([0.9, 0.4], 0.1, 0.95, seeds=[23, 17])
        for table, other in zip(tables, reverse[::-1], strict=True):
            np.testing.assert_array_equal(table, other)
            self.assertTrue(np.all((table[:, 0] >= 0.1) & (table[:, 0] <= 0.95)))

    def test_extreme_and_invalid_inputs_do_not_produce_partial_or_nonfinite_sites(self):
        pattern = DistancePattern(limit_groups=3)
        huge = replace(pattern, limit_groups=1, spacing=Profile(np.full(4, 1e-308)))
        with np.errstate(over="raise", invalid="raise"):
            np.testing.assert_array_equal(huge.tables([1e308], 0, 1)[0], [[1.0, 0.0]])
        with self.assertRaisesRegex(ValueError, "distinct groups"):
            pattern.tables([1e308], 0, 1)
        varied = replace(
            pattern, spacing=Profile(np.array([1e-10, 2e-10, 1e-10, 2e-10]))
        )
        with self.assertRaisesRegex(ValueError, "advance"):
            varied.tables([1e20], 0, 1)
        for lengths, first, last in (([np.nan], 0, 1), ([-1], 0, 1), ([1], 0.8, 0.3)):
            with self.assertRaises(ValueError):
                pattern.tables(lengths, first, last)
        self.assertEqual(len(pattern.tables([1], 0.5, 0.5)[0]), 0)
        self.assertEqual(len(replace(pattern, limit_groups=0).tables([1], 0, 1)[0]), 0)
        with self.assertRaises(ValueError):
            pattern.tables([1], 0, 1, max_sites=True)


if __name__ == "__main__":
    unittest.main()
