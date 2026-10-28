"""Exact crossing and permitted shared-topology controls on binary coordinates."""

import unittest

import numpy as np
from road_closed_exact import intersects, qualify_intersections


class StoredTriangleIntersections(unittest.TestCase):
    def test_crossings_coplanar_overlap_and_shared_vertex_overlap_are_rejected(self):
        a = ((0, 0, 0), (4, 0, 0), (0, 4, 0))
        for b, shared in (
            (((1, 1, -1), (1, 1, 1), (3, 3, 0)), []),
            (((1, 1, 0), (4, 1, 0), (1, 4, 0)), []),
            (((0, 0, 0), (3, 0, 0), (0, 3, 0)), [a[0]]),
        ):
            self.assertTrue(intersects(a, b, shared))
            self.assertTrue(intersects(b, a, shared))

    def test_only_shared_index_edge_or_vertex_contacts_are_permitted(self):
        a = ((0, 0, 0), (4, 0, 0), (0, 4, 0))
        for b, shared in (
            (((0, 0, 0), (0, -4, 0), (4, 0, 0)), [a[0], a[1]]),
            (((0, 0, 0), (0, 0, 4), (-4, 0, 0)), [a[0]]),
        ):
            self.assertFalse(intersects(a, b, shared))
            self.assertFalse(intersects(b, a, shared))
            self.assertTrue(intersects(a, b, []))

    def test_pair_cap_counts_broad_phase_work_and_exact_dyadic_coordinates(self):
        vertices = np.array(
            [[0, 0, 0], [0.5, 0, 0], [0, 0.25, 0], [0, -0.25, 0]], dtype=np.float32
        )
        faces = np.array([[0, 1, 2], [1, 0, 3]], dtype=np.uint32)
        report = qualify_intersections(vertices, faces, max_pairs=1)
        self.assertEqual(report["integer_grid_denominator_bits"], 2)
        self.assertEqual(report["exact_triangle_pairs"], 1)
        self.assertEqual(report["self_intersections"], 0)
        with self.assertRaisesRegex(ValueError, "work_budget"):
            qualify_intersections(vertices, np.tile(faces, (2, 1)), max_pairs=1)
        for budget in (True, 0, 65537):
            with self.assertRaisesRegex(ValueError, "limit"):
                qualify_intersections(vertices, faces, max_pairs=budget)


if __name__ == "__main__":
    unittest.main()
