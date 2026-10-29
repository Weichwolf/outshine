"""Independent plane/depth example and explicit unmatched first-layer samples."""

import unittest

import numpy as np
from field_view_geometry import compare_geometry
from field_view_inputs import TEXEL
from field_view_sampling import FieldGrid, ImageView


class GeometryTests(unittest.TestCase):
    def setUp(self):
        texels = np.zeros((3, 3), TEXEL)
        texels[1, 1] = ((0, 0, 1), 0.625, 1)
        self.atlas = {
            "grid": FieldGrid((0, 0, 0), (0, 0, 1), 4),
            "pixels": 3,
            "texels": texels,
        }
        self.view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 10, 1, 1)
        self.depth = np.array([[0.1 / 8]])
        self.normal = np.array([[[0.0, 0, 1, -1]]])

    def test_native_reversed_depth_recovers_height_without_flattening(self):
        result = compare_geometry(self.atlas, self.view, self.depth, self.normal)
        self.assertEqual(result["matched_centre_samples"], 1)
        self.assertAlmostEqual(
            result["plane_forward_depth_absolute_m"]["maximum"], 2, places=6
        )
        self.assertLess(result["first_layer_forward_depth_absolute_m"]["maximum"], 2e-7)
        self.assertEqual(result["first_layer_normal_angle_deg"]["maximum"], 0)

    def test_capture_miss_remains_a_reported_original(self):
        self.atlas["texels"][1, 1] = ((0, 0, 0), 0, 0)
        result = compare_geometry(self.atlas, self.view, self.depth, self.normal)
        self.assertEqual(result["original_centre_hits"], 1)
        self.assertEqual(result["original_hits_without_captured_surface"], 1)
        self.assertIsNone(result["first_layer_forward_depth_absolute_m"])

    def test_masked_background_and_invalid_foreground_are_distinct(self):
        result = compare_geometry(
            self.atlas, self.view, np.zeros((1, 1)), np.full((1, 1, 4), np.nan)
        )
        self.assertEqual(result["original_centre_hits"], 0)
        with self.assertRaises(ValueError):
            compare_geometry(self.atlas, self.view, self.depth, np.zeros((1, 1, 4)))


if __name__ == "__main__":
    unittest.main()
