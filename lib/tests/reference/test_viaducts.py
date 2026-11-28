import unittest

import numpy as np
import shapely
from outshine.transport.viaducts import SLAB_DEPTH, draw_viaduct, local_viaducts
from outshine.world.mesh import Meshes


class ViaductTest(unittest.TestCase):
    def test_shared_deck_carries_both_motorway_lanes_above_crossing_road(self):
        outline = shapely.box(-40, -8, 40, 8)
        lanes = [shapely.LineString([(-40, y), (40, y)]) for y in [-3.8, 3.8]]
        crossing = shapely.LineString([(0, -12), (0, 12)])
        features = [
            (
                "transportation",
                outline,
                {"class": "bridge", "brunnel": "bridge", "layer": 1},
            ),
            *[
                (
                    "transportation",
                    lane,
                    {"class": "motorway", "brunnel": "bridge", "layer": 2},
                )
                for lane in lanes
            ],
            ("transportation", crossing, {"class": "minor"}),
        ]

        def ground(points):
            return 0.02 * points[:, 0]

        [deck] = local_viaducts(features, shapely.box(-25, -25, 25, 25), ground)
        self.assertEqual(deck.outline_layer, 1)
        self.assertEqual(deck.carriageways, {id(lane) for lane in lanes})
        self.assertAlmostEqual(deck.grade, 0.02)
        sample = np.array([[0, -8], [0, 0], [0, 8]])
        np.testing.assert_allclose(
            deck.height(sample) - SLAB_DEPTH - ground(sample), 4.95
        )
        self.assertTrue(deck.lower_routes.covers(shapely.Point(0, 0)))

        meshes = Meshes()
        draw_viaduct(meshes, deck, ground)
        projected = []
        for name in ("bridge deck", "road"):
            vertices, triangles = meshes[name]
            faces = np.asarray(vertices)[triangles]
            xy = faces[:, :, :2]
            raised = np.isclose(
                faces[:, :, 2], deck.height(xy.reshape(-1, 2)).reshape(-1, 3)
            ).all(axis=1)
            projected.extend(shapely.polygons(xy[raised]))
        self.assertAlmostEqual(shapely.union_all(projected).area, outline.area)
        self.assertAlmostEqual(shapely.area(projected).sum(), outline.area)
