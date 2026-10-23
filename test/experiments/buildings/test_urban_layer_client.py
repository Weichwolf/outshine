"""Sample differences can cancel in the actual filtered image."""

import unittest

import numpy as np
from compare_urban_layers import rgb_errors


class LayerClientTest(unittest.TestCase):
    def test_opposite_subpixel_errors_cancel_only_after_filtering(self):
        values = np.repeat([[1.0], [0.0], [1.0], [0.0]], 3, axis=1)
        actual = rgb_errors(values, np.full_like(values, 0.5), 1, 1, 2)
        self.assertEqual(actual["sample_rgb_rmse"], 0.5)
        self.assertEqual(actual["pixel_rgb_rmse"], 0)

    def test_opposite_pixel_errors_do_not_cancel_across_the_image(self):
        values = np.repeat([[1.0], [1.0], [0.0], [0.0]] * 2, 3, axis=1)
        actual = rgb_errors(values, np.full_like(values, 0.5), 2, 1, 2)
        self.assertEqual(actual["sample_rgb_rmse"], 0.5)
        self.assertEqual(actual["pixel_rgb_rmse"], 0.5)


if __name__ == "__main__":
    unittest.main()
