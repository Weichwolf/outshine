"""Independent Terrarium pixel encoding, bilinear interpolation, precedence and source errors."""

import hashlib
import tempfile
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.places import Elevation
from PIL import Image


class Heights(unittest.TestCase):
    def receipt(self, root, tile, heights):
        encoded = np.asarray(heights, dtype=np.int64) + 32768
        rgb = np.stack(
            [encoded // 256, encoded % 256, np.zeros_like(encoded)], axis=-1
        ).astype(np.uint8)
        path = root / ("-".join(map(str, tile)) + ".webp")
        Image.fromarray(rgb).save(path, lossless=True)
        return {
            "kind": "elevation",
            "tile": tile,
            "path": str(path),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }

    def test_true_terrarium_samples_and_bilinear_height(self):
        with tempfile.TemporaryDirectory(prefix="outshine-buildings-height-") as folder:
            root = Path(folder)
            receipt = self.receipt(root, [0, 0, 0], [[0, 10], [20, 30]])
            sampler = Elevation([receipt])
            self.assertAlmostEqual(float(sampler.sample(0.0, -90.0)), 12.5)
            self.assertAlmostEqual(float(sampler.sample(0.0, 90.0)), 17.5)
            np.testing.assert_allclose(
                sampler.sample([0.0, 0.0], [-90.0, 90.0]), [12.5, 17.5]
            )

    def test_finest_available_camera_height_and_explicit_missing_child(self):
        with tempfile.TemporaryDirectory(prefix="outshine-buildings-height-") as folder:
            root = Path(folder)
            coarse = self.receipt(root, [0, 0, 0], [[0, 10], [20, 30]])
            fine = self.receipt(root, [1, 0, 1], [[80, 80], [80, 80]])
            self.assertEqual(float(Elevation([coarse, fine]).sample(0.0, -90.0)), 80.0)
            missing = {"kind": "elevation", "tile": [1, 0, 1], "missing": True}
            self.assertAlmostEqual(
                float(Elevation([coarse, missing]).sample(0.0, -90.0)), 12.5
            )

    def test_source_tampering_and_coverage_holes_stay_errors(self):
        with tempfile.TemporaryDirectory(prefix="outshine-buildings-height-") as folder:
            receipt = self.receipt(Path(folder), [1, 0, 1], [[80, 80], [80, 80]])
            with self.assertRaises(ValueError):
                Elevation([receipt]).sample(0.0, 90.0)
            Path(receipt["path"]).write_bytes(b"changed")
            with self.assertRaises(ValueError):
                Elevation([receipt]).sample(0.0, -90.0)
            with self.assertRaises(ValueError):
                Elevation([]).sample(float("nan"), 0.0)


if __name__ == "__main__":
    unittest.main()
