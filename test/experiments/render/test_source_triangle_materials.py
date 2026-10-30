"""Original vertex processing must survive demand-selected triangle indices."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_triangle_materials import triangle_parts, write_source


class SourceTriangles(unittest.TestCase):
    def setUp(self):
        self.source = {
            "positions": np.arange(24, dtype=np.float32).reshape(8, 3),
            "indices": np.array([[0, 2, 1], [1, 2, 3], [4, 6, 5], [5, 6, 7]]),
            "parts": [
                {"vertices": 4, "triangles": 2, "material": 7},
                {"vertices": 4, "triangles": 2, "material": 3},
            ],
            "attributes": [
                {"uv": np.arange(8, dtype=np.float32).reshape(4, 2) + at}
                for at in (0, 100)
            ],
        }

    def test_part_offsets_vertex_order_and_materials_survive(self):
        parts = triangle_parts(self.source, [3, 0, 3])
        self.assertEqual([p[0]["material"] for p in parts], [7, 3])
        np.testing.assert_array_equal(parts[0][1]["indices"], [[0, 2, 1]])
        np.testing.assert_array_equal(parts[1][1]["indices"], [[1, 2, 3]])
        for at, (_, values) in enumerate(parts):
            self.assertIs(values["uv"], self.source["attributes"][at]["uv"])
            self.assertTrue(
                np.shares_memory(values["positions"], self.source["positions"])
            )
            self.assertEqual(len(values["positions"]), 4)

    def test_unused_part_keeps_original_material_binding(self):
        parts = triangle_parts(self.source, [2])
        self.assertEqual(parts[0][0], {"vertices": 4, "triangles": 0, "material": 7})
        self.assertEqual(parts[0][1]["indices"].shape, (0, 3))
        self.assertEqual(parts[1][0]["material"], 3)

    def test_unknown_or_invalid_ids_are_not_silently_removed(self):
        for ids in ([], [-1], [4], [0.0], [True], [[0]], [2**64 - 1]):
            with self.subTest(ids=ids), self.assertRaises(ValueError):
                triangle_parts(self.source, ids)

    def test_cross_part_index_is_rejected(self):
        self.source["indices"][2, 0] = 3
        with self.assertRaises(ValueError):
            triangle_parts(self.source, [2])

    def test_incomplete_part_inventory_is_rejected(self):
        self.source["parts"][1]["triangles"] = 1
        with self.assertRaises(ValueError):
            triangle_parts(self.source, [0])

    def test_native_files_keep_raw_attributes_and_local_indices(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(write_source(self.source, [0, 3], root), 2)
            geometry = root / "geometry" / "0"
            metadata = json.loads((geometry / "input.json").read_text())
            self.assertEqual([p["material"] for p in metadata["parts"]], [7, 3])
            for at, expected in enumerate(([0, 2, 1], [1, 2, 3])):
                np.testing.assert_array_equal(
                    np.fromfile(geometry / str(at) / "indices.bin", "<u4"), expected
                )
                self.assertEqual(
                    (geometry / str(at) / "uv.bin").read_bytes(),
                    self.source["attributes"][at]["uv"].tobytes(),
                )


if __name__ == "__main__":
    unittest.main()
