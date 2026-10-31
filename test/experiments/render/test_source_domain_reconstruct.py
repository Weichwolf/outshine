"""Whole original fallback binding, material selection and failure controls."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_domain_reconstruct_study import original_fallback
from source_original_ranges import ROW, write_ranges


class DomainFallbackTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "ranges"
        self.rows = np.zeros(3, dtype=ROW)
        self.rows["ordinal"] = [0, 1, 2]
        self.rows["indices"] = [[0, 1, 2], [3, 4, 5], [6, 7, 8]]
        self.rows["binding"][:, 1] = [8, 9, 8]
        self.rows["binding"][:, 2] = -1
        self.rows["binding"][:, 4] = [0, 1, 0]
        self.rows["corners"][2, :, 8] = 1
        write_ranges(self.path, self.rows, indexed=True)
        self.metadata = {
            "encoding": "source_indexed",
            "source_binding_sha256": "actual-binding",
            "index_sha256": hashlib.sha256(
                (self.path / "index.npy").read_bytes()
            ).hexdigest(),
        }
        self.report = self.path / "report.json"
        self.report.write_text(json.dumps(self.metadata))

    def test_whole_original_and_native_metres_material_rule(self):
        rows, materials, work, total = original_fallback(
            self.path, "actual-binding", {8}
        )
        self.assertEqual(rows.tobytes(), self.rows[[0, 2]].tobytes())
        self.assertEqual(materials.tolist(), [0, 2])
        self.assertGreater(total, work["read_bytes"])
        rows, materials, _, _ = original_fallback(self.path, "actual-binding", {8, 9})
        self.assertEqual(rows.tobytes(), self.rows.tobytes())
        self.assertEqual(materials.tolist(), [0, 1, 2])

    def test_changed_binding_missing_source_and_corrupt_payload_fail(self):
        with self.assertRaises(ValueError):
            original_fallback(self.path, "different-binding", {8})
        with self.assertRaises(ValueError):
            original_fallback(self.path, "actual-binding", {999})
        payload = self.path / "rows.bin"
        raw = payload.read_bytes()
        payload.write_bytes(bytes([raw[0] ^ 1]) + raw[1:])
        with self.assertRaises(ValueError):
            original_fallback(self.path, "actual-binding", {8})


if __name__ == "__main__":
    unittest.main()
