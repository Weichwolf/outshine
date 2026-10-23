"""Exact coverage distinguishes real T-junctions from close valid vertices."""

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

import numpy as np
import shapely
from native_station_mesh import check_cut, compare_reference
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


class StationReferenceConsumer(unittest.TestCase):
    def compare(self, *, corrupt_parent=False):
        # Analytic x=1 split of a triangle; independent expected output words.
        xy = np.array([[0, 0], [2, 0], [0, 2]], dtype=float)
        result = np.vstack([xy, [[1, 0], [1, 1]]])
        before = xy, np.array([[0, 1, 2]]), np.ones((1, 1), dtype=bool)
        after = (
            result,
            np.array([[0, 3, 4], [0, 4, 2], [3, 1, 4]]),
            np.ones((3, 1), dtype=bool),
        )
        with TemporaryDirectory(prefix="outshine-tests.independent-flights.") as tmp:
            path = Path(tmp) / "axes.txt"
            path.write_text("1\n0 0 1 0 1 1\n")
            return compare_reference(
                before,
                after,
                path,
                np.array([[0, 2, 0, 1, 1]], dtype=float),
                np.array([0, 0, int(corrupt_parent)]),
            )

    def test_existing_reference_consumer_matches_analytic_station_split(self):
        result = self.compare()
        self.assertTrue(result["position_words_equal"])
        self.assertTrue(result["faces_equal"])
        self.assertTrue(result["parents_equal"])
        self.assertTrue(result["station_words_equal"])

    def test_existing_reference_still_rejects_incorrect_planar_parent(self):
        with self.assertRaisesRegex(ValueError, "differs from unchanged Python cut"):
            self.compare(corrupt_parent=True)


if __name__ == "__main__":
    unittest.main()
