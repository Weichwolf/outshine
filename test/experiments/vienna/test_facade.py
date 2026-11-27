import unittest

import numpy as np
from facade import FacadeView, draw_facade, flat_bays
from outshine_ref.buildings.facades import FacadeRepeat
from scene_mesh import Meshes


class FacadeTest(unittest.TestCase):
    def test_flat_facade_fills_the_wall_once(self):
        repeat = FacadeRepeat(29, 12, 8, 4, 0.42, 0.55)
        areas = {}
        triangles = {}
        for material, points, indices in flat_bays(repeat):
            a, b, c = points[indices].transpose(1, 0, 2)
            areas[material] = np.cross(b - a, c - a)[:, 2].sum() / 2
            triangles[material] = len(indices)
        self.assertAlmostEqual(sum(areas.values()), 29 * 12)
        self.assertAlmostEqual(areas["glass"], 29 * 12 * 0.42 * 0.55)
        self.assertEqual(triangles["glass"], 2 * repeat.columns * repeat.rows)

    def test_long_wall_keeps_near_relief_and_distant_openings_in_the_same_grid(self):
        meshes = Meshes()
        draw_facade(
            meshes,
            np.array([0, 0]),
            np.array([220, 0]),
            0,
            12,
            FacadeView((8, -24), 700),
            grounded=True,
            bays=True,
        )
        points = np.asarray(meshes["glass"][0])
        near = points[points[:, 1] > 0]
        far = points[points[:, 1] == 0]
        self.assertGreater(len(near), 0)
        self.assertGreater(len(far), 0)
        self.assertLess(near[:, 0].max(), far[:, 0].min())
        self.assertLess(np.asarray(meshes["window sill"][0])[:, 0].max(), 85)
        for detail in (near, far):
            self.assertEqual(len(np.unique(np.round(detail[:, 2], 6))), 8)
        self.assertEqual(np.asarray(meshes["wall"][0])[:, 0].max(), 220)
