"""Exercise depth rejection and count early geometry exactly once."""

import unittest

import numpy as np
from current_visibility import box_bounds, project, raster
from terrain_early_patches import evaluate, rank
from terrain_grid_lod import triangles
from terrain_visibility import partition


class TerrainEarlyPatches(unittest.TestCase):
    def scene(self):
        xy = np.stack(np.meshgrid(np.linspace(-2, 2, 33), np.linspace(-2, 2, 33)), axis=-1).reshape(-1, 2)
        worlds = [np.column_stack((xy, np.full(len(xy), z))) for z in (2, 4)]
        view = (np.zeros(3), np.diag([-1, 1, 1]), 40, 160, 90)
        _, nodes = partition(8)
        low = np.concatenate([world[nodes].min(axis=1) for world in worlds])
        high = np.concatenate([world[nodes].max(axis=1) for world in worlds])
        return worlds, view, box_bounds((low, high), view)

    def test_near_surface_hides_far_patches_without_duplicating_early_geometry(self):
        worlds, view, bounds = self.scene()
        levels = np.full(32, 3)
        counts = np.full(32, len(triangles(9, 8)))
        results, images = evaluate([project(w, view) for w in worlds], bounds, levels, counts, 160, 90, [16], "coverage")
        case = results[0]
        self.assertEqual(case["early_patches"], 16)
        self.assertEqual(case["hidden_patches"], 16)
        self.assertEqual(case["early_triangles"], 2048)
        self.assertEqual(case["late_triangles"], 0)
        self.assertEqual(case["saved_interior_triangles"], 16 * counts[0])
        self.assertEqual(case["changed_depth_pixels"], 0)
        projected, z = project(worlds[0], view)
        expected = raster(projected, z, triangles(33, 1), 160, 90)
        np.testing.assert_allclose(images[16][0], expected, rtol=0, atol=1e-12)

    def test_early_fine_geometry_replaces_coarse_geometry_in_the_reference(self):
        worlds, view, _ = self.scene()
        worlds[0][3 * 33 + 3, 2] = 2.5
        _, nodes = partition(8)
        low = np.concatenate([w[nodes].min(axis=1) for w in worlds])
        high = np.concatenate([w[nodes].max(axis=1) for w in worlds])
        bounds = box_bounds((low, high), view)
        results, images = evaluate([project(w, view) for w in worlds], bounds, np.full(32, 3), np.full(32, 32), 160, 90, [16], "coverage")
        projected, z = project(worlds[0], view)
        expected = raster(projected, z, triangles(33, 1), 160, 90)
        self.assertEqual(results[0]["changed_depth_pixels"], 0)
        np.testing.assert_allclose(images[16][0], expected, rtol=0, atol=1e-12)
        coarse = nodes[:, triangles(9, 8)].reshape(-1, 3)
        self.assertGreater(np.abs(expected - raster(projected, z, coarse, 160, 90)).max(), 1e-5)

    def test_ranking_ignores_outside_patches_and_breaks_ties_by_depth(self):
        low = np.array([[0, 0], [0, 0], [0, 0]], dtype=float)
        high = np.full((3, 2), 10.0)
        bounds = low, high, np.ones(3), np.ones(3, bool), np.array([False, True, True]), np.array([1, 9, 4])
        for priority in ("coverage", "distance"):
            np.testing.assert_array_equal(rank(bounds, 160, 90, priority), [2, 1])

    def test_larger_budget_caps_at_available_patches_and_replaces_the_previous_split(self):
        worlds, view, bounds = self.scene()
        results, _ = evaluate([project(w, view) for w in worlds], bounds, np.full(32, 3), np.full(32, 32), 160, 90, [16, 1000], "surface")
        self.assertEqual([r["early_patches"] for r in results], [16, 32])
        self.assertEqual(results[1]["early_triangles"], 4096)
        self.assertEqual(results[1]["late_triangles"], 0)
        self.assertEqual(results[1]["hidden_patches"], 0)
        self.assertEqual(results[1]["saved_interior_triangles"], 0)
        self.assertTrue(all(r["changed_depth_pixels"] == 0 for r in results))


if __name__ == "__main__":
    unittest.main()
