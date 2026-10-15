"""Pixel-centre, adjacent-tile and retained-source checks for coarse height input."""

import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from capture_height_targets import GIRTH_M, sample_targets, terrain_tile
from PIL import Image


def local_xy(world):
    return (np.asarray(world) - [0.5, 0.5]) * [GIRTH_M, -GIRTH_M]


class HeightSources(unittest.TestCase):
    def test_affine_raster_at_pixel_centres(self):
        rows, columns = np.meshgrid(np.arange(512), np.arange(512), indexing="ij")
        heights = columns + rows * 2
        pixels = np.array([[10.2, 33.4], [205.0, 404.0]])
        xy = local_xy((pixels + 0.5) / 512)
        with patch("capture_height_targets.terrain_tile", return_value=(heights, {})):
            values, _ = sample_targets([0.0, 0.0], xy, Path("unused"), 0)
        np.testing.assert_allclose(values, pixels[:, 0] + 2 * pixels[:, 1], atol=1e-7)

    def test_tile_boundary_uses_actual_neighbour_pixels(self):
        def tile(sources, coordinate, fetch):
            _, x, y = coordinate
            columns, rows = np.meshgrid(np.arange(512), np.arange(512))
            return columns + x * 512 + (rows + y * 512) * 2, {"tile": list(coordinate)}

        xy = local_xy([[0.5, 0.5]])
        with patch("capture_height_targets.terrain_tile", side_effect=tile):
            values, receipts = sample_targets([0.0, 0.0], xy, Path("unused"), 1)
        self.assertEqual(len(receipts), 4)
        self.assertAlmostEqual(values[0], 511.5 * 3)

    def test_longitude_wrap_and_no_network_without_fetch(self):
        heights = np.ones((512, 512))
        with patch("capture_height_targets.terrain_tile", return_value=(heights, {})):
            values, _ = sample_targets(
                [0.0, 0.0], local_xy([[1.0, 0.5], [0.0, 0.5]]), Path("unused"), 1
            )
        np.testing.assert_array_equal(values, [1.0, 1.0])
        with (
            tempfile.TemporaryDirectory(
                prefix="outshine-tests.outshine_transport-height."
            ) as directory,
            patch("outshine_ref.places.sources.subprocess.check_output") as network,
        ):
            with self.assertRaises(FileNotFoundError):
                terrain_tile(Path(directory), (1, 0, 0), False)
            network.assert_not_called()

    def test_terrarium_decoding_and_reuse(self):
        pixels = np.full((512, 512, 3), [128, 12, 128], dtype=np.uint8)
        buffer = io.BytesIO()
        Image.fromarray(pixels).save(buffer, format="WEBP", lossless=True)
        with (
            tempfile.TemporaryDirectory(
                prefix="outshine-tests.outshine_transport-height."
            ) as directory,
            patch(
                "outshine_ref.places.sources.subprocess.check_output",
                return_value=buffer.getvalue() + b"\n200",
            ) as network,
        ):
            heights, receipt = terrain_tile(Path(directory), (1, 0, 0), True)
            self.assertEqual(receipt["origin"], "network")
            np.testing.assert_array_equal(heights, np.full((512, 512), 12.5))
            _, repeated = terrain_tile(Path(directory), (1, 0, 0), True)
            self.assertEqual(repeated["origin"], "retained")
            self.assertEqual(repeated["sha256"], receipt["sha256"])
            network.assert_called_once()


if __name__ == "__main__":
    unittest.main()
