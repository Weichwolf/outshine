"""Independent screen-polygon footprints and the required prefilter support."""

import math
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from field_view_integrals import summed_field
from field_view_sampling import FieldGrid, ImageView, reconstruct


class SamplingTests(unittest.TestCase):
    def setUp(self):
        self.field = summed_field(
            np.array([[[4, 2, 1, 1]]], dtype=np.float64), dtype=np.float64
        )
        self.grid = FieldGrid((0, 0, 0), (0, 0, 1), 1)
        self.view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 20, 8, 8)

    def test_parallel_pixel_integrals_preserve_energy(self):
        result = reconstruct(self.field, self.grid, self.view)
        self.assertEqual(result["origin"], (1, 1))
        self.assertEqual(result["rgba"].shape, (6, 6, 4))
        np.testing.assert_allclose(
            result["rgba"].sum((0, 1)), [64, 32, 16, 16], rtol=0, atol=1e-12
        )
        self.assertTrue(result["parallel"])
        self.assertLess(result["maximum_rectangle_excess_ratio"], 1e-12)

    def test_prefilter_support_includes_pixels_whose_centres_miss_the_card(self):
        grid = FieldGrid((0.125, -0.125, 0), (0, 0, 1), 1.25)
        result = reconstruct(self.field, grid, self.view)
        x0, y0 = result["origin"]
        alpha = result["rgba"][:, :, 3]
        self.assertAlmostEqual(alpha.sum(), 25)
        self.assertGreater(alpha[3 - y0, 1 - x0], 0)

    def test_nonparallel_approximation_is_explicit_and_checked_against_polygon_area(
        self,
    ):
        angle = math.radians(0.25)
        c, s = math.cos(angle), math.sin(angle)
        view = ImageView((0, 0, 10), ((c, 0, -s), (0, 1, 0), (-s, 0, -c)), 20, 8, 8)
        with self.assertRaisesRegex(ValueError, "explicit"):
            reconstruct(self.field, self.grid, view)
        result = reconstruct(self.field, self.grid, view, approximate=True)
        points = np.array([[-1, 1, 0], [1, 1, 0], [1, -1, 0], [-1, -1, 0]])
        xyz = (points - view.eye) @ np.asarray(view.basis).T
        polygon = shapely.Polygon(4 + xyz[:, :2] * [1, -1] * 20 / xyz[:, 2:])
        low = result["origin"]
        expected = np.empty(result["rgba"].shape[:2])
        for y, x in np.ndindex(expected.shape):
            px, py = x + low[0], y + low[1]
            expected[y, x] = polygon.intersection(
                shapely.box(px, py, px + 1, py + 1)
            ).area
        self.assertFalse(result["parallel"])
        self.assertLess(float(abs(expected - result["rgba"][:, :, 3]).max()), 0.002)

    def test_offscreen_card_uses_no_pixel_work_and_has_a_valid_empty_origin(self):
        grid = FieldGrid((-100, 0, 0), (0, 0, 1), 1)
        result = reconstruct(self.field, grid, self.view)
        self.assertEqual(result["output_pixels"], 0)
        self.assertGreaterEqual(result["origin"][0], 0)
        self.assertEqual(result["logical_texel_fetches"], 0)

    def test_output_capacity_precedes_footprint_expansion(self):
        with (
            patch(
                "field_view_sampling.source_corners",
                side_effect=AssertionError("expansion"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            reconstruct(self.field, self.grid, self.view, pixels_most=35)

    def test_eye_plane_and_invalid_camera_are_not_usable_fields(self):
        with self.assertRaisesRegex(ValueError, "eye plane"):
            reconstruct(
                self.field, self.grid, ImageView((0, 0, 0), self.view.basis, 20, 8, 8)
            )
        with self.assertRaises(ValueError):
            ImageView((0, 0, 10), ((1, 0, 0), (1, 0, 0), (0, 0, -1)), 20)
        with self.assertRaises(ValueError):
            FieldGrid((0, 0, 0), (0, 0, 1), -1)


if __name__ == "__main__":
    unittest.main()
