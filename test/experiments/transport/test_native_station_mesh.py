"""Exact coverage distinguishes real T-junctions from close valid vertices."""

import unittest

import numpy as np
import shapely
from native_station_mesh import check_cut
from planar_layout_reference import hanging_pairs


class CutMeshCoverage(unittest.TestCase):
    def check(self, point, triangles):
        xy = np.array([[0, 0], [2, 0], [2, 2], [0, 2], point]) + 100000.0
        original = np.array([[0, 1, 3], [1, 2, 3]])
        faces = np.array(triangles)
        before = xy, original, np.ones((2, 1), dtype=bool)
        after = xy, faces, np.ones((len(faces), 1), dtype=bool)
        return (
            check_cut(
                before,
                after,
                np.empty((0, len(xy))),
                np.array([0, *([1] * (len(faces) - 1))]),
                [shapely.Polygon(xy[:4])],
                [],
            ),
            xy,
            faces,
        )

    def test_real_missing_edge_vertex_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "mismatched canonical edges"):
            self.check([1, 1], [[0, 1, 3], [1, 2, 4], [4, 2, 3]])

    def test_close_distinct_vertices_do_not_require_a_new_weld_tolerance(self):
        step = 2**-31
        errors, xy, faces = self.check(
            [2 - step, 2 * step],
            [[0, 1, 3], [1, 2, 4], [2, 3, 4], [3, 1, 4]],
        )
        self.assertEqual(errors, [0])
        self.assertGreater(len(hanging_pairs(xy, faces)), 0)
        self.assertTrue(shapely.coverage_is_valid(shapely.polygons(xy[faces])))


if __name__ == "__main__":
    unittest.main()
