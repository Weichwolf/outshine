"""Seeded growth against independent nearest queries and invariant seed topology."""

import unittest
from unittest.mock import patch

import numpy as np
from colonization import GrowthBudget, grow
from oak_scaffold import OakRecipe


class SeededGrowthTests(unittest.TestCase):
    def test_seed_is_exact_prefix_and_tree_queries_agree_with_brute(self):
        points = np.random.default_rng(7).uniform([-1, -1, 0.5], [1, 1, 2], (64, 3))
        seed = (
            np.array([[0.0, 0, 0], [0, 0, 0.5], [0.3, 0, 0.7]]),
            np.array([-1, 0, 1], np.int32),
        )
        original = [a.copy() for a in seed]
        args = (points, 2, 0.25, 0.12, 0.7, 0.16)
        a = grow(*args, method="brute", budget=GrowthBudget(256, 32), seed=seed)
        b = grow(*args, method="tree", budget=GrowthBudget(256, 32), seed=seed)
        for key in ("positions", "parents", "remaining"):
            np.testing.assert_array_equal(a[key], b[key])
        for key in ("consumed", "queries", "iterations", "terminal_reason"):
            self.assertEqual(a[key], b[key])
        for key, source in zip(("positions", "parents"), original, strict=True):
            np.testing.assert_array_equal(a[key][: len(source)], source)
        for actual, source in zip(seed, original, strict=True):
            np.testing.assert_array_equal(actual, source)

    def test_explicit_legacy_trunk_equals_default_seed_exactly(self):
        points = np.array([[0.2, 0.1, 1.5], [0.5, -0.2, 1.8]])
        positions = np.zeros((6, 3))
        positions[:, 2] = np.linspace(0, 0.5, 6)
        parents = np.arange(-1, 5, dtype=np.int32)
        args = (points, 2, 0.25, 0.1, 1.5, 0.15)
        a = grow(*args, budget=GrowthBudget(100, 16))
        b = grow(*args, budget=GrowthBudget(100, 16), seed=(positions, parents))
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])

    def test_seed_capacity_refuses_before_nearest_queries(self):
        recipe = OakRecipe(attractor_count=32, leaves=8)
        seed = recipe.skeleton()
        with (
            patch("colonization.tree_nearest", side_effect=AssertionError("query")),
            self.assertRaises(ValueError),
        ):
            grow(
                np.zeros((1, 3)),
                18,
                0.23,
                0.28,
                3,
                0.4,
                budget=GrowthBudget(len(seed[1]) - 1, 1),
                seed=seed,
            )
        limited = grow(
            recipe.growth_space(),
            18,
            0.23,
            0.28,
            3,
            0.4,
            budget=GrowthBudget(len(seed[1]), 1),
            seed=seed,
        )
        np.testing.assert_array_equal(limited["positions"], seed[0])
        self.assertLessEqual(len(limited["parents"]), len(seed[1]))

    def test_invalid_seed_geometry_and_topology_refuse(self):
        p = np.array([[0.0, 0, 0], [0, 0, 1]])
        for positions, parents in (
            (p, [-1, 1]),
            (p, [-1.0, 0.0]),
            (p * 0, [-1, 0]),
            (p * np.nan, [-1, 0]),
            (p[:, :2], [-1, 0]),
        ):
            with self.assertRaises(ValueError):
                grow(
                    np.ones((1, 3)),
                    2,
                    0.25,
                    0.1,
                    1,
                    0.2,
                    budget=GrowthBudget(32, 8),
                    seed=(positions, np.array(parents)),
                )

    def test_scaffold_is_parametric_connected_and_bounded(self):
        recipe = OakRecipe()
        a, parents = recipe.skeleton()
        b, q = recipe.skeleton()
        np.testing.assert_array_equal(a, b)
        np.testing.assert_array_equal(parents, q)
        trunk, links = recipe.skeleton(False)
        np.testing.assert_array_equal(a[: len(trunk)], trunk)
        np.testing.assert_array_equal(parents[: len(links)], links)
        self.assertTrue(np.all(parents[1:] < np.arange(1, len(parents))))
        lengths = np.linalg.norm(a[1:] - a[parents[1:]], axis=1)
        self.assertTrue((lengths > 0).all())
        self.assertLessEqual(float(lengths.max()), recipe.step_m * (1 + 1e-12))
        self.assertGreater(np.ptp(a[:, 0]), recipe.spread_m * 0.3)
        for kwargs in ({"leaves": 100001}, {"step_m": 0}, {"height_m": np.inf}):
            with self.assertRaises(ValueError):
                OakRecipe(**kwargs)


if __name__ == "__main__":
    unittest.main()
