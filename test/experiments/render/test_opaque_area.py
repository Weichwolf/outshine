"""Analytic opaque area, changing depth order and polygon integration controls."""

import unittest

import numpy as np
import shapely
from native_capture_meshes import independent_depth
from opaque_area_grid import AreaGrid, integration_points, visible_fragments


def fragments(candidates, planes):
    work = {
        "fragment_pair_tests": 0,
        "visible_fragments": 0,
        "coincident_comparisons": 0,
    }
    return visible_fragments(candidates, np.asarray(planes, float), (0, 0), work)


class AreaTests(unittest.TestCase):
    def test_projected_triangle_area_and_front_winding(self):
        source = {
            "positions": np.array([[0, 0, 2], [0, 2, 2], [2, 0, 2]], float),
            "indices": np.array([[0, 1, 2]]),
        }
        grid = AreaGrid(source, np.zeros(3), np.eye(3), [0, 0], [1, 1], [1, 1])
        _, pieces = next(grid.fragments())
        self.assertEqual(sum(p.area for _, p in pieces), 0.5)
        source["indices"] = np.array([[0, 2, 1]])
        back = AreaGrid(source, np.zeros(3), np.eye(3), [0, 0], [1, 1], [1, 1])
        self.assertEqual(list(back.fragments()), [])

    def test_nearest_and_coincident_face_identity(self):
        square = shapely.box(0, 0, 1, 1)
        pieces = fragments([(0, square), (1, square)], [[0, 0, 1], [0, 0, 2]])
        self.assertEqual([(f, p.area) for f, p in pieces], [(1, 1)])
        pieces = fragments([(1, square), (0, square)], [[0, 0, 1], [0, 0, 1]])
        self.assertEqual([(f, p.area) for f, p in pieces], [(0, 1)])

    def test_depth_order_changes_inside_pixel(self):
        square = shapely.box(0, 0, 1, 1)
        # r0=x+1 and r1=1.5 cross exactly at x=0.5.
        pieces = fragments([(0, square), (1, square)], [[1, 0, 1], [0, 0, 1.5]])
        by_face = dict(pieces)
        self.assertAlmostEqual(by_face[0].area, 0.5)
        self.assertAlmostEqual(by_face[1].area, 0.5)
        self.assertGreater(by_face[0].centroid.x, 0.5)
        self.assertLess(by_face[1].centroid.x, 0.5)

    def test_ring_hole_exposes_background(self):
        square, hole = shapely.box(0, 0, 1, 1), shapely.box(0.25, 0.25, 0.75, 0.75)
        ring = shapely.difference(square, hole)
        pieces = fragments([(0, ring), (1, square)], [[0, 0, 2], [0, 0, 1]])
        by_face = dict(pieces)
        self.assertAlmostEqual(by_face[0].area, 0.75)
        self.assertAlmostEqual(by_face[1].area, 0.25)
        for refined in (False, True):
            xy, weight = integration_points(by_face[0], refined=refined)
            self.assertAlmostEqual(weight.sum(), 0.75)
            self.assertFalse(shapely.contains_xy(hole, xy[:, 0], xy[:, 1]).any())

    def test_quadrature_integrates_quadratic_and_multi_polygon(self):
        square = shapely.box(0, 0, 1, 1)
        multi = shapely.MultiPolygon(
            [shapely.box(0, 0, 0.4, 1), shapely.box(0.6, 0, 1, 1)]
        )
        for refined in (False, True):
            xy, weight = integration_points(square, refined=refined)
            np.testing.assert_allclose(weight @ xy, [0.5, 0.5], atol=1e-14)
            self.assertAlmostEqual(weight @ (xy[:, 0] ** 2), 1 / 3)
            self.assertAlmostEqual(weight @ (xy[:, 0] * xy[:, 1]), 1 / 4)
            xy, weight = integration_points(multi, refined=refined)
            self.assertAlmostEqual(weight.sum(), 0.8)

    def test_fragment_depth_against_independent_world_triangle_intersection(self):
        xy = np.array([[0, 0], [0, 1], [1, 0]], float)
        planes = np.array([[0.3, 0.1, 0.3], [-0.2, 0.1, 0.55], [0.1, -0.2, 0.5]])
        depth = 1 / (planes[:, :2] @ xy.T + planes[:, 2:])
        triangles = np.dstack((xy[None] * depth[:, :, None], depth))
        source = {
            "positions": triangles.reshape(-1, 3),
            "indices": np.arange(9).reshape(3, 3),
        }
        grid = AreaGrid(source, np.zeros(3), np.eye(3), [0, 0], [1, 1], [1, 1])
        for _, pieces in grid.fragments():
            for face, polygon in pieces:
                points, _ = integration_points(polygon)
                rays = np.column_stack((points, np.ones(len(points))))
                exact = independent_depth(triangles, np.zeros_like(rays), rays)
                expected = 1 / (points @ planes[face, :2] + planes[face, 2])
                np.testing.assert_allclose(exact, expected, rtol=1e-13)


if __name__ == "__main__":
    unittest.main()
