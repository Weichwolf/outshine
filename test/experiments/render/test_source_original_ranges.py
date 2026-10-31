import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_original_ranges import (
    decode_indexed,
    encode_indexed,
    original_rows,
    read_ranges,
    write_ranges,
)


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
        for ids in ([99], [8, 8], [2**70]):
            with self.subTest(ids=ids), self.assertRaises(ValueError):
                read_ranges(self.path, self.index, ids)
        for bad_index in (self.index[::-1], self.index[[0, 0]]):
            with self.assertRaisesRegex(ValueError, "Sorted unique"):
                read_ranges(self.path, bad_index, [8])
        with (self.path / "rows.bin").open("r+b") as stream:
            stream.seek(int(self.index[0]["offset"]) + 1)
            value = stream.read(1)
            stream.seek(-1, 1)
            stream.write(bytes([value[0] ^ 1]))
        with self.assertRaisesRegex(ValueError, "changed"):
            read_ranges(self.path, self.index, [8])

    def test_indexed_ranges_preserve_original_words(self):
        path = Path(self.temp.name) / "indexed"
        index = write_ranges(path, self.rows, indexed=True)
        for ids, expected in (
            ([8], self.rows[[0, 2]]),
            ([9, 8], self.rows),
            ([], self.rows[:0]),
        ):
            rows, _ = read_ranges(path, index, ids, indexed=True)
            np.testing.assert_array_equal(rows.view("u1"), expected.view("u1"))
            chunks, _ = read_ranges(path, index, ids, indexed=True, compact=True)
            self.assertEqual(sum(len(tri) for tri, _ in chunks), len(expected))
        with self.assertRaises(ValueError):
            read_ranges(self.path, self.index, [8], compact=True)
        copied, _ = read_ranges(path, index, [8, 9], indexed=True, compact=True)
        self.assertIsNot(copied[0][0].base, copied[1][0].base)
        chunks, _ = read_ranges(
            path, index, [8, 9], indexed=True, compact=True, share_contiguous=True
        )
        first_base, second_base = (tri.base for tri, _ in chunks)
        self.assertIs(first_base, second_base)
        self.assertFalse(chunks[0][0].flags.writeable)
        survivor = chunks[0][0]
        del chunks, first_base, second_base
        self.assertEqual(
            len(survivor.base), int(index[0]["bytes"]) + int(index[1]["bytes"])
        )
        with self.assertRaises(ValueError):
            read_ranges(path, index, [8], indexed=True, share_contiguous=True)
        encoded = encode_indexed(self.rows[[0, 2]])
        embedded = b"prefix" + encoded + b"tail"
        restored = decode_indexed(embedded, offset=6, length=len(encoded))
        np.testing.assert_array_equal(restored.view("u1"), self.rows[[0, 2]].view("u1"))
        for offset, length in (
            (-1, len(encoded)),
            (6, len(encoded) - 1),
            (6, len(encoded) + 5),
        ):
            with self.assertRaises(ValueError):
                decode_indexed(embedded, offset=offset, length=length)
        with self.assertRaises(ValueError):
            decode_indexed(encoded[:-1])
        bad = self.rows[[0, 2]].copy()
        bad["corners"][1, 0, 0] += 1
        with self.assertRaisesRegex(ValueError, "identical"):
            encode_indexed(bad)

    def test_coalescing_reads_gap_without_returning_its_source(self):
        rows = np.concatenate((self.rows, self.rows[:1].copy()))
        rows["ordinal"][-1] = 3
        rows["binding"][-1, 1] = 10
        for indexed in (False, True):
            path = Path(self.temp.name) / f"gaps-{indexed}"
            index = write_ranges(path, rows, indexed=indexed)
            exact, exact_cost = read_ranges(path, index, [8, 10], indexed=indexed)
            merged, merged_cost = read_ranges(
                path, index, [8, 10], indexed=indexed, max_gap_bytes=4096
            )
            np.testing.assert_array_equal(exact.view("u1"), merged.view("u1"))
            self.assertEqual(
                (exact_cost["read_calls"], merged_cost["read_calls"]), (2, 1)
            )
            self.assertEqual(
                merged_cost["read_bytes"] - exact_cost["read_bytes"],
                int(index[1]["bytes"]),
            )
            if indexed:
                chunks, compact_cost = read_ranges(
                    path,
                    index,
                    [8, 10],
                    indexed=True,
                    compact=True,
                    max_gap_bytes=4096,
                    share_contiguous=True,
                )
                self.assertEqual(compact_cost, merged_cost)
                actual_ordinals = np.sort(
                    np.concatenate([tri["ordinal"] for tri, _ in chunks])
                )
                np.testing.assert_array_equal(actual_ordinals, merged["ordinal"])
                first_base, second_base = (tri.base for tri, _ in chunks)
                self.assertIsNot(first_base, second_base)
                self.assertEqual(len(first_base), int(index[0]["bytes"]))
                self.assertEqual(len(second_base), int(index[2]["bytes"]))
            with self.assertRaises(ValueError):
                read_ranges(path, index, [8], indexed=indexed, max_gap_bytes=-1)


if __name__ == "__main__":
    unittest.main()
