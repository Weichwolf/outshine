"""Original bindings and conservative candidate discovery for interior fans."""

import unittest
from fractions import Fraction as F

import numpy as np
from source_patch_fans import discover, retriangulate
from surface_patch_overlay import compare
from test_source_patch_candidates import source


def fan_source():
    s = source()
    s["positions"] = np.vstack((s["positions"], [[0.5, 0.5, 0.125]])).astype("f4")
    data = s["attributes"][0]
    for key, row in (
        ("normals", [0, 0, 1]),
        ("uv", [0.5, 0.5]),
        ("uv1", [0.5, 0.5]),
        ("colours", [1, 1, 1, 1]),
    ):
        data[key] = np.vstack((data[key], [row])).astype("f4")
    s["indices"] = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]], np.uint32)
    s["parts"] = ({"vertices": 5, "triangles": 4, "material": 0},)
    return s


class SourcePatchFans(unittest.TestCase):
    def test_boundary_and_original_ordinals_survive_interior_removal(self):
        s = fan_source()
        before = s["positions"].tobytes(), s["indices"].tobytes()
        fans, fields, normals, ids = discover(s)
        self.assertEqual(len(fans), 1)
        fan = fans[0]
        self.assertEqual(fan["triangle_ordinals"], (0, 1, 2, 3))
        self.assertEqual(len(fan["boundary_point_ordinals"]), 4)
        original, replacement, axes = retriangulate(s, fan, fields, normals, ids)
        self.assertEqual(len(replacement[2]), 2)
        result = compare(original, replacement, axes=axes)
        self.assertEqual(result["position_error_squared"], F(1, 64))
        self.assertEqual(result["field_error_absolute"], (0,) * 8)
        self.assertEqual(before, (s["positions"].tobytes(), s["indices"].tobytes()))

    def test_normal_code_boundaries_and_open_fans_are_not_removed(self):
        s = fan_source()
        s["attributes"][0]["uv"][0, 0] = 512
        self.assertEqual(discover(s)[0], ())
        s = fan_source()
        s["indices"] = s["indices"][:3]
        s["parts"] = ({"vertices": 5, "triangles": 3, "material": 0},)
        self.assertEqual(discover(s)[0], ())


if __name__ == "__main__":
    unittest.main()
