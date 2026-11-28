import unittest
from functools import partial
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.transport.road_tunnels import PREFERRED_CLEARANCE, plan_road_tunnels
from outshine.world.mesh import assemble, terrain_grid, terrain_height
from outshine.world.scene import support_height


class RoadTunnelTest(unittest.TestCase):
    def test_both_mouths_join_the_roads_and_carry_the_surface_road_above(self):
        tag = {"class": "secondary"}
        source = SimpleNamespace(
            height=lambda xy: 6 * np.maximum(1 - (xy[:, 0] / 39) ** 2, 0),
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000, 111000]),
            features=[
                (
                    "transportation",
                    shapely.LineString([(-48, 0), (48, 0)]),
                    tag | {"brunnel": "tunnel", "layer": -1},
                ),
                ("transportation", shapely.LineString([(-85, 0), (-48, 0)]), tag),
                ("transportation", shapely.LineString([(48, 0), (85, 0)]), tag),
                ("transportation", shapely.LineString([(0, -15), (0, 15)]), tag),
                (
                    "transportation",
                    shapely.LineString([(8, -45), (8, 45)]),
                    {"class": "rail"},
                ),
            ],
        )
        terrain, quads = terrain_grid(source, 90)
        [tunnel] = plan_road_tunnels(
            source.features,
            shapely.box(-90, -90, 90, 90),
            [],
            shapely.Polygon(),
            [],
            terrain,
            quads,
            partial(terrain_height, terrain),
        )
        self.assertEqual(len(shapely.get_parts(tunnel.openings)), 2)
        self.assertFalse(tunnel.openings.intersects(shapely.Point(0, 0)))
        meshes = assemble(source, 90)
        for y in (-2.5, 0, 2.5):
            road = support_height(meshes["road"], (0, y))
            ceiling = support_height(meshes["bridge deck"], (0, y))
            self.assertAlmostEqual(ceiling.min() - road.min(), PREFERRED_CLEARANCE)
            self.assertGreater(road.max(), ceiling.max())
        for x in (7.2475, 8.7525):
            ballast = support_height(meshes["rail ballast"], (x, 0)).max()
            roof = support_height(meshes["bridge deck"], (x, 0)).min()
            rail = support_height(meshes["rail head"], (x, 0)).max()
            self.assertGreater(ballast, roof)
            self.assertAlmostEqual(rail - ballast, 0.18 + 0.172)
        for x in tunnel.points[[0, -1], :, 0].mean(axis=1):
            for y in (-2.5, 0, 2.5):
                self.assertAlmostEqual(
                    support_height(meshes["road"], (x - 0.01, y)).min(), 0
                )
                self.assertAlmostEqual(
                    support_height(meshes["road"], (x + 0.01, y)).min(), 0
                )
