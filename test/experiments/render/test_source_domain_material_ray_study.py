"""Flat shader inputs cannot be inferred from an interpolated UV alone."""

import unittest

import numpy as np
from source_domain_material_ray_study import compare, reference_codes


class FlatMaterialReference(unittest.TestCase):
    def test_vertex_disagreement_is_unknown_and_constant_code_is_exact(self):
        source = {
            "uv": np.array(
                [[255, 0], [256, 0], [0, 0], [26112, 0], [26118, 0], [26112, 0]],
                dtype="<f4",
            ),
            "indices": np.array([[0, 1, 2], [3, 4, 5]]),
        }
        codes, eligible = reference_codes(source, np.array([0, 1, -1]))
        np.testing.assert_array_equal(eligible, [False, True, False])
        self.assertEqual(codes[1], 102)

    def test_ambiguous_triangle_cannot_count_as_match_or_mismatch(self):
        faces = np.array([0, 1])
        fields = {"uv": np.zeros((2, 2)), "metres": np.zeros((2, 2))}
        direct = {
            "uv": np.array([[0, 0], [-562, 0]], np.float32),
            "metres": np.zeros((2, 2)),
            "qualified": np.array([True, True]),
        }
        result = compare(
            faces, fields, direct, np.array([1, 102]), np.array([False, True])
        )
        self.assertEqual(result["flat_program_eligible_rays"], 1)
        self.assertEqual(result["flat_program_ambiguous_rays"], 1)
        self.assertEqual(result["flat_program_mismatches"], 1)


if __name__ == "__main__":
    unittest.main()
