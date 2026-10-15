"""Inspection clipping splits queries while preserving one total candidate budget."""

import unittest

import numpy as np
from ground_surface_showcase import sample_vertices
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


def source():
    xy = np.array([[0, 0], [4, 0], [4, 4], [0, 4]], float)
    return Mesh(np.c_[xy, 10 + xy @ [0.1, 0.2]], [[0, 1, 2], [0, 2, 3]])


class ClippingBudgets(unittest.TestCase):
    def test_tilted_points_and_source_are_preserved_across_query_batches(self):
        mesh = source()
        before = mesh.vertices.copy()
        sampler = SurfaceSampler(mesh)
        xy = np.array([[0.5, 0.25], [1.5, 0.25], [3.5, 3.75], [0.5, 3.75], [2.5, 1.25]])
        queries = xy.copy()
        calls = []

        class TrackedSampler:
            def sample(self, points, **limits):
                calls.append((len(points), limits))
                return sampler.sample(points, **limits)

        points = sample_vertices(
            TrackedSampler(), xy, batch_queries=2, max_candidates=16
        )
        np.testing.assert_array_equal(points[:, :2], queries)
        np.testing.assert_allclose(
            points[:, 2], 10 + xy @ [0.1, 0.2], atol=1e-12, rtol=0
        )
        np.testing.assert_array_equal(mesh.vertices, before)
        np.testing.assert_array_equal(xy, queries)
        self.assertEqual([n for n, _ in calls], [2, 2, 1])
        self.assertEqual([r["max_queries"] for _, r in calls], [2, 2, 2])
        self.assertEqual([r["max_candidates"] for _, r in calls], [16, 14, 12])

    def test_candidate_budget_is_not_reset_after_a_query_batch(self):
        xy = np.array([[0.5, 0.25], [1.5, 0.25], [3.5, 3.75], [0.5, 3.75]])
        with self.assertRaisesRegex(ValueError, "candidate_budget"):
            sample_vertices(
                SurfaceSampler(source()), xy, batch_queries=2, max_candidates=3
            )


if __name__ == "__main__":
    unittest.main()
