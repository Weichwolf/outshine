"""Independent regional/cell replay, Poisson and bounded-work controls."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import GroundCoverPlan


def ordered(product):
    ids = product["ids"]
    order = np.lexsort((ids[:, 2], ids[:, 1], ids[:, 0]))
    return {k: v[order] for k, v in product.items()}


class CoverChecks(unittest.TestCase):
    def test_negative_cells_and_adjacent_queries_exactly_partition_whole_plan(self):
        plan = GroundCoverPlan(718, 75, 0.5)
        whole = plan.sites((-2.3, -1.7), (1.4, 2.6))
        parts = [
            plan.sites((a, c), (b, d))
            for a, b in ((-2.3, 0), (0, 1.4))
            for c, d in ((-1.7, 0.3), (0.3, 2.6))
        ]
        joined = ordered(
            {k: np.concatenate([p[k] for p in parts[::-1]]) for k in whole}
        )
        for key, value in ordered(whole).items():
            np.testing.assert_array_equal(value, joined[key])
        self.assertEqual(len(np.unique(whole["ids"], axis=0)), len(whole["ids"]))
        np.testing.assert_array_equal(np.floor(whole["xy"] / 0.5), whole["ids"][:, :2])
        self.assertTrue(
            np.all((whole["xy"] >= (-2.3, -1.7)) & (whole["xy"] < (1.4, 2.6)))
        )

    def test_insufficient_budgets_never_draw_site_poses_or_make_a_large_grid(self):
        plan = GroundCoverPlan(2, 100)
        original = GroundCoverPlan._random

        def counts_only(owner, cell, purpose):
            self.assertEqual(purpose, b"count")
            return original(owner, cell, purpose)

        with patch.object(GroundCoverPlan, "_random", counts_only):
            self.assertIsNone(plan.sites((-1, -1), (1, 1), max_sites=1))
        with patch.object(np, "meshgrid", side_effect=AssertionError("oversized grid")):
            self.assertIsNone(plan.sites((-1e7, -1e7), (1e7, 1e7)))
        self.assertEqual(plan.sites((1, 1), (1, 2), 0)["xy"].shape, (0, 2))

    def test_count_and_position_streams_are_separate_and_replay_is_versioned(self):
        low, high = GroundCoverPlan(8, 10), GroundCoverPlan(8, 100)
        a, b = low.sites((0, 0), (1, 1)), high.sites((0, 0), (1, 1))
        n = min(len(a["xy"]), len(b["xy"]))
        for key in a:
            np.testing.assert_array_equal(a[key][:n], b[key][:n])
        restored = GroundCoverPlan.from_recipe(json.loads(json.dumps(high.recipe())))
        self.assertEqual(restored.identity, high.identity)
        for key, value in b.items():
            np.testing.assert_array_equal(value, restored.sites((0, 0), (1, 1))[key])
        with self.assertRaises(ValueError):
            GroundCoverPlan.from_recipe({**high.recipe(), "numpy": "future"})

    def test_poisson_moments_and_uniform_positions_match_independent_expectations(self):
        plan = GroundCoverPlan(943, 12)
        counts = plan.summary((-32, -32), (32, 32))["counts"]
        self.assertLess(abs(counts.mean() - 12), 6 * np.sqrt(12 / 4096))
        self.assertLess(abs(counts.var() - 12), 6 * np.sqrt((12 + 2 * 12**2) / 4096))
        sites = plan.sites((-32, -32), (32, 32))
        fractional = sites["xy"] - np.floor(sites["xy"])
        for column in np.c_[fractional, sites["marks"]].T:
            self.assertLess(abs(column.mean() - 0.5), 6 / np.sqrt(12 * len(column)))
            self.assertTrue(np.all((column >= 0) & (column < 1)))

    def test_bad_numeric_inputs_and_zero_density(self):
        for args in (
            (-1, 10),
            (1, np.inf),
            (1, -1),
            (1, 3, 0),
            (True, 10),
            (1, [1]),
            (1, "3"),
            (1, 1j),
        ):
            with self.assertRaises(ValueError):
                GroundCoverPlan(*args)
        plan = GroundCoverPlan(1, 0)
        with patch.object(
            GroundCoverPlan, "_random", side_effect=AssertionError("empty field RNG")
        ):
            self.assertEqual(plan.sites((-2, -2), (2, 2), 0)["ids"].shape, (0, 3))
        for a, b in (((1, 0), (0, 1)), ((np.nan, 0), (1, 1))):
            with self.assertRaises(ValueError):
                plan.sites(a, b)


if __name__ == "__main__":
    unittest.main()
