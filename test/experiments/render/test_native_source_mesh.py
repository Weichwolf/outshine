"""Native source parsing must preserve identity and reject false geometry/replay."""

import copy
import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

import numpy as np
from native_source_mesh import face_sources, load_source, read_capture


class NativeSourceMeshTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="outshine-fine-stream-test-"
        )
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        vertex = np.zeros((3, 6), "<u4")
        vertex[:, :3] = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], "<f4").view("<u4")
        self.arrays = {
            "walls": vertex,
            "roofs": vertex,
            "wall_indices": np.arange(3, dtype="<u4"),
            "roof_indices": np.arange(3, dtype="<u4"),
            "colours": np.empty(0, "<f4"),
            "metres": np.empty(0, "<f4"),
        }
        self.original = [
            {"original_source_ordinal": 11, "original_source_id": [0, 0]},
            {"original_source_ordinal": 42, "original_source_id": [2, 99]},
        ]
        self.metadata = {"original_sources": self.original}
        self.write_streams()

    def write_streams(self):
        specs = {}
        for capture in (0, 1):
            directory = self.root / "mesh" / str(capture)
            directory.mkdir(parents=True, exist_ok=True)
            for name, array in self.arrays.items():
                data = array.tobytes()
                (directory / f"{name}.bin").write_bytes(data)
                specs[name] = {
                    "bytes": len(data),
                    "count": len(array) if array.ndim == 2 else array.size,
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
        calls = [
            {
                "native_call": {
                    "source": 1,
                    "detail": 0,
                    "accepted": True,
                    "openings": True,
                },
                "wall_indices": [0, 3],
                "roof_indices": [0, 0],
            },
            {
                "native_call": {
                    "source": 0,
                    "detail": 0,
                    "accepted": True,
                    "openings": True,
                },
                "wall_indices": [3, 3],
                "roof_indices": [0, 3],
            },
        ]
        self.receipt = {
            "captures": [specs, copy.deepcopy(specs)],
            "original_sources": self.original,
            "call_ranges": [calls, copy.deepcopy(calls)],
        }
        self.save_receipt()
        (self.root / "input.json").write_text(json.dumps(self.metadata))

    def save_receipt(self):
        (self.root / "mesh/receipt.json").write_text(json.dumps(self.receipt))

    def test_original_streams_and_unknown_source_ids_survive(self):
        source = load_source(self.root)
        np.testing.assert_array_equal(source["indices"], [[0, 1, 2], [3, 4, 5]])
        np.testing.assert_array_equal(
            source["positions"][:3], [[0, 0, 0], [1, 0, 0], [0, 1, 0]]
        )
        np.testing.assert_array_equal(face_sources(self.root, source), [1, 0])
        self.assertEqual(source["metadata"]["original_sources"], self.original)
        self.assertEqual(
            source["metadata"]["original_sources"][0]["original_source_id"], [0, 0]
        )

    def test_unsigned_roof_index_cannot_wrap_into_wall_vertices(self):
        self.arrays["roof_indices"][0] = 2**32 - 1
        self.write_streams()
        with self.assertRaisesRegex(ValueError, "index outside its own vertices"):
            load_source(self.root)

    def test_source_range_cannot_extend_past_its_stream(self):
        source = load_source(self.root)
        self.receipt["call_ranges"][0][0]["wall_indices"][1] = 6
        self.save_receipt()
        with self.assertRaisesRegex(ValueError, "range"):
            face_sources(self.root, source)

    def test_unbound_source_index_and_changed_identity_are_rejected(self):
        source = load_source(self.root)
        self.receipt["call_ranges"][0][0]["native_call"]["source"] = 2
        self.save_receipt()
        with self.assertRaisesRegex(ValueError, "Fine source ranges"):
            face_sources(self.root, source)
        self.receipt["original_sources"] = []
        self.save_receipt()
        with self.assertRaisesRegex(ValueError, "identities differ"):
            face_sources(self.root, source)

    def test_actual_second_capture_is_checked_beyond_matching_receipts(self):
        path = self.root / "mesh/1/walls.bin"
        data = bytearray(path.read_bytes())
        data[-1] ^= 1
        path.write_bytes(data)
        load_source(self.root)
        self.assertEqual(self.receipt["captures"][0], self.receipt["captures"][1])
        with self.assertRaisesRegex(ValueError, "source stream changed"):
            read_capture(self.root, self.receipt, 1)

    def test_count_mismatch_and_nonfinite_native_values_are_rejected(self):
        self.receipt["captures"][0]["walls"]["count"] += 1
        self.save_receipt()
        with self.assertRaisesRegex(ValueError, "count/byte extent"):
            load_source(self.root)
        self.arrays["walls"][0, 0] = np.array(np.nan, "<f4").view("<u4")
        self.write_streams()
        with self.assertRaisesRegex(ValueError, "invalid source vertices"):
            load_source(self.root)

    def test_versioned_original_fine_input_is_self_contained(self):
        fixture = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-capture-movement/original-fine-cell.zip"
        )
        root = self.root / "original"
        with zipfile.ZipFile(fixture) as archive:
            archive.extractall(root)
        source = load_source(root, allow_mixed_codes=True)
        self.assertEqual(
            (len(source["positions"]), len(source["indices"])), (51970, 26378)
        )
        np.testing.assert_array_equal(
            np.unique(face_sources(root, source)), np.arange(32)
        )
        self.assertEqual(
            source["metadata"]["original_sources"][0],
            {"original_source_ordinal": 1750, "original_source_id": [0, 0]},
        )
        receipt = json.loads((root / "mesh/receipt.json").read_text())
        self.assertEqual(receipt["captures"][0], receipt["captures"][1])
        read_capture(root, receipt, 1)


if __name__ == "__main__":
    unittest.main()
