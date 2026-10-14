"""Independent affine minima and projection/work-budget counterexamples."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance


def triangle(xy, plane):
    xy = np.asarray(xy, float)
    return Mesh(np.c_[xy, xy @ plane[:2] + plane[2]], [[0, 1, 2]])


def surface(domain, plane):
    pieces = shapely.get_parts(shapely.constrained_delaunay_triangles(domain))
    xy = shapely.get_coordinates(pieces).reshape(-1, 4, 2)[:, :3]
    xy, labels = np.unique(xy.reshape(-1, 2), axis=0, return_inverse=True)
    return Mesh(np.c_[xy, xy @ plane[:2] + plane[2]], labels.reshape(-1, 3))


class ClearanceTests(unittest.TestCase):
    def test_minimum_at_new_intersection_vertex_not_input_samples(self):
        lower = triangle([[0, 0], [4, 0], [0, 4]], [0, 0, 0])
        upper = triangle([[2, -1], [5, 2], [2, 5]], [1, -1, 5])
        result = vertical_clearance(lower, upper)
        self.assertTrue(result["valid"], result)
        self.assertEqual(result["minimum_clearance_m"], 5.0)
        np.testing.assert_array_equal(result["witness_xy"], [2, 2])
        self.assertFalse(
            any(
                np.array_equal(v[:2], result["witness_xy"])
                for m in (lower, upper)
                for v in m.vertices
            )
        )

    def test_negative_gap_and_winding_are_supported(self):
        lower = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, 2])
        upper = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, 1])
        for a, b in ((lower, upper), (Mesh(lower.vertices, [[2, 1, 0]]), upper)):
            result = vertical_clearance(a, b)
            self.assertTrue(result["valid"])
            self.assertEqual(result["minimum_clearance_m"], -1.0)

    def test_thickness_opposed_crossfalls_and_tiny_positive_overlap(self):
        domain = shapely.box(0, 0, 2, 2)
        lower = surface(domain, [0, 0.02, 0])
        underside = surface(domain, [0, -0.02, 4.6 - 0.35])
        self.assertAlmostEqual(
            vertical_clearance(lower, underside)["minimum_clearance_m"], 4.17
        )
        lower, upper = surface(domain, [0, 0, 0]), surface(domain, [0, 0, 1])
        for offset, valid in ((2.0, False), (2.0 - 1e-13, True)):
            shifted = Mesh(upper.vertices + [offset, 0, 0], upper.triangles)
            self.assertEqual(vertical_clearance(lower, shifted)["valid"], valid)

    def test_point_edge_and_disjoint_shapes_have_no_numeric_gap(self):
        lower = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, 0])
        for xy in (
            [[1, 0], [2, 0], [1, 1]],
            [[0, 0], [1, 0], [1, -1]],
            [[3, 0], [4, 0], [3, 1]],
        ):
            result = vertical_clearance(lower, triangle(xy, [0, 0, 2]))
            self.assertFalse(result["valid"])
            self.assertEqual(result["reason"], "no_positive_area_overlap")
            self.assertNotIn("minimum_clearance_m", result)

    def test_holes_and_all_disconnected_parts_participate(self):
        ring = shapely.box(0, 0, 10, 10).difference(shapely.box(2, 2, 8, 8))
        lower = surface(ring, [0, 0, 0])
        hole = surface(shapely.box(4, 4, 6, 6), [0, 0, 9])
        self.assertEqual(
            vertical_clearance(lower, hole)["reason"], "no_positive_area_overlap"
        )
        domain = shapely.union_all((shapely.box(0, 0, 1, 1), shapely.box(3, 0, 4, 1)))
        result = vertical_clearance(
            surface(domain, [0, 0, 0]), surface(domain, [-1, 0, 9])
        )
        self.assertEqual(result["minimum_clearance_m"], 5.0)

    def test_candidate_budget_counts_returned_pairs_not_all_upper_faces(self):
        lower = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, 0])
        domain = shapely.union_all(
            (shapely.Polygon(lower.vertices[:, :2]), shapely.box(10, 0, 11, 1))
        )
        upper = surface(domain, [0, 0, 3])
        result = vertical_clearance(lower, upper, max_pairs=1)
        self.assertTrue(result["valid"], result)
        self.assertEqual(result["checks"], 1)
        self.assertEqual(result["minimum_clearance_m"], 3.0)
        disjoint = surface(shapely.box(20, 0, 21, 1), [0, 0, 3])
        result = vertical_clearance(lower, disjoint, max_pairs=0)
        self.assertEqual(result["reason"], "no_positive_area_overlap")

    def test_zero_work_and_face_caps_are_explicit_failures(self):
        mesh = surface(shapely.box(0, 0, 1, 1), [0, 0, 1])
        for kwargs, reason in (
            ({"seconds": 0}, "clearance_time_budget"),
            ({"max_pairs": 0}, "clearance_pair_budget"),
            ({"max_faces": 1}, "clearance_face_budget"),
        ):
            result = vertical_clearance(mesh, mesh, **kwargs)
            self.assertFalse(result["valid"])
            self.assertEqual(result["reason"], reason)

    def test_degenerate_projection_and_overflow_have_no_numeric_certificate(self):
        mesh = Mesh([[0, 0, 0], [1, 0, 1], [2, 0, 0]], [[0, 1, 2]])
        result = vertical_clearance(mesh, mesh)
        self.assertFalse(result["valid"])
        self.assertEqual(result["reason"], "clearance_degenerate_projection")
        low = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, 1e308])
        high = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, -1e308])
        for a, b in ((low, high), (high, low)):
            result = vertical_clearance(a, b)
            self.assertFalse(result["valid"])
            self.assertEqual(result["reason"], "clearance_nonfinite_gap")
            self.assertNotIn("minimum_clearance_m", result)

    def test_invalid_limits_and_unselected_input_are_programmer_errors(self):
        mesh = triangle([[0, 0], [1, 0], [0, 1]], [0, 0, 0])
        for kwargs in (
            {"max_pairs": True},
            {"max_pairs": -1},
            {"max_faces": False},
            {"max_faces": 65537},
            {"seconds": float("nan")},
            {"seconds": -1},
        ):
            with self.assertRaises(ValueError):
                vertical_clearance(mesh, mesh, **kwargs)
        with self.assertRaises(TypeError):
            vertical_clearance(None, mesh)


if __name__ == "__main__":
    unittest.main()
