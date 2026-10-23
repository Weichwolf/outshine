"""Independent positional, rank and labelled-triangle controls."""

import unittest
from unittest.mock import patch

import numpy as np
from coupled_vertex_clusters import cluster_mesh, feasible_segment, qef_direction


class CoupledClustersTest(unittest.TestCase):
    def test_unaggregated_positions_keep_their_exact_bits(self):
        p = np.array([[0.123, 0, 0], [10.123, 0, 0], [0, 10.123, 0]], np.float32)
        faces = np.array([[0, 1, 2]], np.uint16)
        for method in ("centre", "qef"):
            out, report = cluster_mesh(p, faces, method=method, radius=0.1)
            np.testing.assert_array_equal(
                out["positions"][out["input_vertex_map"]].view(np.uint32),
                p.view(np.uint32),
            )
            self.assertEqual(report["qef_eigendecompositions"], 0)

    def test_cluster_shares_vertices_and_keeps_collapsed_triangles_explicit(self):
        p = np.array([[0.01, 0, 0], [0.02, 0, 0], [0.01, 0.01, 0]], np.float32)
        faces = np.array([[0, 1, 2], [2, 1, 0]], np.uint16)
        out, report = cluster_mesh(p, faces, method="qef")
        self.assertEqual(len(out["positions"]), 1)
        self.assertEqual(len(out["indices"]), 2)
        self.assertEqual(report["zero_area_triangles"], 2)

    def test_rank_two_fit_preserves_nullspace_closest_to_centre(self):
        p = np.array(
            [[0, 0, 2], [1, 0, 2], [0, 1, 2], [3, 0, 0], [3, 1, 0], [3, 0, 1]],
            np.float32,
        )
        faces = np.array([[0, 1, 2], [3, 4, 5]], np.uint16)
        centre = np.array([[0, 5, 0]], float)
        delta, report = qef_direction(
            p, faces, np.zeros(6, int), centre, np.array([True])
        )
        np.testing.assert_allclose(centre + delta, [[3, 5, 2]], atol=1e-12)
        self.assertEqual(report["qef_rank_counts"], [0, 0, 1, 0])

    def test_rank_zero_fit_stays_at_centre(self):
        p = np.zeros((3, 3), np.float32)
        delta, report = qef_direction(
            p,
            np.array([[0, 1, 2]]),
            np.zeros(3, int),
            np.zeros((1, 3)),
            np.array([True]),
        )
        np.testing.assert_array_equal(delta, 0)
        self.assertEqual(report["qef_rank_counts"], [1, 0, 0, 0])

    def test_segment_clamps_against_every_original_vertex_even_for_large_fit(self):
        p = np.array([[0.2, 0, 0], [-0.2, 0, 0]])
        placed, alpha = feasible_segment(
            p, np.zeros(2, int), np.zeros((1, 3)), np.array([[1e12, -2e12, 3e12]]), 1
        )
        self.assertLess(alpha[0], 1)
        self.assertAlmostEqual(
            float(np.linalg.norm(placed - p, axis=1).max()), 1, places=12
        )

    def test_tangent_from_a_tight_ball_boundary_cannot_move(self):
        p = np.array([[1, 0, 0], [-1, 0, 0]], float)
        placed, alpha = feasible_segment(
            p, np.zeros(2, int), np.zeros((1, 3)), np.array([[0, 1, 0]], float), 1
        )
        np.testing.assert_array_equal(placed, 0)
        self.assertEqual(alpha[0], 0)

    def test_barycentric_correspondence_bounds_triangle_interiors(self):
        rng = np.random.default_rng(2026)
        p = rng.uniform(-5, 5, (100, 3)).astype(np.float32)
        faces = rng.integers(0, len(p), (100, 3), dtype=np.uint16)
        weights = rng.dirichlet([1, 1, 1], 100)
        for method in ("centre", "qef"):
            out, report = cluster_mesh(p, faces, method=method, radius=1)
            original = np.einsum("ni,nij->nj", weights, p[faces].astype(float))
            mapped = out["positions"][out["input_vertex_map"][faces]].astype(float)
            changed = np.einsum("ni,nij->nj", weights, mapped)
            self.assertLessEqual(
                float(np.linalg.norm(changed - original, axis=1).max()),
                report["geometric_model_displacement_bound_m"],
            )

    def test_reorientation_records_the_corner_permutation(self):
        p = np.array(
            [
                [0, 0, 0],
                [1.49, 1.51, 0],
                [2.51, 3.49, 0],
                [0.01, 0.01, 0],
                [1.48, 1.52, 0],
                [2.52, 3.48, 0],
            ],
            np.float32,
        )
        faces = np.array([[0, 1, 2]], np.uint16)
        out, report = cluster_mesh(p, faces, method="centre", radius=np.sqrt(3) / 2)
        self.assertEqual(report["reoriented_triangles"], 1)
        self.assertTrue(out["corner_swap"][0])
        old = p[faces[0]].astype(float)
        new = out["positions"][out["indices"][0]].astype(float)
        self.assertGreater(
            np.cross(old[1] - old[0], old[2] - old[0])
            @ np.cross(new[1] - new[0], new[2] - new[0]),
            0,
        )

    def test_invalid_radius_and_nonfinite_coordinates_fail(self):
        p = np.zeros((3, 3), np.float32)
        for radius in (0, -1, np.inf):
            with self.subTest(radius=radius), self.assertRaises(ValueError):
                cluster_mesh(p, np.array([[0, 1, 2]]), method="centre", radius=radius)
        p[0, 0] = np.nan
        with self.assertRaises(ValueError):
            cluster_mesh(p, np.array([[0, 1, 2]]), method="qef")

    def test_solver_failure_keeps_the_valid_centre_instead_of_loosening_radius(self):
        p = np.array([[0.01, 0, 0], [0.02, 0, 0], [0.01, 0.01, 0]], np.float32)
        indices = np.array([[0, 1, 2]], np.uint16)
        centre, _ = cluster_mesh(p, indices, method="centre")
        with patch(
            "coupled_vertex_clusters.np.linalg.eigh",
            side_effect=np.linalg.LinAlgError("injected convergence failure"),
        ):
            result, report = cluster_mesh(p, indices, method="qef")
        np.testing.assert_array_equal(
            result["positions"].view(np.uint32), centre["positions"].view(np.uint32)
        )
        self.assertTrue(report["qef_solver_failure"])
        self.assertEqual(report["numerical_centre_fallback_clusters"], 1)
        self.assertLessEqual(report["maximum_prepack_vertex_shift_m"], 3)
