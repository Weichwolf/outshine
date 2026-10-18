"""Exact null cut facets retain oriented parent normals without changing geometry."""

import unittest
from fractions import Fraction

import numpy as np
import shapely
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.cut_normals import triangle_normals
from outshine_ref.buildings.rooflight_panels import triangle_surface


class CutNormals(unittest.TestCase):
    def setUp(self):
        self.original = {
            "vertices": np.array([[0.0, 0, 0], [2.0, 0, 0], [0.0, 2, 0]]),
            "faces": np.array([[0, 1, 2]]),
        }
        self.null = np.array([[0.0, 0, 0], [0.0, 0, 1], [0.0, 0, 2]])
        self.faces = np.array([[0, 1, 2]])

    def test_null_normals_follow_original_or_reversed_cutter_and_do_not_move_vertices(
        self,
    ):
        old = self.null.copy()
        source = triangle_normals(self.null, self.faces, self.original, [], [0], [0])
        np.testing.assert_array_equal(source, [[0.0, 0, 1]])
        cutter = PrismVolume("cut", shapely.box(0, 0, 1, 1), 0, 1)
        result = triangle_normals(
            self.null, self.faces, self.original, [cutter], [1], [0]
        )
        np.testing.assert_array_equal(result, [[-1.0, 0, 0]])
        np.testing.assert_array_equal(self.null, old)

    def test_invalid_or_underflowing_area_cannot_be_relabelled_as_an_exact_null_face(
        self,
    ):
        for points in (
            self.original["vertices"] * 1e-120,
            self.original["vertices"] + np.nan,
        ):
            with self.assertRaisesRegex(ValueError, "unresolved"):
                triangle_normals(points, self.faces, self.original, [], [0], [0])
            with self.assertRaisesRegex(ValueError, "unresolved"):
                triangle_surface({"vertices": points, "faces": self.faces}, 1)
        with self.assertRaisesRegex(ValueError, "parent"):
            triangle_normals(
                self.null,
                self.faces,
                dict(self.original, vertices=self.null),
                [],
                [0],
                [0],
            )

    def test_draw_omits_only_exact_null_facets_and_preserves_the_area_bearing_triangle(
        self,
    ):
        points = np.r_[self.original["vertices"], self.null]
        faces = np.array([[0, 1, 2], [3, 4, 5]])
        source = triangle_surface(self.original, 7)
        draw = triangle_surface({"vertices": points, "faces": faces}, 7)
        for key in source:
            np.testing.assert_array_equal(draw[key], source[key])
        np.testing.assert_array_equal(
            points, np.r_[self.original["vertices"], self.null]
        )
        empty = triangle_surface({"vertices": self.null, "faces": self.faces}, 7)
        self.assertEqual(empty["faces"].shape, (0, 3))
        self.assertTrue(all(np.isfinite(value).all() for value in empty.values()))

    def test_floating_cross_cancellation_is_not_proof_of_exact_null_geometry(self):
        a = 2.0**-27
        points = np.array([[0.0, 0, 0], [1 + a, 1, 0], [1 + 2 * a, 1 + a, 0]])
        floating = np.cross(points[1] - points[0], points[2] - points[0])
        np.testing.assert_array_equal(floating, [0, 0, 0])
        exact = Fraction(1 + a) ** 2 - Fraction(1 + 2 * a)
        self.assertEqual(exact, Fraction(1, 2**54))
        for axes in ((0, 1, 2), (1, 2, 0), (2, 0, 1)):
            placed = points[:, axes]
            with self.assertRaisesRegex(ValueError, "cancellation"):
                triangle_normals(placed, self.faces, self.original, [], [0], [0])
            with self.assertRaisesRegex(ValueError, "cancellation"):
                triangle_surface({"vertices": placed, "faces": self.faces}, 7)


if __name__ == "__main__":
    unittest.main()
