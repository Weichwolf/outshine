"""Noncontiguous anonymous source membership, raw bytes and malformed cells."""

import struct
import unittest

import numpy as np
from building_plan_inputs import read_plans
from compare_native_cell_views import low_eye_view
from subset_native_cell_input import subset_cell


def fixture():
    # Synthetic schema8 with shared coordinate arrays and repeated anonymous IDs.
    prefix = struct.pack("<II3d", 0x31425350, 8, 1, 2, 3)
    prefix += bytes(8 + 4 + 1 + 8 + 4 + 32 + 8)
    prefix += b"\0" + struct.pack("<Q", 0)
    prefix += bytes(8 + 1 + 4 + 1) + struct.pack("<Q", 0)
    prefix += struct.pack("<Q6d", 6, 0, 0, 0, 1, 1, 0)
    prefix += struct.pack("<Q", 0)  # No holes.
    prefix += struct.pack("<Q3d", 3, 100, 101, 102)
    records = []
    for ordinal, cell in enumerate((1, 2, 1, 3, 1)):
        record = struct.pack("<7I", 0, 3, 0, 0, 0, 0, cell)
        record += bytes(32) + struct.pack("<2di", 10 + ordinal, 0, 0)
        record += b"\1" + struct.pack("<3f", ordinal / 10, 0.5, 0.5)
        record += b"\0" + struct.pack("<QB", 0, 0) + b"\0"
        record += bytes(86)
        record += struct.pack("<4dQ4d", 0, 0, 1, 1, 0, 100, 100, 0.5, 1)
        records.append(record)
    data = prefix + struct.pack("<Q", 5) + b"".join(records)
    data += struct.pack("<Q", 5) + bytes(5)  # Unsupported selection summaries.
    return data, prefix, records


class NativeCellTests(unittest.TestCase):
    def test_all_noncontiguous_anonymous_sources_and_raw_arrays_are_retained(self):
        original, prefix, records = fixture()
        result, members = subset_cell(original, 1)
        expected = (
            prefix + struct.pack("<Q", 3) + b"".join(records[i] for i in (0, 2, 4))
        )
        expected += struct.pack("<Q", 3) + bytes(3)
        self.assertEqual(result, expected)
        self.assertEqual([m["original_source_ordinal"] for m in members], [0, 2, 4])
        self.assertEqual([m["original_source_id"] for m in members], [(0, 0)] * 3)
        before, after = read_plans(original), read_plans(result)
        for key in ("points_latlon", "holes", "corner_asl_m"):
            np.testing.assert_array_equal(before[key], after[key])
        self.assertEqual([r["height_m"] for r in after["records"]], [10, 12, 14])

    def test_empty_out_of_range_truncated_schema_and_trailing_inputs_reject(self):
        original, _, _ = fixture()
        for cell in (0, 64, 65):
            with self.assertRaises(ValueError):
                subset_cell(original, cell)
        for data in (
            original[:-1],
            original + b"x",
            original[:4] + struct.pack("<I", 9) + original[8:],
        ):
            with self.assertRaises(ValueError):
                subset_cell(data, 1)

    def test_low_eye_height_and_translation_do_not_change_camera_orientation(self):
        positions = np.array([[0, 0, 100], [20, 30, 115]], np.float32)
        up, east, north = np.eye(3)[[2, 0, 1]]
        a = low_eye_view(positions, up, east, north, 1000, 8, 44, 0)
        b = low_eye_view(positions, up, east, north, 1000, 8, 44, 32)
        self.assertAlmostEqual(a[0] @ up, 101.7)
        np.testing.assert_allclose(b[0] - a[0], 32 * a[1][0])
        np.testing.assert_array_equal(a[1], b[1])
        np.testing.assert_allclose(a[1] @ a[1].T, np.eye(3), atol=1e-14)


if __name__ == "__main__":
    unittest.main()
