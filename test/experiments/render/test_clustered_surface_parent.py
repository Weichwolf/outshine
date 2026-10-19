"""Disappearing-component counterexamples and independent triangle-distance checks."""

import unittest
from dataclasses import replace
from fractions import Fraction

import numpy as np
import trimesh
from check_surface_parent import check_parent
from clustered_surface_parent import cluster_parent
from far_product_cut import projection_error


class SurfaceParentTests(unittest.TestCase):
    def make(self, xyz, triangles, size):
        xyz = np.asarray(xyz, dtype=np.float32)
        triangles = np.asarray(triangles, dtype=np.uint32)
        parent = cluster_parent(xyz, triangles, cell_m=size)
        check_parent(xyz, triangles, parent)
        return xyz, triangles, parent

    def test_isolated_collapsed_component_survives(self):
        xyz, triangles, parent = self.make(
            [[0, 0, 0], [0.01, 0, 0], [0, 0.01, 0]], [[0, 1, 2]], 1
        )
        self.assertEqual(parent.mode.tolist(), [0])
        np.testing.assert_array_equal(
            parent.positions[parent.triangles], xyz[triangles]
        )
        self.assertEqual(parent.error_m, 0)

    def test_collapse_requires_actual_retained_edge(self):
        xyz, triangles, parent = self.make(
            [[0, 0, 0], [0.05, 0, 0], [0, 2, 0], [2, 2, 0]], [[0, 1, 2], [1, 3, 2]], 0.1
        )
        self.assertEqual(parent.mode.tolist(), [3, 1])
        self.assertEqual(len(parent.triangles), 1)
        self.assertGreater(parent.error_m, 0.049)
        # Independent point-to-triangle oracle in both directions, including interiors.
        weights = np.array(
            [
                [a, b, 1 - a - b]
                for a in np.linspace(0, 1, 11)
                for b in np.linspace(0, 1 - a, 11)
            ]
        )
        original = xyz[triangles].astype(float)
        coarse = parent.positions[parent.triangles].astype(float)
        for source, target in ((original, coarse), (coarse, original)):
            points = np.einsum("wc,tcd->twd", weights, source).reshape(-1, 3)
            mesh = trimesh.Trimesh(
                target.reshape(-1, 3),
                np.arange(target.size // 3).reshape(-1, 3),
                process=False,
            )
            distances = trimesh.proximity.closest_point_naive(mesh, points)[1]
            self.assertLessEqual(float(distances.max()), parent.error_m)

    def test_uncovered_collapsed_segment_is_restored(self):
        _, _, parent = self.make(
            [[0, 0, 0], [0.05, 0, 0], [0, 2, 0], [4, 0, 0], [6, 0, 0], [4, 2, 0]],
            [[0, 1, 2], [3, 4, 5]],
            0.1,
        )
        self.assertEqual(parent.mode.tolist(), [0, 1])

    def test_unused_vertex_cannot_expand_reference_enclosure(self):
        _, _, parent = self.make(
            [[-0.01, 0, 0], [0, 0, 0], [2, 0, 0], [0, 2, 0]], [[1, 2, 3]], 0.1
        )
        self.assertFalse((parent.original_vertex_ids == 0).any())
        self.assertEqual(parent.error_m, 0)

    def test_oriented_duplicates_and_reverse_faces(self):
        _, _, parent = self.make(
            [[0, 0, 0], [2, 0, 0], [0, 2, 0]], [[0, 1, 2], [1, 2, 0], [0, 2, 1]], 0.1
        )
        self.assertEqual(len(parent.triangles), 2)
        self.assertEqual(parent.mode.tolist(), [1, 2, 1])

    def test_exact_rational_corner_bound(self):
        xyz, triangles, parent = self.make(
            [[10000, 0, 0], [10000.03125, 0.0625, 0], [10000, 4, 0], [10004, 4, 0]],
            [[0, 1, 2], [1, 3, 2]],
            0.125,
        )
        bound2 = Fraction(parent.error_m) ** 2
        for row, mode in enumerate(parent.mode):
            if mode == 0:
                continue
            for a, b in zip(
                xyz[triangles[row]], xyz[parent.corner_map[row]], strict=True
            ):
                exact = sum(
                    (Fraction(float(x)) - Fraction(float(y))) ** 2
                    for x, y in zip(a, b, strict=True)
                )
                self.assertLessEqual(exact, bound2)

    def test_corrupt_witness_fails(self):
        xyz, triangles, parent = self.make(
            [[0, 0, 0], [0.05, 0, 0], [0, 2, 0], [2, 2, 0]], [[0, 1, 2], [1, 3, 2]], 0.1
        )
        with self.assertRaises(AssertionError):
            check_parent(xyz, triangles, replace(parent, error_m=0))
        images = parent.corner_map.copy()
        images[0] = [1, 2, 3]
        with self.assertRaises(AssertionError):
            check_parent(xyz, triangles, replace(parent, corner_map=images))

    def test_budget_and_invalid_inputs_do_not_drop_original_product(self):
        xyz = np.array([[0.0, 0, 0], [2, 0, 0], [0, 2, 0]])
        ids = np.array([[0, 1, 2]], dtype=np.uint32)
        for kwargs in ({"max_triangles": 0}, {"cell_m": float("inf")}):
            with self.assertRaises(ValueError):
                cluster_parent(xyz, ids, **({"cell_m": 0.1} | kwargs))

    def test_tight_displacement_can_pass_where_enclosure_diameter_cannot(self):
        box = np.array([[-50.0, -20, 1000], [50, 20, 1020]])
        observer = np.zeros((2, 3))
        bound = projection_error(
            box, observer, displacement_m=0.25, focal_px=1000, width=1280, height=720
        )
        self.assertLess(bound, 1)
        self.assertGreater(bound, 0.25)
        self.assertTrue(
            np.isinf(
                projection_error(
                    box, box, displacement_m=0.25, focal_px=1000, width=1280, height=720
                )
            )
        )


if __name__ == "__main__":
    unittest.main()
