"""Contained circulation, deterministic equal routes and local bounded failures."""

import unittest

import numpy as np
import shapely
from outshine_ref.buildings.interiors.routes import graph


class CirculationGridTests(unittest.TestCase):
    def setUp(self):
        self.space = shapely.box(0, 0, 12, 12).difference(shapely.box(3, 3, 9, 9))

    def test_equal_length_hole_routes_keep_the_lower_boundary_choice(self):
        paths, work = graph(self.space, np.array([(1.0, 6.0), (11.0, 6.0)]))
        self.assertEqual(len(paths), 1)
        route = paths[0]
        self.assertTrue(self.space.covers(route))
        self.assertEqual(route.length, 16.0)
        self.assertEqual(tuple(route.coords[0]), (1.0, 6.0))
        self.assertEqual(tuple(route.coords[-1]), (11.0, 6.0))
        self.assertEqual(route.bounds, (1.0, 3.0, 11.0, 6.0))
        self.assertEqual(work, {"candidates": 49, "grid_nodes": 30, "grid_edges": 48})

    def test_all_anchors_join_one_contained_tree_without_duplicate_branches(self):
        anchors = np.array([(1.0, 1.0), (11.0, 1.0), (11.0, 11.0), (1.0, 11.0)])
        paths, work = graph(self.space, anchors)
        self.assertEqual(len(paths), 3)
        self.assertEqual([path.length for path in paths], [10.0, 10.0, 10.0])
        connected = shapely.union_all(paths)
        self.assertTrue(self.space.covers(connected))
        self.assertTrue(connected.covers(shapely.MultiPoint(anchors)))
        self.assertEqual(connected.length, 30.0)
        self.assertEqual(work, {"candidates": 60, "grid_nodes": 36, "grid_edges": 60})

    def test_boundary_and_duplicate_anchors_keep_valid_zero_work_paths(self):
        paths, _ = graph(self.space, np.array([(3.0, 3.0), (9.0, 9.0)]))
        self.assertEqual(list(paths[0].coords), [(3.0, 3.0), (9.0, 3.0), (9.0, 9.0)])
        self.assertTrue(self.space.covers(paths[0]))
        paths, _ = graph(self.space, np.array([(1.0, 1.0), (1.0, 1.0)]))
        self.assertEqual(paths, [])
        paths, work = graph(shapely.Point(1.0, 1.0), np.array([(1.0, 1.0)] * 2))
        self.assertEqual(paths, [])
        self.assertEqual(work, {"candidates": 0, "grid_nodes": 1, "grid_edges": 0})

    def test_illegal_or_disconnected_anchors_do_not_get_an_invented_route(self):
        for target in ((6.0, 6.0), (30.0, 1.0)):
            with self.subTest(target=target):
                paths, work = graph(self.space, np.array([(1.0, 1.0), target]))
                self.assertIsNone(paths)
                self.assertEqual(work["reason"], "anchor outside the route grid")
        space = shapely.MultiPolygon([shapely.box(0, 0, 2, 2), shapely.box(4, 0, 6, 2)])
        paths, work = graph(space, np.array([(1.0, 1.0), (5.0, 1.0)]))
        self.assertIsNone(paths)
        self.assertEqual(work["reason"], "bounded grid does not reach every anchor")

    def test_anchor_budget_remains_explicit_and_does_not_truncate_requests(self):
        anchors = np.c_[np.arange(39.0), np.ones(39)]
        with self.assertRaisesRegex(ValueError, "too many circulation anchors"):
            graph(shapely.box(0, 0, 40, 2), anchors)

    def test_rejected_gap_is_not_measured_or_allowed_to_overflow(self):
        anchors = np.array([(0.0, 0.0), (1e200, 0.0)])
        with np.errstate(over="raise", invalid="raise"):
            paths, work = graph(shapely.MultiPoint(anchors), anchors)
        self.assertIsNone(paths)
        self.assertEqual(work["reason"], "bounded grid does not reach every anchor")


if __name__ == "__main__":
    unittest.main()
