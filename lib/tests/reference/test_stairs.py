import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.transport.stairs import fit_stairway
from outshine.world.mesh import Meshes, ground_surfaces, terrain_grid
from outshine.world.scene import support_height
from shapely.geometry import LineString, box


class StairwayTest(unittest.TestCase):
    def test_ground_without_stairs_keeps_its_walkable_surface(self):
        source = SimpleNamespace(height=lambda xy: np.zeros(len(xy)), features=[])
        meshes = Meshes()
        vertices, quads = terrain_grid(source, 4)
        ground_surfaces(
            meshes, source, box(-4, -4, 4, 4), shapely.Polygon(), vertices, quads
        )
        self.assertEqual(set(meshes), {"ground"})
        self.assertEqual(support_height(meshes["ground"]).max(), 0)

    def test_steep_turn_off_a_landing_gets_treads_and_a_terrain_apron(self):
        source = SimpleNamespace(
            height=lambda xy: (
                0.25 * np.maximum(xy[:, 1] - 3, 0)
                + 0.55 * np.maximum(-xy[:, 0] - 0.9, 0)
            ),
            features=[
                (
                    "transportation",
                    LineString([(0, 0), (0, 3), (0, 7)]),
                    {"class": "path", "subclass": "steps"},
                ),
                (
                    "transportation",
                    LineString([(0, 0), (-3.6, 0.9)]),
                    {"class": "path", "subclass": "footway"},
                ),
            ],
        )
        meshes = Meshes()
        vertices, quads = terrain_grid(source, 9)
        ground_surfaces(
            meshes, source, box(-9, -9, 9, 9), shapely.Polygon(), vertices, quads
        )
        z = lambda x, y: support_height(meshes["sidewalk"], (x, y)).max()
        for y in np.linspace(0.1, 1.7, 9):
            self.assertAlmostEqual(z(-0.89, y), z(-0.91, y), places=5)
            self.assertLess(abs(z(-3.59, y) - z(-3.61, y)), 0.01)
        heights = {round(z(x, 0.9), 6) for x in np.linspace(-3.58, -0.92, 201)}
        self.assertEqual(len(heights), 9)
        np.testing.assert_allclose(np.diff(sorted(heights)), 1.485 / 8, atol=1e-6)

    def test_shared_landing_meets_a_third_flight_across_its_side(self):
        source = SimpleNamespace(
            height=lambda xy: (
                0.18 * np.maximum(abs(xy[:, 1]) - 2, 0)
                + 0.09 * np.maximum(-xy[:, 0], 0)
            ),
            features=[
                (
                    "transportation",
                    LineString(points),
                    {"class": "path", "subclass": role},
                )
                for points, role in (
                    ([(0, -5), (0, -2)], "steps"),
                    ([(0, 2), (0, 5)], "steps"),
                    ([(-4, 0), (-0.9, 0)], "steps"),
                    ([(0, -2), (0, 2)], "footway"),
                )
            ],
        )
        meshes = Meshes()
        vertices, quads = terrain_grid(source, 6)
        ground_surfaces(
            meshes, source, box(-6, -6, 6, 6), shapely.Polygon(), vertices, quads
        )
        for y in np.linspace(-0.8, 0.8, 17):
            tread = support_height(meshes["sidewalk"], (-0.902, y)).max()
            landing = support_height(meshes["sidewalk"], (-0.898, y)).max()
            self.assertLess(abs(tread - landing), 0.001)

    def test_oblique_footway_meets_the_full_stair_port(self):
        source = SimpleNamespace(
            height=lambda xy: 0.07 * xy[:, 0] + 0.08 * xy[:, 1],
            features=[
                (
                    "transportation",
                    LineString([(0, 0), (4, 0)]),
                    {"class": "path", "subclass": "steps"},
                ),
                (
                    "transportation",
                    LineString([(-3, -1), (0, 0)]),
                    {"class": "path", "subclass": "footway"},
                ),
            ],
        )
        meshes = Meshes()
        vertices, quads = terrain_grid(source, 5)
        ground_surfaces(
            meshes, source, box(-5, -5, 5, 5), shapely.Polygon(), vertices, quads
        )
        for y in np.linspace(-0.8, 0.8, 17):
            self.assertAlmostEqual(
                support_height(meshes["sidewalk"], (0.002, y)).max(), 0
            )
            self.assertLess(
                abs(support_height(meshes["sidewalk"], (-0.002, y)).max()), 0.003
            )

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
