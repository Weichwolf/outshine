"""Avoid the known GEOS rectangle operation only when the hull proves its answer."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.buildings._rectangle import minimum_rectangle
from outshine_ref.buildings.structures import footprint_metrics
from shapely import affinity


class RectangleBoundsTests(unittest.TestCase):
    def test_already_owned_coordinates_are_borrowed_without_a_second_decode(self):
        source = shapely.box(-3, 7, 9, 12)
        points = shapely.get_coordinates(source)
        frozen = points.copy()
        points.setflags(write=False)
        with patch("shapely.get_coordinates", side_effect=AssertionError):
            result = minimum_rectangle(source, points)
        np.testing.assert_array_equal(points, frozen)
        self.assertTrue(result.normalize().equals_exact(source.normalize(), 0))

    def test_rectangle_retains_source_srid_and_precision_model(self):
        source = shapely.set_srid(
            shapely.set_precision(shapely.box(0, 0, 8, 6), 0.25), 3857
        )
        result = minimum_rectangle(source)
        self.assertEqual(shapely.get_srid(result), 3857)
        self.assertEqual(shapely.get_precision(result), 0.25)
        self.assertTrue(result.normalize().equals_exact(source.normalize(), 0))

    def test_axis_rectangle_hulls_do_not_call_minimum_rectangle_backend(self):
        rectangle = shapely.box(-3, 7, 9, 12)
        inputs = (
            rectangle,
            shapely.Polygon(
                rectangle.exterior.coords, [[(-1, 8), (2, 8), (2, 10), (-1, 10)]]
            ),
            shapely.MultiPoint([*rectangle.exterior.coords, (0, 9), (3, 11)]),
            shapely.Polygon(
                [(-3, 7), (9, 7), (9, 12), (4, 12), (4, 10), (0, 10), (0, 12), (-3, 12)]
            ),
        )
        for source in inputs:
            with (
                self.subTest(source=source.wkt),
                patch(
                    "shapely.minimum_rotated_rectangle",
                    side_effect=RuntimeWarning("backend failure"),
                ),
            ):
                actual = minimum_rectangle(source)
            self.assertTrue(actual.normalize().equals_exact(rectangle.normalize(), 0))
            self.assertTrue(actual.covers(source))
            self.assertEqual(actual.area, 60)

    def test_points_and_lines_retain_exact_normalized_degenerate_geometry(self):
        for source in (shapely.Point(3, 7), shapely.LineString([(1, 2), (6, 2)])):
            with patch("shapely.minimum_rotated_rectangle", side_effect=AssertionError):
                actual = minimum_rectangle(source)
            self.assertTrue(actual.normalize().equals_exact(source.normalize(), 0))

    def test_non_axis_and_nearly_axis_hulls_keep_backend_and_warnings(self):
        shapes = (
            affinity.rotate(shapely.box(0, 0, 12, 5), 31),
            shapely.Polygon([(0, 0), (2, 0), (2, 1), (np.nextafter(0.0, 1.0), 1)]),
            shapely.Polygon([(0, 0), (2, 0), (1, 1)]),
        )
        for source in shapes:
            with (
                self.subTest(source=source.wkt),
                patch(
                    "shapely.minimum_rotated_rectangle",
                    side_effect=RuntimeWarning("backend failure"),
                ),
                self.assertRaisesRegex(RuntimeWarning, "backend failure"),
            ):
                minimum_rectangle(source)

    def test_unmodified_geos_results_match_for_non_axis_shapes(self):
        shapes = (
            affinity.rotate(shapely.box(-3, -2, 8, 5), 23),
            shapely.Polygon([(0, 0), (8, 0), (9, 4), (4, 7), (0, 3)]),
            shapely.Polygon(),
        )
        for source in shapes:
            with self.subTest(source=source.wkt):
                expected = shapely.minimum_rotated_rectangle(source)
                self.assertTrue(minimum_rectangle(source).equals_exact(expected, 0))

    def test_structure_metrics_share_the_known_rectangle_path(self):
        with patch("shapely.minimum_rotated_rectangle", side_effect=AssertionError):
            result = footprint_metrics(shapely.box(0, 0, 8, 6), {}, 0, 0)
        self.assertEqual((result.width_m, result.length_m), (6, 8))


if __name__ == "__main__":
    unittest.main()
