"""Missing radiance must remain visible and cannot become zero-error agreement."""

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

import numpy as np
from compare_reconstructed_field import save_images
from PIL import Image


class FieldImageTest(unittest.TestCase):
    def test_one_missing_subsample_disqualifies_its_pixel(self):
        original = np.full((8, 3), 0.25)
        candidate = original.copy()
        candidate[0] = np.nan
        missing = np.zeros(8, bool)
        missing[0] = True
        with TemporaryDirectory(prefix="outshine-tests.field-image.") as directory:
            output = Path(directory)
            result = save_images(
                original,
                candidate,
                missing,
                (None, None, None, 2, 1),
                np.array([[0, 0], [2, 1]]),
                2,
                1,
                output,
            )
            self.assertFalse(result["global_image_qualified"])
            self.assertEqual(result["qualified_pixels"], 1)
            self.assertEqual(result["unresolved_pixels"], 1)
            image = np.asarray(Image.open(output / "field.png"))
            np.testing.assert_array_equal(image[0, 0], [255, 0, 255])
            missing[:] = True
            result = save_images(
                original,
                candidate,
                missing,
                (None, None, None, 2, 1),
                np.array([[0, 0], [2, 1]]),
                2,
                1,
                output,
            )
            self.assertIsNone(result["max_rgb8_error_on_qualified_pixels"])
