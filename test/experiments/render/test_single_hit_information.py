"""First-hit information counterexamples and finite native projection checks."""

import unittest

import numpy as np
from compare_single_hit_information import (
    indistinguishable_pair,
    missing_samples,
    rectangle_samples,
)


class InformationTests(unittest.TestCase):
    def test_hidden_rear_changes_no_capture_but_all_four_novel_samples(self):
        proof = indistinguishable_pair()
        self.assertTrue(proof["identical_capture_rgba_and_depth"])
        self.assertEqual(proof["source_coverage_gap"], 1)
        self.assertEqual(proof["worst_case_coverage_error_lower_bound"], 0.5)
        self.assertEqual(proof["worst_case_linear_rgb_error_lower_bound"], 0.5)
        # Also inspect off-grid continuous capture positions and edge/outside.
        uv = np.array([[0.13, 0.24], [-0.5, 0.5], [0.501, 0], [-0.4, -0.49]])
        for a, b in zip(
            rectangle_samples(uv, 0, False), rectangle_samples(uv, 0, True), strict=True
        ):
            np.testing.assert_array_equal(a, b)

    def test_opaque_background_union_alpha_does_not_identify_source_coverage(self):
        uv = np.array([[[-1.1, -0.1], [-0.9, -0.1]], [[-1.1, 0.1], [-0.9, 0.1]]])
        a, b = (rectangle_samples(uv, 1, rear)[0] for rear in (False, True))
        background = np.full((2, 2, 4), 0.1)
        background[:, :, 3] = 1
        first, second = (v + (1 - v[:, :, 3:]) * background for v in (a, b))
        np.testing.assert_array_equal(first[:, :, 3], second[:, :, 3])
        self.assertEqual(abs(first[:, :, 0] - second[:, :, 0]).max(), 0.9)
        self.assertEqual(abs(a[:, :, 3] - b[:, :, 3]).max(), 1)

    def test_identical_depth_planes_map_to_their_own_cells(self):
        z = np.full((2, 2), 0.5, np.float32)
        absent, stable, result = missing_samples(z, z, np.eye(4), [0, 0, 2, 2], [2, 2])
        self.assertFalse(absent.any())
        self.assertFalse(stable.any())
        self.assertEqual(result["native_source_samples"], 4)
        self.assertEqual(result["projection_evaluations"], 36)

    def test_absence_and_outside_capture_are_distinct(self):
        z = np.full((2, 2), 0.5, np.float32)
        empty = np.zeros_like(z)
        absent, stable, result = missing_samples(
            z, empty, np.eye(4), [0, 0, 2, 2], [2, 2]
        )
        self.assertTrue(absent.all())
        self.assertTrue(stable.all())
        self.assertEqual(result["same_empty_cell_under_declared_probe"], 4)
        shift = np.eye(4)
        shift[0, 3] = 4
        absent, _, result = missing_samples(z, empty, shift, [0, 0, 2, 2], [2, 2])
        self.assertFalse(absent.any())
        self.assertEqual(result["projected_samples_outside_capture"], 4)

    def test_wrong_primitive_or_invalid_projective_domain_is_rejected(self):
        z = np.full((2, 2), 0.5, np.float32)
        for bad in (z.astype(float), np.full_like(z, np.nan), np.full_like(z, 2)):
            with self.assertRaises(ValueError):
                missing_samples(bad, z, np.eye(4), [0, 0, 2, 2], [2, 2])
        behind = np.eye(4)
        behind[3, 3] = -1
        with self.assertRaises(ValueError):
            missing_samples(z, z, behind, [0, 0, 2, 2], [2, 2])


if __name__ == "__main__":
    unittest.main()
