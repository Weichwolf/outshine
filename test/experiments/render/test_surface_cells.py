"""SAT controls independent of rational six-plane polygon clipping."""

import unittest

import numpy as np
from surface_cell_oracle import clipped
from surface_cells import assemble, overlap


class SurfaceCells(unittest.TestCase):
    def test_sat_matches_exact_clipping_for_dyadic_triangles_and_contacts(self):
        rng = np.random.default_rng(885)
        for _ in range(128):
            tri = rng.integers(-8, 9, (3, 3)) / 2
            if not np.cross(tri[1] - tri[0], tri[2] - tri[0]).any():
                continue
            centres = rng.integers(-4, 5, (8, 3)).astype(float)
            expected = [clipped(tri, c) for c in centres]
            actual, _ = overlap(tri, centres)
            np.testing.assert_array_equal(actual, expected)

    def test_exact_plane_boundary_keeps_both_sides_and_all_original_ids(self):
        tri = np.array([[[0, 0, 0], [1, 0, 0], [0, 1, 0]]], float)
        result, work = assemble(np.repeat(tri, 2, axis=0), pitch=1)
        self.assertEqual(result["fallback"].size, 0)
        xyz = result["origin"] + (result["coordinates"].astype(float) + 0.5)
        self.assertEqual(set(xyz[:, 2]), {-0.5, 0.5})
        self.assertEqual(set(result["original_faces"]), {0, 1})
        self.assertEqual(work["accepted_incidences"], 2 * work["occupied_cells"])

    def test_budget_keeps_small_unrelated_faces_and_original_heavy_face(self):
        tri = np.array(
            [
                [[0, 0, 0], [100, 0, 0], [0, 100, 0]],
                [[200, 0, 0], [201, 0, 0], [200, 1, 0]],
            ],
            float,
        )
        result, work = assemble(tri, pitch=1, candidate_limit=100)
        np.testing.assert_array_equal(result["fallback"], [0])
        self.assertEqual(set(result["original_faces"]), {1})
        self.assertLessEqual(work["candidate_cells"], 100)

    def test_incidence_face_and_grid_limits_return_original_fallback(self):
        tri = np.array([[[0, 0, 0], [1, 0, 0], [0, 1, 0]]], float)
        for kwargs, reason in (
            ({"incidence_limit": 0}, 5),
            ({"face_limit": 0}, 3),
            ({"candidate_limit": 0}, 4),
        ):
            result, _ = assemble(tri, pitch=1, **kwargs)
            np.testing.assert_array_equal(result["fallback"], [0])
            self.assertEqual(result["reasons"][0], reason)
        result, _ = assemble(tri * 100000, pitch=1)
        self.assertEqual(result["reasons"][0], 2)

    def test_empty_degenerate_and_invalid_inputs(self):
        result, work = assemble(np.empty((0, 3, 3)), pitch=1)
        self.assertEqual(result["coordinates"].shape, (0, 3))
        self.assertEqual(work["occupied_cells"], 0)
        result, _ = assemble(np.zeros((1, 3, 3)), pitch=1)
        self.assertEqual(result["reasons"][0], 1)
        for kwargs in (
            {"pitch": 0},
            {"pitch": 1e-300},
            {"candidate_limit": -1},
            {"incidence_limit": 524289},
        ):
            with self.assertRaises(ValueError):
                assemble(np.zeros((1, 3, 3)), **({"pitch": 1} | kwargs))


if __name__ == "__main__":
    unittest.main()
