"""Analytic pixel areas and a narrow opening invisible to four point samples."""

import unittest

import numpy as np
from compare_group_coverage import front_source
from triangle_coverage_reference import four_samples, integrate

VIEW = (np.zeros(3), np.eye(3), 1.0, 2, 2)
BOX = np.array([[0, 0], [2, 2]])


def source(triangles):
    points = np.asarray(triangles, dtype=float).reshape(-1, 2)
    positions = np.column_stack(
        (points[:, 0] - 1, 1 - points[:, 1], np.ones(len(points)))
    )
    return {"positions": positions, "indices": np.arange(len(points)).reshape(-1, 3)}


def rectangle(a, b, c, d):
    return [[(a, b), (c, b), (c, d)], [(a, b), (c, d), (a, d)]]


class TriangleCoverage(unittest.TestCase):
    def test_front_selection_uses_geometric_winding(self):
        geometry = source(rectangle(0, 0, 1, 1))
        front = geometry["indices"].copy()
        geometry["indices"] = np.vstack((front, front[:, ::-1]))
        selected, _ = front_source(geometry, VIEW[0])
        np.testing.assert_array_equal(selected["indices"], front)

    def test_half_pixel_and_complete_area_partition(self):
        areas, silhouette, work = integrate(
            source([[(0, 0), (1, 0), (0, 1)]]), VIEW, BOX
        )
        np.testing.assert_array_equal(areas, [[0.5, 0], [0, 0]])
        self.assertEqual(silhouette.area, 0.5)
        self.assertEqual(work["area_sum_residual_px"], 0)

    def test_overlapping_triangles_count_union_instead_of_area_sum(self):
        triangles = rectangle(0, 0, 2, 2)
        areas, _, _ = integrate(source(triangles + triangles), VIEW, BOX)
        np.testing.assert_array_equal(areas, np.ones((2, 2)))

    def test_opening_is_missed_by_all_four_samples(self):
        triangles = rectangle(0, 0, 0.375, 1) + rectangle(0.625, 0, 1, 1)
        areas, silhouette, _ = integrate(source(triangles), VIEW, BOX)
        np.testing.assert_array_equal(areas, [[0.75, 0], [0, 0]])
        np.testing.assert_array_equal(
            four_samples(silhouette, areas.shape), [[1, 0], [0, 0]]
        )

    def test_thin_opaque_strip_is_missed_by_all_four_samples(self):
        areas, silhouette, _ = integrate(
            source(rectangle(0.4375, 0, 0.5625, 1)), VIEW, BOX
        )
        self.assertEqual(areas.sum(), 0.125)
        self.assertEqual(four_samples(silhouette, areas.shape).sum(), 0)

    def test_clipping_and_edge_on_triangle_preserve_finite_roi(self):
        triangles = rectangle(-1, -1, 3, 3) + [[(0, 0), (1, 1), (2, 2)]]
        areas, _, work = integrate(source(triangles), VIEW, BOX)
        np.testing.assert_array_equal(areas, np.ones((2, 2)))
        self.assertEqual(work["positive_projected_triangles"], 2)

    def test_sub_roi_uses_global_pixel_coordinates(self):
        areas, _, _ = integrate(
            source(rectangle(0, 0, 2, 2)), VIEW, np.array([[1, 1], [2, 2]])
        )
        np.testing.assert_array_equal(areas, [[1]])

    def test_near_plane_and_bad_indices_decline_whole_input(self):
        geometry = source(rectangle(0, 0, 1, 1))
        geometry["positions"][0, 2] = 0.1
        with self.assertRaisesRegex(ValueError, "near plane"):
            integrate(geometry, VIEW, BOX)
        geometry = source(rectangle(0, 0, 1, 1))
        geometry["indices"][0, 0] = -1
        with self.assertRaisesRegex(ValueError, "indexed triangles"):
            integrate(geometry, VIEW, BOX)


if __name__ == "__main__":
    unittest.main()
