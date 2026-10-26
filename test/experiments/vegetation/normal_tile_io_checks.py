"""Actual signed Float32 UDIM IO and independent prepared-field integrity controls."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import bpy
import numpy as np
from normal_view_images import ViewImages
from normal_view_tiles import load_tiles, write_tiles


class TileIOTests(unittest.TestCase):
    def test_independent_images_only_read_and_copy_the_demanded_view(self):
        values = np.full((64, 32, 32, 3), np.nan, np.float32)
        values[9] = [0.25, -0.125, 0.3]
        factory = ViewImages(9)
        image = factory(values)
        pixels = np.asarray(image.pixels[:], np.float32).reshape(32, 32, 4)
        np.testing.assert_array_equal(pixels[..., :3], values[9])
        self.assertEqual(factory.float_payload_bytes, 16384)
        self.assertFalse(image.use_half_precision)
        bpy.data.images.remove(image)
        with self.assertRaisesRegex(ValueError, "finite demanded"):
            ViewImages(0)(values)

    def test_every_tile_retains_exact_signed_float_values_and_hit_does_not_rewrite(
        self,
    ):
        values = np.zeros((64, 32, 32, 3), np.float32)
        values[..., 0] = np.arange(64)[:, None, None] / 64 - 0.5
        values[..., 1] = -0.125
        values[..., 2] = 0.3
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-tile-io-", dir="/tmp"
        ) as tmp:
            folder = Path(tmp) / "field"
            write_tiles(values, folder)
            for view in range(64):
                image = bpy.data.images.load(
                    str(folder / f"field.{1001 + view}.exr"), check_existing=False
                )
                image.colorspace_settings.name = "Non-Color"
                decoded = np.asarray(image.pixels[:], np.float32).reshape(32, 32, 4)
                np.testing.assert_array_equal(decoded[..., :3], values[view])
                np.testing.assert_array_equal(decoded[..., 3], 1)
                bpy.data.images.remove(image)
            with patch(
                "normal_view_tiles.write_tiles", side_effect=AssertionError("rewrite")
            ):
                image = load_tiles(values, folder)
            self.assertEqual(len(image.tiles), 64)
            self.assertFalse(image.use_half_precision)
            self.assertEqual(
                sum(t.size[0] * t.size[1] * t.channels * 4 for t in image.tiles),
                1048576,
            )
            bpy.data.images.remove(image)
            with (folder / "field.1064.exr").open("ab") as stream:
                stream.write(b"changed")
            with self.assertRaisesRegex(ValueError, "tile changed"):
                load_tiles(values, folder)

    def test_invalid_field_refuses_before_output_and_existing_products_are_not_overwritten(
        self,
    ):
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-tile-io-", dir="/tmp"
        ) as tmp:
            folder = Path(tmp) / "invalid"
            with self.assertRaises(ValueError):
                write_tiles(np.zeros((63, 32, 32, 3)), folder)
            self.assertFalse(folder.exists())
            folder.mkdir()
            with self.assertRaises(FileExistsError):
                write_tiles(np.zeros((64, 32, 32, 3)), folder)


if __name__ == "__main__":
    unittest.main()
