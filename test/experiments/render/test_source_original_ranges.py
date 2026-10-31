import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_original_ranges import original_rows, read_ranges, write_ranges


class OriginalRangesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "ranges"
        positions = np.arange(18, dtype="<f4").reshape(6, 3)
        attributes = {
            "normals": np.full((6, 3), 0.5, "<f4"),
            "uv": np.arange(12, dtype="<f4").reshape(6, 2),
            "uv1": np.zeros((6, 2), "<f4"),
            "colours": np.ones((6, 4), "<f4"),
        }
        source = {
            "positions": positions,
            "indices": np.array([[0, 1, 2], [3, 4, 5], [2, 3, 4]], dtype="<u4"),
            "attributes": [attributes],
        }
        binding = np.zeros((3, 6), dtype="<i4")
        binding[:, 1] = [8, 9, 8]
        binding[:, 2] = -1
        self.rows = original_rows(source, binding)
        self.index = write_ranges(self.path, self.rows)

    def test_whole_source_and_order(self):
        rows, cost = read_ranges(self.path, self.index, [8])
        np.testing.assert_array_equal(rows, self.rows[[0, 2]])
        self.assertEqual(cost["read_calls"], 1)
        all_rows, cost = read_ranges(self.path, self.index, [9, 8])
        np.testing.assert_array_equal(all_rows, self.rows)
        self.assertEqual(cost["read_calls"], 1)
        empty, cost = read_ranges(self.path, self.index, [])
        self.assertEqual((len(empty), cost["read_calls"]), (0, 0))

    def test_missing_duplicate_and_corrupt_source_rejected(self):
        for ids in ([99], [8, 8]):
            with self.subTest(ids=ids), self.assertRaises(ValueError):
                read_ranges(self.path, self.index, ids)
        with (self.path / "rows.bin").open("r+b") as stream:
            stream.seek(int(self.index[0]["offset"]) + 1)
            value = stream.read(1)
            stream.seek(-1, 1)
            stream.write(bytes([value[0] ^ 1]))
        with self.assertRaisesRegex(ValueError, "changed"):
            read_ranges(self.path, self.index, [8])


if __name__ == "__main__":
    unittest.main()
