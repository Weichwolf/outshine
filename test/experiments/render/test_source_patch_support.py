"""Whole-source support, hidden material layers and continuous interior error."""

import unittest
from fractions import Fraction as F

import numpy as np
from source_patch_support import compare_source


def quad():
    p = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], np.float32)
    rows = np.c_[
        p, np.tile([0, 0, 1], (4, 1)), p[:, :2], p[:, :2], np.ones((4, 4))
    ].astype("f4")
    return rows, np.array([[0, 1, 2], [0, 2, 3]]), np.array([[0, 1, 3], [1, 2, 3]])


class SourcePatchSupport(unittest.TestCase):
    def test_complete_changed_diagonal_is_exact_in_geometry_and_affine_fields(self):
        rows, old, new = quad()
        p = compare_source(
            rows[old], rows[new], np.zeros(2, np.int8), np.zeros(2, np.int8)
        )
        self.assertTrue(p["whole_source_proven"])
        self.assertEqual(p["position_error_squared"], 0)
        self.assertEqual(p["field_error_absolute"], (0,) * 11)

    def test_interior_nonplanarity_is_measured_instead_of_using_shading_normal(self):
        rows, old, new = quad()
        rows[3, 2] = 0.125
        p = compare_source(
            rows[old], rows[new], np.zeros(2, np.int8), np.zeros(2, np.int8)
        )
        self.assertTrue(p["whole_source_proven"])
        self.assertEqual(p["position_error_squared"], F(1, 256))

    def test_missing_floor_or_changed_material_cannot_be_hidden_by_front_coverage(self):
        rows, old, new = quad()
        floor = rows.copy()
        floor[:, 2] = -1
        floor[:, 3:6] = [0, 0, -1]
        floor[:, 6] = -9
        original = np.concatenate((rows[old], floor[old[:, ::-1]]))
        p = compare_source(
            original, rows[new], np.zeros(4, np.int8), np.zeros(2, np.int8)
        )
        self.assertFalse(p["whole_source_proven"])
        self.assertIn("Unmatched", p["reason"])
        p = compare_source(
            rows[old], rows[new], np.zeros(2, np.int8), np.ones(2, np.int8)
        )
        self.assertFalse(p["whole_source_proven"])

    def test_identical_originals_keep_seams_and_layers_without_reinterpreting_them(
        self,
    ):
        rows, old, _ = quad()
        corners = rows[old].copy()
        corners[1, :, 3] += 0.1
        p = compare_source(
            corners, corners[::-1], np.zeros(2, np.int8), np.zeros(2, np.int8)
        )
        self.assertTrue(p["whole_source_proven"])
        self.assertEqual(p["mode"], "same_primitive_word_multiset")

    def test_limits_and_flat_code_changes_retain_original(self):
        rows, old, new = quad()
        candidate = rows[new].copy()
        candidate[0, 0, 6] = 512
        p = compare_source(
            rows[old], candidate, np.zeros(2, np.int8), np.zeros(2, np.int8)
        )
        self.assertFalse(p["whole_source_proven"])
        self.assertIn("flat facade-code", p["reason"])
        p = compare_source(
            np.tile(rows[old], (129, 1, 1)),
            rows[new],
            np.zeros(258, np.int8),
            np.zeros(2, np.int8),
        )
        self.assertFalse(p["whole_source_proven"])


if __name__ == "__main__":
    unittest.main()
