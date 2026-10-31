import unittest

import numpy as np
from source_original_range_parts import part_inputs
from source_original_ranges import original_rows
from source_triangle_materials import triangle_parts


class OriginalPartInputsTest(unittest.TestCase):
    def setUp(self):
        self.source = {
            "positions": np.arange(27, dtype="<f4").reshape(9, 3),
            "indices": np.array([[0, 1, 2], [2, 3, 0], [4, 5, 6], [6, 7, 8]], "<u4"),
            "parts": [
                {"material": 0, "triangles": 2, "vertices": 4},
                {"material": 1, "triangles": 2, "vertices": 5},
            ],
            "attributes": [
                {
                    "normals": np.ones((count, 3), "<f4"),
                    "uv": np.zeros((count, 2), "<f4"),
                    "uv1": np.full((count, 2), -0.0, "<f4"),
                    "colours": np.ones((count, 4), "<f4"),
                }
                for count in (4, 5)
            ],
        }
        binding = np.arange(24, dtype="<i4").reshape(4, 6)
        binding[:, 1] = [1, 2, 1, 2]
        self.rows = original_rows(self.source, binding)

    def test_parts_equal_existing_adapter(self):
        for selected in ([0, 2, 3], [0, 1, 2, 3], []):
            with self.subTest(selected=selected):
                expected = triangle_parts(
                    self.source, np.array(selected, "<u4"), referenced_vertices=True
                )
                actual = part_inputs(self.rows[selected], self.source["parts"])
                for (metadata, arrays), (
                    actual_metadata,
                    actual_arrays,
                    binding,
                ) in zip(expected, actual, strict=True):
                    self.assertEqual(metadata, actual_metadata)
                    self.assertEqual(len(binding), metadata["triangles"])
                    for name, array in arrays.items():
                        self.assertEqual(array.tobytes(), actual_arrays[name].tobytes())

    def test_rejects_missing_order_and_inconsistent_vertex(self):
        with self.assertRaisesRegex(ValueError, "order"):
            part_inputs(self.rows[[1, 0]], self.source["parts"])
        corrupt = self.rows[:2].copy()
        corrupt["corners"][1, 0, 0] += 1
        with self.assertRaisesRegex(ValueError, "disagree"):
            part_inputs(corrupt, self.source["parts"])
        corrupt = self.rows[:1].copy()
        corrupt["indices"][0, 0] = 5
        with self.assertRaisesRegex(ValueError, "crosses"):
            part_inputs(corrupt, self.source["parts"])


if __name__ == "__main__":
    unittest.main()
