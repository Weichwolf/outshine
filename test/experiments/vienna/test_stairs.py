import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from reference_scene import support_height
from scene_mesh import Meshes, ground_surfaces, terrain_grid
from shapely.geometry import LineString, box
from stairs import fit_stairway


class StairwayTest(unittest.TestCase):
    def test_turn_has_landings_and_uniform_risers_between_fixed_endpoints(self):
        line = LineString([(-3, 0), (0, 0), (0, 3)])
        stair = fit_stairway(line, 1.8, lambda xy: [0, 1, 2])
        self.assertIsNotNone(stair)
        self.assertAlmostEqual(stair.footprint.area, 6 * 1.8)
        self.assertAlmostEqual(shapely.area(stair.treads).sum(), stair.footprint.area)
        self.assertEqual((stair.heights[0], stair.heights[-1]), (0, 2))
        risers = np.diff(stair.heights)
        self.assertGreaterEqual(risers.min(), 0)
        np.testing.assert_allclose(risers[risers != 0], 2 / 12)
        # The tread at the turn shares the full cross section with both flights.
        turn = shapely.Point(0, 0)
        self.assertEqual(sum(shape.covers(turn) for shape in stair.treads), 2)

    def test_terrain_is_cut_and_the_stairway_carries_walkable_treads(self):
        route = LineString([(-2, 0), (2, 0)])
        source = SimpleNamespace(
            height=lambda xy: 0.25 * xy[:, 0],
            features=[
                ("transportation", route, {"class": "path", "subclass": "steps"})
            ],
        )
        meshes = Meshes()
        vertices, quads = terrain_grid(source, 4)
        ground_surfaces(
            meshes, source, box(-4, -4, 4, 4), shapely.Polygon(), vertices, quads
        )
        vertices, indices = meshes["sidewalk"]
        triangles = np.asarray(vertices)[indices]
        flat = np.ptp(triangles[:, :, 2], axis=1) == 0
        self.assertTrue(np.any(~flat))
        self.assertAlmostEqual(
            shapely.area(shapely.polygons(triangles[flat, :, :2])).sum(), 4 * 1.8
        )
        self.assertEqual(triangles[flat, :, 2].min(), -0.5)
        self.assertEqual(triangles[flat, :, 2].max(), 0.5)
        self.assertAlmostEqual(support_height(meshes["sidewalk"], [1.8, 0]).max(), 0.5)
        ground, indices = meshes["ground"]
        area = shapely.area(shapely.polygons(np.asarray(ground)[indices, :2])).sum()
        self.assertAlmostEqual(area, 8 * 8 - 4 * 1.8, places=5)
