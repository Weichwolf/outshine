"""Distinct collinearity, attribute seams and retained-reference memory countercases."""

import unittest

import numpy as np
from duplicate_draw_work import audit


class DuplicateDrawAuditTest(unittest.TestCase):
    def source(self):
        return {
            "positions": np.array(
                [[0, 0, 0], [1, 0, 0], [2, 0, 0], [0, 1, 0], [0, 0, 0]], np.float32
            ),
            "indices": np.array(
                [[0, 1, 0], [0, 1, 2], [0, 1, 3], [0, 1, 4]], np.uint16
            ),
            "corner_uv": np.array([[[0, 0], [1, 0], [2, 0]]] * 4, np.float32),
            "face_shading_valid": np.array([False, True, True, False]),
            "corner_swap": np.array([True, False, False, False]),
            "opaque_future_metadata": np.ones(4, np.uint32),
        }

    def test_only_identical_indices_are_candidates_even_with_equal_other_positions(
        self,
    ):
        a = self.source()
        result = audit(a)
        self.assertEqual(result["repeated_position_index_triangles"], 1)
        self.assertEqual(result["distinct_index_zero_cross_triangles_retained"], 2)
        self.assertEqual(result["hypothetical_draw_triangles"], 3)
        self.assertEqual(
            result["duplicate_positions_with_different_corner_fields"]["corner_uv"], 1
        )
        self.assertEqual(result["hypothetical_live_unqualified_shading_triangles"], 1)

    def test_costs_include_swap_map_and_unknown_metadata_and_never_assume_shared_bytes_twice(
        self,
    ):
        a = self.source()
        result = audit(a)
        # Face rows: index6 + UV24 + validity1 + corner-swap1 =32B per triangle.
        self.assertEqual(result["reference_face_array_bytes"], 128)
        self.assertEqual(result["unchanged_array_bytes"], 60 + 16)
        self.assertEqual(result["hypothetical_live_product_bytes"], 76 + 96 + 6)
        self.assertEqual(result["hypothetical_saved_array_bytes_including_map"], 32 - 6)
        self.assertEqual(
            result["both_resident_bytes_sharing_unchanged_arrays"], 76 + 128 + 96 + 6
        )

    def test_audit_preserves_all_input_words(self):
        a = self.source()
        before = {key: value.tobytes() for key, value in a.items()}
        audit(a)
        self.assertEqual(before, {key: value.tobytes() for key, value in a.items()})

    def test_attribute_seams_count_word_differences_including_signed_zero(self):
        a = self.source()
        a["corner_uv"][0] = 0
        a["corner_uv"][0, 2, 0] = -0.0
        result = audit(a)
        self.assertEqual(
            result["duplicate_positions_with_different_corner_fields"]["corner_uv"], 1
        )

    def test_inconsistent_fields_and_invalid_indices_reject(self):
        a = self.source()
        with self.assertRaises(ValueError):
            audit(a | {"corner_swap": np.zeros(3, bool)})
        with self.assertRaises(ValueError):
            audit(a | {"indices": np.array([[0, 1, 99]], np.uint16)})


if __name__ == "__main__":
    unittest.main()
