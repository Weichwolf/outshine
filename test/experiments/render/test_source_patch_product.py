"""Whole native part roundtrip, empty material slots and unchanged signed words."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_patch_product import FIELDS, SLICES, read_product, write_product


class SourcePatchProduct(unittest.TestCase):
    def test_existing_native_parts_preserve_every_corner_word_and_source(self):
        corners = np.arange(42, dtype="<f4").reshape(1, 3, 14)
        corners[0, 0, 6] = -0.0
        other = corners.copy()
        other[0, 0, 6] = 0.0
        parts = tuple(
            {"material": index, "vertices": 0, "triangles": 0} for index in range(3)
        )
        chunks = {
            12: (corners, np.array([0], np.int8)),
            34: (other, np.array([2], np.int8)),
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.source-patch-product."
        ) as directory:
            root = Path(directory) / "candidate"
            report = write_product(chunks, {12: 1, 34: 0}, {12, 34}, parts, root)
            geometry = root / "geometry/0"
            saved = json.loads((geometry / "input.json").read_text())["parts"]
            self.assertEqual([p["triangles"] for p in saved], [1, 0, 1])
            for material, expected in ((0, corners), (2, other)):
                values = np.column_stack(
                    [
                        np.fromfile(
                            geometry / str(material) / (field + ".bin"), "<f4"
                        ).reshape(-1, columns.stop - columns.start)
                        for field, columns in zip(FIELDS, SLICES, strict=True)
                    ]
                )
                faces = np.fromfile(
                    geometry / str(material) / "indices.bin", "<u4"
                ).reshape(-1, 3)
                self.assertEqual(values[faces].tobytes(), expected.tobytes())
            with np.load(root / "face-sources.npz", allow_pickle=False) as labels:
                np.testing.assert_array_equal(
                    labels["part0_prepared_source_ordinals"], [12]
                )
                np.testing.assert_array_equal(
                    labels["part2_prepared_source_ordinals"], [34]
                )
                self.assertEqual(labels["part1_prepared_source_ordinals"].size, 0)
            self.assertEqual(len(report["native_file_sha256"]), 18)
            self.assertEqual(
                report["native_description_sha256"],
                hashlib.sha256((geometry / "input.json").read_bytes()).hexdigest(),
            )
            self.assertEqual(
                report["prepared_source_face_binding_sha256"],
                hashlib.sha256((root / "face-sources.npz").read_bytes()).hexdigest(),
            )
            self.assertEqual(report["prepared_source_face_binding_bytes"], 8)
            replay = read_product(root, report, {12, 34})
            self.assertEqual(len(replay), 3)
            np.testing.assert_array_equal(replay[0][2], [12])
            labels_path = root / "face-sources.npz"
            labels_path.write_bytes(labels_path.read_bytes() + b"changed")
            with self.assertRaisesRegex(ValueError, "hash changed"):
                read_product(root, report, {12, 34})

    def test_missing_original_source_is_not_a_valid_partial_product(self):
        with (
            tempfile.TemporaryDirectory(
                prefix="outshine-tests.source-patch-product."
            ) as directory,
            self.assertRaisesRegex(ValueError, "Complete bound"),
        ):
            write_product(
                {}, {}, {12}, ({"material": 0},) * 3, Path(directory) / "candidate"
            )


if __name__ == "__main__":
    unittest.main()
