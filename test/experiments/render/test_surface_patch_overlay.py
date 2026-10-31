"""Independent exact counterexamples to sampling-only/normal-only surface error."""

import unittest
from fractions import Fraction as F

import numpy as np
from surface_patch_overlay import compare


def quad():
    p = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], np.float32)
    f = np.array([[0, 1, 2], [0, 2, 3]], np.uint32)
    return p, p[:, :2].copy(), f


class SurfacePatchOverlay(unittest.TestCase):
    def test_identical_surface_and_affine_fields_with_changed_diagonal(self):
        a = quad()
        b = (*a[:2], np.array([[0, 1, 3], [1, 2, 3]], np.uint32))
        result = compare(a, b, axes=(0, 1))
        self.assertEqual(result["position_error_squared"], 0)
        self.assertEqual(result["field_error_absolute"], (0, 0))
        self.assertEqual(result["positive_area_cells"], 4)

    def test_maximum_between_original_vertices_is_not_missed(self):
        a = quad()
        a[0][3, 2] = 1
        a[1][3, 0] = 2
        b = (*a[:2], np.array([[0, 1, 3], [1, 2, 3]], np.uint32))
        result = compare(a, b, axes=(0, 1))
        self.assertEqual(result["position_error_squared"], F(1, 4))
        # Old centre x varying is 1/2; the new diagonal interpolates (1+2)/2.
        self.assertEqual(result["field_error_absolute"], (F(1), 0))
        self.assertEqual(result["position_witness"][2], (F(1, 2), F(1, 2)))

    def test_subnormal_geometry_error_is_exact(self):
        a = quad()
        a[0][3, 2] = np.nextafter(np.float32(0), np.float32(1))
        b = (*a[:2], np.array([[0, 1, 3], [1, 2, 3]], np.uint32))
        self.assertEqual(
            compare(a, b, axes=(0, 1))["position_error_squared"], F(2) ** -300
        )

    def test_interior_fan_vertex_removal_has_whole_continuous_bound(self):
        p, uv, faces = quad()
        p = np.vstack((p, np.array([[0.5, 0.5, 0.125]], np.float32)))
        uv = np.vstack((uv, np.array([[0.5, 0.5]], np.float32)))
        fan = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]], np.uint32)
        result = compare((p, uv, fan), (p, uv, faces), axes=(0, 1))
        self.assertEqual(result["position_error_squared"], F(1, 64))
        self.assertEqual(result["field_error_absolute"], (0, 0))

    def test_missing_corner_hole_and_duplicate_layer_cannot_pass_by_total_area(self):
        a = quad()
        with self.assertRaisesRegex(ValueError, "Different complete"):
            compare(a, (*a[:2], a[2][:1]), axes=(0, 1))
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            compare(a, (*a[:2], np.tile(a[2][:1], (2, 1))), axes=(0, 1))

    def test_opposite_winding_and_projection_collapse_retain_originals(self):
        a = quad()
        with self.assertRaisesRegex(ValueError, "winding"):
            compare(a, (*a[:2], a[2][:, ::-1]), axes=(0, 1))
        with self.assertRaisesRegex(ValueError, "Degenerate"):
            compare(a, a, axes=(0, 2))

    def test_two_layers_touching_in_chart_are_not_a_continuous_patch(self):
        p, uv, faces = quad()
        p = np.vstack((p, p[faces[1]]))
        p[4:, 2] = 1
        uv = np.vstack((uv, uv[faces[1]]))
        a = p, uv, np.array([[0, 1, 2], [4, 5, 6]], np.uint32)
        with self.assertRaisesRegex(ValueError, "Discontinuous"):
            compare(a, a, axes=(0, 1))

    def test_real_hole_remains_part_of_the_whole_domain(self):
        p = np.array(
            [
                [0, 0, 0],
                [3, 0, 0],
                [3, 3, 0],
                [0, 3, 0],
                [1, 1, 0],
                [2, 1, 0],
                [2, 2, 0],
                [1, 2, 0],
            ],
            np.float32,
        )
        ring = np.array(
            [
                [0, 1, 5],
                [0, 5, 4],
                [1, 2, 6],
                [1, 6, 5],
                [2, 3, 7],
                [2, 7, 6],
                [3, 0, 4],
                [3, 4, 7],
            ],
            np.uint32,
        )
        original = p, np.empty((8, 0), np.float32), ring
        self.assertEqual(
            compare(original, original, axes=(0, 1))["position_error_squared"], 0
        )
        with self.assertRaisesRegex(ValueError, "Different complete"):
            compare(
                original,
                (*original[:2], np.array([[0, 1, 2], [0, 2, 3]], np.uint32)),
                axes=(0, 1),
            )


if __name__ == "__main__":
    unittest.main()
