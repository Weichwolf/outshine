"""Exact zero-loss eligibility, original remainders and native flat-code boundaries."""

import copy
import unittest

import numpy as np
from source_patch_candidates import integer_positions, prepare


def source():
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], np.float32)
    return {
        "positions": positions,
        "indices": np.array([[0, 1, 2], [0, 2, 3]], np.uint32),
        "parts": ({"vertices": 4, "triangles": 2, "material": 0},),
        "attributes": (
            {
                "normals": np.tile(np.array([0, 0, 1], np.float32), (4, 1)),
                "uv": positions[:, :2].copy(),
                "uv1": positions[:, :2].copy(),
                "colours": np.ones((4, 4), np.float32),
            },
        ),
    }


class SourcePatchCandidates(unittest.TestCase):
    def test_connected_complete_planar_quad_retains_all_originals(self):
        s = source()
        before = copy.deepcopy(s)
        p = prepare(s)
        self.assertEqual(len(p["patches"]), 1)
        self.assertEqual(p["patches"][0]["triangles"], (0, 1))
        self.assertEqual(p["fallback"], ())
        self.assertIs(p["original_source"], s)
        np.testing.assert_array_equal(
            s["positions"].view("u4"), before["positions"].view("u4")
        )
        np.testing.assert_array_equal(s["indices"], before["indices"])
        for key in s["attributes"][0]:
            np.testing.assert_array_equal(
                s["attributes"][0][key].view("u4"),
                before["attributes"][0][key].view("u4"),
            )

    def test_subnormal_out_of_plane_position_cannot_be_ignored(self):
        s = source()
        s["positions"][3, 2] = np.nextafter(np.float32(0), np.float32(1))
        p = prepare(s)
        self.assertEqual(len(p["patches"]), 2)
        self.assertEqual(sum(len(x["triangles"]) for x in p["patches"]), 2)

    def test_reversed_original_winding_cannot_merge_through_a_shading_normal(self):
        s = source()
        s["indices"][1] = s["indices"][1, ::-1]
        p = prepare(s)
        self.assertEqual(len(p["patches"]), 2)

    def test_changed_affine_attribute_is_an_original_remainder(self):
        s = source()
        s["attributes"][0]["uv1"][3, 0] = 0.125
        p = prepare(s)
        self.assertEqual(p["patches"], ())
        self.assertEqual(
            p["fallback"],
            ((0, "nonaffine_original_attribute"), (1, "nonaffine_original_attribute")),
        )

    def test_native_flat_facade_code_and_varying_normal_keep_originals(self):
        s = source()
        s["attributes"][0]["uv"][0, 0] = 512
        p = prepare(s)
        self.assertEqual(
            dict(p["work"]["fallback_reasons"]), {"native_flat_facade_code_boundary": 2}
        )
        s = source()
        s["attributes"][0]["normals"][0, 0] = 0.01
        self.assertEqual(
            prepare(s)["work"]["fallback_reasons"],
            {"varying_original_vertex_normal": 2},
        )

    def test_signed_zero_and_degenerate_triangles_are_not_silently_rewritten(self):
        s = source()
        s["attributes"][0]["uv1"][0, 0] = -0.0
        self.assertEqual(
            prepare(s)["work"]["fallback_reasons"], {"mixed_signed_zero": 2}
        )
        s = source()
        s["indices"][0] = [0, 0, 0]
        p = prepare(s)
        self.assertIn((0, "degenerate_original"), p["fallback"])
        self.assertEqual(
            sum(len(x["triangles"]) for x in p["patches"]) + len(p["fallback"]), 2
        )

    def test_position_integer_grid_is_exact_for_float32_values(self):
        from fractions import Fraction as F

        values = np.array(
            [
                [1.0, -2.0, 0.0],
                [0.125, -0.00003, np.nextafter(np.float32(0), np.float32(1))],
            ],
            np.float32,
        )
        points, exponent = integer_positions(values)
        scale = F(2) ** exponent
        for p, row in zip(points, values, strict=True):
            for x, y in zip(p, row, strict=True):
                self.assertEqual(x * scale, F(float(y)))


if __name__ == "__main__":
    unittest.main()
