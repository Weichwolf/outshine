"""Independent cohort, nearest-query, C1 composition and capacity counterexamples."""

import unittest
from unittest.mock import patch

import numpy as np
from colonization import GrowthBudget
from limb_spine import LimbSpine
from limb_surface import limb_mesh, path_recipe
from oak_scaffold import OakRecipe
from scipy.interpolate import CubicHermiteSpline
from subcrown_probe import build
from subcrown_space import cohorts, sample_space


class SubcrownTests(unittest.TestCase):
    def test_analytic_ellipsoids_fit_shared_box(self):
        for recipe in (
            OakRecipe(),
            OakRecipe(height_m=5, spread_m=25),
            OakRecipe(height_m=35, spread_m=4),
        ):
            for mode in ("raised", "limb"):
                d = cohorts(recipe, mode)
                extent = np.sqrt(np.sum(d["transforms"] ** 2, axis=2))
                self.assertEqual(len(d["hosts"]), 7)
                self.assertTrue((np.linalg.det(d["transforms"]) > 0).all())
                self.assertTrue((d["centres"] - extent >= d["lower"] - 1e-14).all())
                self.assertTrue((d["centres"] + extent <= d["upper"] + 1e-14).all())

    def test_points_inside_own_cohort_and_volume_moments(self):
        recipe = OakRecipe(attractor_count=8192)
        points, owners, d = sample_space(recipe)
        local = np.linalg.solve(
            d["transforms"][owners], (points - d["centres"][owners])[..., None]
        )[..., 0]
        self.assertLessEqual(float(np.max(np.linalg.norm(local, axis=1))), 1 + 1e-14)
        np.testing.assert_allclose(local.mean(0), 0, atol=0.02)
        np.testing.assert_allclose((local**2).mean(0), 1 / 5, atol=0.01)
        self.assertEqual(len(points), recipe.attractor_count)
        np.testing.assert_array_equal(np.bincount(owners), d["counts"])
        ratio = abs(np.linalg.det(d["transforms"]))
        ratio *= recipe.attractor_count / ratio.sum()
        self.assertLess(float(abs(d["counts"] - ratio).max()), 1)

    def test_replay_seed_changes_and_invalid_modes(self):
        for seed in (0, 682, 2**64 - 1):
            recipe = OakRecipe(seed=seed, attractor_count=32)
            a = sample_space(recipe)
            b = sample_space(recipe)
            np.testing.assert_array_equal(a[0], b[0])
            np.testing.assert_array_equal(a[1], b[1])
        self.assertFalse(
            np.array_equal(
                sample_space(OakRecipe(seed=1))[0], sample_space(OakRecipe(seed=2))[0]
            )
        )
        for recipe, mode in ((None, "limb"), (OakRecipe(), "invalid")):
            with self.assertRaises((TypeError, ValueError)):
                cohorts(recipe, mode)

    def test_small_brute_and_indexed_growth_are_identical(self):
        recipe = OakRecipe(attractor_count=32, leaves=64)
        points, _, _ = sample_space(recipe)
        a, _, _ = build(recipe, points, method="brute", budget=GrowthBudget(512, 16))
        b, _, _ = build(recipe, points, method="tree", budget=GrowthBudget(512, 16))
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])

    def test_growth_caps_preserve_seed_without_leaf_geometry(self):
        recipe = OakRecipe(attractor_count=32, leaves=64)
        seed = recipe.skeleton()
        points, _, _ = sample_space(recipe)
        a, _, metrics = build(recipe, points, budget=GrowthBudget(len(seed[1]), 1))
        self.assertEqual(metrics["summary"]["leaves"], 64)
        np.testing.assert_array_equal(a["positions"], seed[0])
        np.testing.assert_array_equal(a["parents"], seed[1])


class LongLimbTests(unittest.TestCase):
    def test_composition_agrees_with_independent_global_hermite(self):
        for count in (66, 129, 130, 1025):
            t = np.linspace(0, 1, count)
            points = np.column_stack((0.1 * np.sin(t * 2), 0.03 * t**2, t * 12))
            derivatives = np.column_stack(
                (0.2 * np.cos(t * 2), 0.06 * t, np.full(count, 12))
            ) / (count - 1)
            composed = LimbSpine(points, derivatives)
            reference = CubicHermiteSpline(t, points, derivatives * (count - 1))
            probes = np.unique(
                np.r_[
                    np.linspace(0, 1, 257),
                    t[::64],
                    np.nextafter(t[::64], 0),
                    np.nextafter(t[::64][:-1], 1),
                ]
            )
            for derivative in (0, 1, 2):
                np.testing.assert_allclose(
                    composed.evaluate(probes, derivative),
                    reference(probes, derivative),
                    rtol=1e-8,
                    atol=2e-7,
                )
            np.testing.assert_allclose(composed.evaluate(t), points, atol=2e-14)
            self.assertEqual(composed.evaluate(0.5).shape, (3,))

    def test_long_mesh_stays_closed_regular_and_budgeted(self):
        p = np.column_stack((np.zeros((130, 2)), np.arange(130) * 0.2))
        radii = np.linspace(0.1, 0.001, len(p))
        with patch("limb_surface.path_recipe", side_effect=AssertionError):
            self.assertIsNone(limb_mesh(p, radii, np.arange(130), max_faces=0))
        mesh = limb_mesh(p, radii, np.arange(130))
        np.testing.assert_allclose(
            np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-14
        )
        edges = mesh["faces"][:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
        _, counts = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
        np.testing.assert_array_equal(counts, np.full(len(counts), 2))
        for count in (1, 1026):
            with self.assertRaises(ValueError):
                path_recipe(p, radii, np.arange(count))


if __name__ == "__main__":
    unittest.main()
