"""Independent opaque expectations for a controlled layered-leaf experiment."""

import unittest

import numpy as np
from grass_shadow import GrassShadowIndex, query_origins
from grass_shadow_reference import direct_visibility
from leaf_stack_cases import CASES, LIGHT, case_parts, full_geometry, receiver_points


class LeafStackChecks(unittest.TestCase):
    def test_exact_opaque_visibility_at_every_gap_and_winding(self):
        vertices, faces = full_geometry()
        points = receiver_points()
        normals = np.tile((1, 0, 0.0), (len(points), 1))
        origins = query_origins(points, normals, LIGHT, 2e-6)
        expected = np.array([True] + [False] * (len(CASES) - 1))
        oracle, counters = direct_visibility(
            vertices, faces, origins, LIGHT, np.full(len(points), 2.0)
        )
        np.testing.assert_array_equal(oracle, expected)
        self.assertGreater(counters["reference_triangle_tests"], 0)
        index = GrassShadowIndex(vertices, faces)
        for batch in (1, 4, 8192):
            actual, _ = index.visibility(points, normals, LIGHT, batch=batch)
            np.testing.assert_array_equal(actual, expected)

    def test_receiver_and_layer_sources_are_explicit(self):
        self.assertEqual(
            [len(case_parts(i)) - 1 for i in range(len(CASES))],
            [0, 1, 1, 1, 1, 1, 2, 1, 1],
        )
        for i in range(len(CASES)):
            for role, v, f in case_parts(i):
                n = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
                self.assertTrue(np.all(np.linalg.norm(n, axis=1) > 0))
                self.assertIn(role, ("receiver", "opaque", "sheet"))


if __name__ == "__main__":
    unittest.main()
