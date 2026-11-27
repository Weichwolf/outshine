import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from plant_sites import scatter_mesh
from woodland_shrubs import woodland_shrubs


class WoodlandShrubsTest(unittest.TestCase):
    def test_crowns_leave_the_reserved_path_clear_on_sloping_ground(self):
        xy = np.array([[-5, -5], [7, -5], [7, 5], [-5, 5]])
        ground = np.column_stack((xy, 0.2 * xy[:, 0]))
        mesh = ground, [(0, 1, 2), (0, 2, 3)]
        habitat = shapely.box(-5, -5, 7, 5).difference(shapely.box(0, -5, 2, 5))
        candidates = np.array([[-2.5, 1.0], [-0.05, 1.0], [1.0, 1.0]])
        marks = np.tile([0.1, 0.2, 0.0], (len(candidates), 1))
        source = SimpleNamespace(lon=0.0, lat=0.0, metres=np.array([1e5, 1e5]))
        with patch("woodland_shrubs.tile_sites", return_value=[(candidates, marks)]):
            batches = list(
                woodland_shrubs(mesh, source, habitat, np.array([0, 0, 1.7]), 4)
            )
        ((_, _, products, poses),) = batches
        self.assertEqual(len(poses), 1)
        np.testing.assert_allclose(poses[0, :3], [-2.5, 1, -0.525])
        for mesh in products.values():
            vertices, _ = scatter_mesh(mesh, poses[:, :3], poses[:, 3], poses[:, 4])
            self.assertTrue((vertices[:, 0] < -1).all())
