import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.buildings.entrances import (
    APPROACH_DEPTH_M,
    DOOR_HEIGHT_M,
    frontages,
    pave_entrances,
)
from outshine.buildings.facade import FacadeView
from outshine.buildings.street_facade import draw_low_front
from outshine.terrain.ground_surface import surface_elevations
from outshine.terrain.wall_ground import WallGround
from outshine.world.mesh import Meshes


class LowFacadeTest(unittest.TestCase):
    def test_frontage_uses_a_clear_path_and_lands_on_the_solved_pad(self):
        polygon = shapely.orient_polygons(shapely.box(-7, 0, 7, 15))
        model = SimpleNamespace(
            foundation=SimpleNamespace(apron_m=6),
            bottom=0.50,
            top=4.50,
            polygon=polygon,
            passages=(),
            group=None,
        )
        path = [
            (
                "transportation",
                shapely.LineString([(-8, -8), (8, -8)]),
                {"class": "path"},
            )
        ]
        ground = {
            "ground": (
                [(-12, -10, 0), (12, -10, 0), (12, 0, 0), (-12, 0, 0)],
                [[0, 1, 2], [0, 2, 3]],
            )
        }
        self.assertFalse(
            frontages([model], [[]], path, shapely.box(-10, -6, 10, -3), ground)
        )
        (front,) = frontages([model], [[]], path, shapely.Polygon(), ground).values()
        raised = ground | {
            "sidewalk": (
                [(-0.08, -0.15, 0), (0.08, -0.15, 0), (0, -0.12, 0.9)],
                [[0, 1, 2]],
            )
        }
        self.assertFalse(
            frontages(
                [model], [[]], path, shapely.Polygon(), raised, WallGround(raised)
            )
        )
        corners = np.array(polygon.exterior.coords)
        a, b = corners[front.side : front.side + 2]
        meshes = Meshes()
        draw_low_front(meshes, a, b, 0.50, 4.50, "wall", FacadeView((0, -9)), front)
        door = np.asarray(meshes["entrance door"][0])
        stairs = np.asarray(meshes["foundation"][0])
        np.testing.assert_allclose(
            [door[:, 2].min(), door[:, 2].max()], [0.50, 0.50 + DOOR_HEIGHT_M]
        )
        self.assertGreater(door[:, 1].min(), 0)
        self.assertAlmostEqual(float(stairs[:, 1].min()), -APPROACH_DEPTH_M)
        self.assertAlmostEqual(float(stairs[:, 2].max()), 0.50)
        self.assertLess(stairs[:, 2].min(), front.ground)
        windows = np.asarray(meshes["glass"][0])
        self.assertGreater(windows[:, 2].min(), 1.50)
        self.assertLess(windows[:, 2].max(), door[:, 2].max())
        meshes.add("ground", *ground["ground"])
        pave_entrances(meshes, {0: front})
        self.assertEqual(surface_elevations(meshes["paving"], [[0, -5]])[0], 0)
        self.assertEqual(surface_elevations(meshes["ground"], [[0, -5]])[0], -np.inf)
