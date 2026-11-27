import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from scene_mesh import Meshes, terrain_grid, terrain_height
from woodland import surface_roots


class WoodlandTest(unittest.TestCase):
    def test_surface_roots_follow_a_slope_and_stop_at_the_path(self):
        source = SimpleNamespace(height=lambda xy: 0.18 * xy[:, 0] - 0.21 * xy[:, 1])
        terrain, _ = terrain_grid(source, 8)
        domain = shapely.box(-4, -4, 0.9, 4)
        meshes = Meshes()
        surface_roots(meshes, domain, terrain, np.zeros(2), 0.0, 0.23)

        vertices, faces = (np.asarray(values) for values in meshes["bark"])
        relief = vertices[:, 2] - terrain_height(terrain, vertices[:, :2])
        triangles = shapely.polygons(vertices[faces, :2])
        self.assertTrue(np.all(shapely.covers(domain, triangles)))
        self.assertTrue(np.any(vertices[:, 0] > 0.6))
        self.assertAlmostEqual(relief.min(), -0.012)
        self.assertGreater(relief.max(), 0.20)
