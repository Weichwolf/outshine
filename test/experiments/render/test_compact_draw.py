"""Canonical lineage, arbitrary corner seams, unknown normals and exact-index limits."""

import unittest

import numpy as np
from compact_draw import FACE_ARRAYS, canonical_faces, compact, verify


def source():
    return {
        "positions": np.array(
            [[0, 0, 0], [1, 0, 0], [2, 0, 0], [0, 1, 0], [0, 0, 0]], np.float32
        ),
        "indices": np.array([[0, 1, 0], [0, 1, 2], [0, 1, 3], [0, 1, 4]], np.uint16),
        "corner_uv": np.arange(24, dtype=np.float32).reshape(4, 3, 2),
        "corner_metres": np.zeros((4, 3, 2), np.float32),
        "corner_shading_words": np.arange(12, dtype=np.uint32).reshape(4, 3, 1),
        "corner_colour": np.arange(48, dtype=np.float32).reshape(4, 3, 4),
        "face_shading_valid": np.array([False, True, True, False]),
        "face_native_stream": np.array([-1, 0, 0, -1], np.int8),
        "face_normal_words": np.array([0, 1, 2, 3], np.uint32),
        "face_patches": np.arange(4, dtype=np.uint16),
        "corner_swap": np.array([True, False, True, False]),
        "patch_sources": np.array([41, 42, 43, 44], np.uint32),
    }


class CompactDrawTest(unittest.TestCase):
    def test_first_hit_lineage_preserves_original_rest_and_sky(self):
        result = canonical_faces(np.array([0, 3, -1]), np.array([1, 2, 3]), 4, 1)
        np.testing.assert_array_equal(result, [1, 4, -1])

    def test_only_repeated_indices_disappear_and_all_original_corner_words_remain(self):
        original = source()
        original["corner_metres"][2, 0, 0] = -0.0
        before = {key: value.tobytes() for key, value in original.items()}
        draw, report = compact(original)
        verify(original, draw)
        np.testing.assert_array_equal(draw["draw_to_reference"], [1, 2, 3])
        self.assertEqual(report["removed_repeated_index_triangles"], 1)
        self.assertEqual(int(draw["reference_triangle_count"]), 4)
        self.assertFalse(draw["face_shading_valid"][-1])
        self.assertEqual(
            before, {key: value.tobytes() for key, value in original.items()}
        )
        self.assertTrue(np.signbit(draw["corner_metres"][1, 0, 0]))
        self.assertIs(draw["positions"], original["positions"])
        for key in (
            "corner_uv",
            "corner_shading_words",
            "corner_colour",
            "corner_swap",
        ):
            self.assertEqual(draw[key].tobytes(), original[key][[1, 2, 3]].tobytes())

    def test_draw_is_self_contained_after_canonical_arrays_go_away(self):
        original = source()
        draw, _ = compact(original)
        expected = original["positions"][original["indices"][[1, 2, 3]]].copy()
        del original
        np.testing.assert_array_equal(draw["positions"][draw["indices"]], expected)
        self.assertTrue(draw["corner_uv"].flags.c_contiguous)

    def test_all_removed_is_a_valid_empty_draw_without_vertex_merging(self):
        original = source()
        original = {
            k: v[:1] if k not in ("positions", "patch_sources") else v
            for k, v in original.items()
        }
        draw, report = compact(original)
        verify(original, draw)
        self.assertEqual(draw["indices"].shape, (0, 3))
        self.assertEqual(draw["corner_uv"].shape, (0, 3, 2))
        self.assertEqual(report["draw_triangles"], 0)
        self.assertEqual(len(draw["positions"]), 5)

    def test_reference_map_width_depends_on_canonical_ids_not_live_count(self):
        original = {
            "positions": source()["positions"],
            "indices": np.zeros((65537, 3), np.uint16),
        }
        original["indices"][-1] = [0, 1, 3]
        draw, _ = compact(original)
        self.assertEqual(draw["draw_to_reference"].dtype, np.uint32)
        np.testing.assert_array_equal(draw["draw_to_reference"], [65536])
        verify(original, draw)

    def test_undeclared_fields_and_repeated_compaction_need_an_explicit_contract(self):
        original = source()
        with self.assertRaises(ValueError):
            compact(original | {"future_face_data": np.ones(4)})
        draw, _ = compact(original)
        with self.assertRaises(ValueError):
            compact(draw)

    def test_missing_live_row_or_changed_word_is_rejected_by_lineage_check(self):
        original = source()
        draw, _ = compact(original)
        draw["corner_uv"][0, 0, 0] += 1
        with self.assertRaisesRegex(ValueError, "field words"):
            verify(original, draw)

    def test_deleting_a_distinct_index_collinear_face_fails_even_with_valid_field_rows(
        self,
    ):
        original = source()
        draw, _ = compact(original)
        reduced = {
            key: value[1:]
            if key in FACE_ARRAYS or key == "draw_to_reference"
            else value
            for key, value in draw.items()
        }
        with self.assertRaisesRegex(ValueError, "exactly the repeated-index"):
            verify(original, reduced)


if __name__ == "__main__":
    unittest.main()
