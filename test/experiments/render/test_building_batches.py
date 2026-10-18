"""Independent native stream/index/DAG preservation and bounded packing controls."""

import unittest

import numpy as np
from building_resident_batches import pack, pages, verify
from cached_building_streams import STREAM_BYTES, validate_streams


def triangle(cell=1, optional=False, copies=1):
    vertices = np.array(
        [[0, 0, 0, 0, 0, 17], [1, 0, 0, 1, 0, 23], [0, 1, 0, 0, 1, 31]], dtype="<f4"
    )
    streams = [b""] * 10
    streams[0] = np.tile(vertices, (copies, 1)).tobytes()
    streams[2] = streams[7] = np.arange(3 * copies, dtype="<u4").tobytes()
    records = np.zeros((copies, 13), dtype="<u4")
    records[:, 10] = np.arange(copies) * 3
    records[:, 11] = 3
    streams[4] = records.tobytes()
    if optional:
        streams[5] = np.ones(12 * copies, dtype="<f4").tobytes()
        streams[6] = np.arange(6 * copies, dtype="<f4").tobytes()
    counts = tuple(len(s) // w for s, w in zip(streams, STREAM_BYTES, strict=True))
    return {
        "key": str(cell),
        "cell": cell,
        "parent": "same-frame",
        "view": None,
        "detail": None,
        "streams": streams,
        "counts": counts,
    }


class BuildingBatches(unittest.TestCase):
    def test_independent_expected_rebased_indices_and_cluster_offsets(self):
        originals = [triangle(1), triangle(2)]
        output, mapping = pack(originals)
        self.assertEqual(
            np.frombuffer(output[7], dtype="<u4").tolist(), [0, 1, 2, 3, 4, 5]
        )
        self.assertEqual(
            np.frombuffer(output[4], dtype="<u4").reshape(-1, 13)[:, 10].tolist(),
            [0, 3],
        )
        self.assertEqual(
            output[0], originals[0]["streams"][0] + originals[1]["streams"][0]
        )
        verify(output, mapping, originals)

    def test_roundtrip_keeps_all_optional_attributes_and_dag_levels(self):
        originals = [triangle(1, True, 2), triangle(2, True)]
        mixed_levels = bytearray(originals[0]["streams"][4])
        np.frombuffer(mixed_levels, dtype="<u4").reshape(-1, 13)[1, 12] = 2
        originals[0]["streams"][4] = mixed_levels
        output, mapping = pack(originals)
        verify(output, mapping, originals)
        self.assertEqual(
            output[6], originals[0]["streams"][6] + originals[1]["streams"][6]
        )

    def test_source_labels_and_duplicate_storage_ranges_cannot_alias(self):
        originals = [triangle(1), triangle(2)]
        output, mapping = pack(originals)
        mapping[1]["offsets"] = mapping[0]["offsets"]
        with self.assertRaises(ValueError):
            verify(output, mapping, originals)
        output, mapping = pack(originals)
        mapping[1]["cell"] = 1
        with self.assertRaises(ValueError):
            verify(output, mapping, originals)

    def test_corrupted_attribute_index_and_cluster_offset_are_detected(self):
        originals = [triangle(1), triangle(2)]
        for stream, byte in ((0, 16), (7, 0), (4, 40)):
            output, mapping = pack(originals)
            output[stream][byte] ^= 1
            with self.assertRaises(ValueError):
                verify(output, mapping, originals)

    def test_no_default_attributes_are_invented(self):
        groups = pages([triangle(1), triangle(2, True)], 4096)
        self.assertEqual(len(groups), 2)

    def test_capacity_partitions_before_allocating(self):
        groups = pages([triangle(i) for i in range(1, 65)], 4096)
        self.assertGreater(len(groups), 1)
        for group in groups:
            self.assertLessEqual(
                sum(sum(len(s) for s in p["streams"]) for p in group), 4096
            )
        with self.assertRaises(ValueError):
            pages([triangle(copies=100)], 4096)

    def test_reject_mixed_parent_view_and_duplicate_cells(self):
        for second in (
            triangle(1),
            triangle(2) | {"parent": "other"},
            triangle(2) | {"view": {"input_bytes": "different"}},
        ):
            with self.assertRaises(ValueError):
                pages([triangle(1), second], 4096)

    def test_empty_mesh_and_missing_source_are_not_confused(self):
        empty = triangle() | {"streams": [b""] * 10, "counts": (0,) * 10}
        output, mapping = pack([empty])
        verify(output, mapping, [empty])
        with self.assertRaises(ValueError):
            verify(output, mapping, [empty, triangle(2)])

    def test_invalid_triangle_or_cluster_range_is_rejected(self):
        for stream in (7, 4):
            item = triangle()
            streams = [bytearray(s) for s in item["streams"]]
            streams[stream][0 if stream == 7 else 40] = 255
            with self.assertRaises(ValueError):
                validate_streams(streams, item["counts"])

    def test_independent_slice_and_view_copies_keep_same_complete_bytes(self):
        originals = [triangle(1, True, 2), triangle(2, True)]
        view, view_map = pack(originals, "view")
        sliced, slice_map = pack(originals, "slice")
        self.assertEqual(view, sliced)
        self.assertEqual(view_map, slice_map)
        with self.assertRaises(ValueError):
            pack(originals, "unknown")


if __name__ == "__main__":
    unittest.main()
