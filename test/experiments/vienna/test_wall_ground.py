import unittest

import numpy as np
import shapely
from facade import FacadeView, draw_facade
from scene_mesh import Meshes
from wall_ground import WallGround


class WallGroundTest(unittest.TestCase):
    def test_narrow_hill_contact_removes_the_whole_window_but_keeps_its_wall(self):
        a, b = np.array([0, 0]), np.array([12, 0])
        surfaces = {
            "sidewalk": (
                [[4.45, -0.17, 0], [4.55, -0.17, 0], [4.50, -0.14, 2.4]],
                [[0, 1, 2]],
            ),
        }
        buried = WallGround(surfaces).buried(a, b, 0, 6.4)
        self.assertTrue(buried.covers(shapely.Point(4.5, 2.3)))
        self.assertFalse(buried.intersects(shapely.box(4, 3.3, 5, 6.0)))

        mesh = Meshes()
        draw_facade(
            mesh,
            a,
            b,
            0,
            6.4,
            FacadeView((4.5, -6)),
            grounded=False,
            bays=True,
            buried=buried,
        )
        glass = np.asarray(mesh["glass"][0])
        chosen_bay = glass[(glass[:, 0] > 3.8) & (glass[:, 0] < 5.2)]
        self.assertGreater(len(chosen_bay), 0)
        self.assertGreater(chosen_bay[:, 2].min(), 3.2)
        self.assertTrue(np.any((glass[:, 0] < 3) & (glass[:, 2] < 3.2)))
        points, faces = mesh["wall"]
        triangles = np.asarray(points)[faces]
        wall = shapely.union_all(
            shapely.polygons(
                triangles[np.all(triangles[:, :, 1] == 0, axis=1)][:, :, [0, 2]]
            )
        )
        self.assertTrue(wall.covers(shapely.Point(4.5, 1.4)))
