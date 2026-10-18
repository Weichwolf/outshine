"""Independent polygon integrals, finite sources, periodic means and mip controls."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from asphalt_angular import aggregates, angular_tile, coverage, mip_fields


class AngularAsphalt(unittest.TestCase):
    def test_independent_triangle_and_diamond_pixel_integrals(self):
        triangle = np.array([[0, 0], [1, 0], [0, 1]])
        diamond = np.array([[0.5, -0.25], [1.25, 0.5], [0.5, 1.25], [-0.25, 0.5]])
        for polygon, expected in ((triangle, 0.5), (diamond, 0.875)):
            actual = coverage(polygon, (0, 0, 1, 1), 1, "area")
            np.testing.assert_allclose(actual, [[expected]], atol=1e-14, rtol=0)

    def test_complete_and_disjoint_source_pixels(self):
        whole = np.array([[-1, -1], [2, -1], [2, 2], [-1, 2]])
        outside = whole + [4, 0]
        np.testing.assert_array_equal(coverage(whole, (0, 0, 1, 1), 1, "area"), [[1]])
        np.testing.assert_array_equal(coverage(outside, (0, 0, 1, 1), 1, "area"), [[0]])

    def test_translated_pixel_has_same_half_triangle_integral(self):
        triangle = np.array([[0, 0], [1, 0], [0, 1]]) + [7, -3]
        np.testing.assert_allclose(
            coverage(triangle, (7, -3, 8, -2), 1, "area"), [[0.5]], rtol=0, atol=1e-14
        )

    def test_numpy_point_membership_is_an_independent_edge_control(self):
        triangle = np.array([[0, 0], [1, 0], [0, 1]])
        np.testing.assert_array_equal(
            coverage(triangle, (0, 0, 2, 2), 2, "point"), [[True, True], [True, False]]
        )

    def test_sources_keep_bounded_centres_radii_and_immutable_vertices(self):
        for item in aggregates(8, 4):
            polygon = shapely.Polygon(item.vertices)
            self.assertTrue(polygon.is_valid and polygon.area > 0)
            self.assertLessEqual(len(item.vertices), 8)
            self.assertTrue(
                np.all(
                    np.linalg.norm(item.vertices - item.centre, axis=1) <= 0.22 + 1e-12
                )
            )
            self.assertTrue(np.all(item.centre - np.floor(item.centre) >= 0.2 - 1e-12))
            self.assertTrue(np.all(item.centre - np.floor(item.centre) <= 0.8 + 1e-12))
            self.assertFalse(item.vertices.flags.writeable)
            self.assertFalse(item.centre.flags.writeable)

    def test_periodic_source_geometry_translates_without_a_new_recipe(self):
        items = list(aggregates(8, 1))
        for axis in (0, 1):
            delta = np.eye(2)[axis] * 8
            for item in items:
                if not 0 < item.centre[axis] < 1:
                    continue
                matching = next(
                    other
                    for other in items
                    if np.linalg.norm(other.centre - item.centre - delta) < 1e-12
                )
                np.testing.assert_allclose(
                    matching.vertices - delta, item.vertices, rtol=0, atol=1e-12
                )
                self.assertEqual(matching.weight, item.weight)

    def test_all_exact_mips_equal_independent_direct_area_integrals(self):
        field, _ = angular_tile(8, 1, 32, "area")
        for level in mip_fields(field):
            direct, _ = angular_tile(8, 1, len(level), "area")
            np.testing.assert_allclose(level, direct, rtol=0, atol=1e-12)
            self.assertLess(abs(level.mean()), 1e-12)

    def test_numerical_dc_projection_preserves_authored_mean(self):
        for density in (1, 4):
            field, _ = angular_tile(8, density, 32, "ss32-dc")
            self.assertLess(abs(field.mean()), 1e-12)

    def test_invalid_inputs_are_rejected_before_source_geometry(self):
        with patch(
            "asphalt_angular.grain", side_effect=AssertionError("premature grain")
        ):
            for kwargs in (
                {"period": 7},
                {"density": 3},
                {"resolution": 2048},
                {"resolution": 3},
                {"method": "unknown"},
                {"binder": "unknown"},
            ):
                with self.assertRaises(ValueError):
                    angular_tile(**kwargs)


if __name__ == "__main__":
    unittest.main()
