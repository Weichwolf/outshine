import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.terrain.ground_surface import ROAD_WIDTH
from outshine.transport.heights import _face_gradients
from outshine.transport.road_grades import fit_road_grades
from outshine.transport.route_graph import (
    RouteAttributes,
    RouteGraph,
    RouteNode,
    RoutePath,
)
from outshine.transport.route_tiles import grid_point
from outshine.world.mesh import assemble, terrain_grid
from outshine.world.scene import support_height


class RoadGradeTest(unittest.TestCase):
    def test_rendered_road_and_shoulders_meet_at_the_fitted_height(self):
        domain = ("land", 0, "", "", False)
        tags = {"class": "secondary"}
        path = RoutePath(
            ((14, 0, 0), 1),
            RouteAttributes(
                "secondary", domain, (1, -1), frozenset({"motor"}), tuple(tags.items())
            ),
            tuple(RouteNode(grid_point(x / 111000, 0), domain) for x in (-20, 20)),
        )
        source = SimpleNamespace(
            height=lambda p: 2.0 * np.maximum(1.0 - (p[:, 0] / 7) ** 2, 0.0),
            features=[
                ("transportation", shapely.LineString([(-20, 0), (20, 0)]), tags)
            ],
            transport_graph=RouteGraph((path,), (), (), (), ()),
            metric=lambda lon, lat: 111000 * np.column_stack((lon, lat)),
        )
        product = assemble(source, 24)
        self.assertLess(support_height(product["road"], (0, 0)).max(), 1.5)
        for x in (-5, 0, 5):
            for edge in (-ROAD_WIDTH["secondary"] / 2, ROAD_WIDTH["secondary"] / 2):
                road = support_height(product["road"], (x, edge))
                soil = support_height(product["ground"], (x, edge))
                self.assertAlmostEqual(road.max(), soil.max(), places=7)

    def test_steep_connected_pavement_uses_one_surface_with_fixed_terrain_edges(self):
        terrain = SimpleNamespace(
            height=lambda p: 2.0 * np.maximum(1.0 - (p[:, 0] / 7) ** 2, 0.0)
        )
        vertices, quads = terrain_grid(terrain, 24)
        road = shapely.box(-12, -2.5, 12, 2.5).union(shapely.box(-2.5, -8, 2.5, 8))
        result = fit_road_grades(vertices, quads, {0.12: road}, shapely.Polygon())
        self.assertGreater(np.max(abs(result.vertices[:, 2] - vertices[:, 2])), 0.5)
        self.assertGreater(len(result.faces), 0)
        self.assertEqual(result.unresolved_faces, 0)
        xy = vertices[:, :2]
        boundary = (abs(xy) == 24).any(axis=1)
        np.testing.assert_array_equal(result.vertices[boundary], vertices[boundary])
        np.testing.assert_array_equal(result.vertices[:, :2], xy)
        grades = _face_gradients(xy, result.faces, result.vertices[:, 2])
        self.assertTrue(np.all(np.linalg.norm(grades, axis=1) <= result.limits + 1e-7))

    def test_existing_structure_contacts_keep_their_elevation(self):
        terrain = SimpleNamespace(height=lambda p: p[:, 0] * 0.32)
        vertices, quads = terrain_grid(terrain, 24)
        road = shapely.box(-20, -3, 20, 3)
        result = fit_road_grades(vertices, quads, {0.1: road}, road.buffer(2))
        np.testing.assert_array_equal(result.vertices, vertices)
        self.assertEqual(len(result.faces), 0)
        self.assertGreater(result.unresolved_faces, 0)
