"""Check stitched patch topology and bounds with independent interpolation."""

import itertools
import unittest

import numpy as np
from matplotlib import tri
from terrain_grid_lod import stencil, triangles
from terrain_patch_stitching import balanced, boundaries, stitched
from terrain_visibility import partition


def page_faces(levels):
    _, nodes = partition(8)
    return np.concatenate(
        [
            nodes[i][
                triangles(
                    9, 1 << int(level), boundary_steps=boundaries(levels, *divmod(i, 4))
                )
            ]
            for i, level in enumerate(levels.ravel())
        ]
    )


class TerrainPatchStitching(unittest.TestCase):
    def test_random_neighbors_have_no_internal_open_or_t_junction_edges(self):
        rng = np.random.default_rng(515)
        for trial in range(30):
            initial = rng.integers(0, 4, (4, 4))
            levels = balanced(initial) if trial % 2 else initial
            self.assertTrue(np.all(levels <= initial))
            if trial % 2:
                self.assertTrue(np.all(np.abs(np.diff(levels, axis=0)) <= 1))
                self.assertTrue(np.all(np.abs(np.diff(levels, axis=1)) <= 1))
            faces = page_faces(levels)
            edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
            unique, counts = np.unique(edges, axis=0, return_counts=True)
            expected = set()
            for i in range(32):
                expected.update(
                    (
                        (i, i + 1),
                        (1056 + i, 1057 + i),
                        (33 * i, 33 * (i + 1)),
                        (33 * i + 32, 33 * (i + 1) + 32),
                    )
                )
            self.assertEqual(set(map(tuple, unique[counts == 1])), expected)
            self.assertTrue(np.all(counts <= 2))

    def test_variant_envelopes_bound_dense_irregular_vector_surfaces(self):
        fx = np.array([0, 0.07, 0.23, 0.29, 0.48, 0.61, 0.79, 0.93, 1])
        fy = np.array([0, 0.04, 0.19, 0.38, 0.47, 0.58, 0.71, 0.88, 1])
        xy = np.stack(np.meshgrid(fx, fy), axis=-1).reshape(-1, 2)
        rng = np.random.default_rng(328)
        values = rng.normal(size=(81, 3))
        points = rng.uniform(0, 1, (1000, 2))
        fine = tri.Triangulation(xy[:, 0], xy[:, 1], triangles(9, 1))
        expected = np.column_stack(
            [tri.LinearTriInterpolator(fine, values[:, i])(*points.T) for i in range(3)]
        )
        for stride in (2, 4, 8):
            variants = set(itertools.product((stride, stride // 2), repeat=4)) | {
                (1, stride, 1, stride)
            }
            for edges in variants:
                indices, weights, _ = stencil(
                    fx, stride, vertical=fy, boundary_steps=edges
                )
                bound = np.linalg.norm(
                    np.einsum("ijk,ij->ik", values[indices], weights), axis=1
                ).max()
                mesh = tri.Triangulation(
                    xy[:, 0], xy[:, 1], triangles(9, stride, boundary_steps=edges)
                )
                observed = np.column_stack(
                    [
                        tri.LinearTriInterpolator(mesh, values[:, i])(*points.T)
                        for i in range(3)
                    ]
                )
                self.assertLessEqual(
                    np.linalg.norm(observed - expected, axis=1).max(), bound + 1e-11
                )

    def test_flat_page_keeps_outer_edges_with_fewer_internal_segments(self):
        f = np.linspace(0, 1, 33)
        xy = np.stack(np.meshgrid(f, f), axis=-1).reshape(-1, 2)
        local = np.column_stack((xy, 5 + 3 * xy[:, 0] - 2 * xy[:, 1]))
        _, nodes = partition(8)
        levels, counts, error, rounds, _ = stitched(
            f, local, nodes, np.full(16, 1000.0), np.full(16, 3), 600, 0.8, {}
        )
        self.assertTrue(np.all(levels == 3))
        self.assertEqual(int(counts.sum()), 168)
        self.assertEqual(rounds, 0)
        self.assertLess(error.max(), 1e-8)

    def test_peak_and_near_plane_require_refinement(self):
        f = np.linspace(0, 1, 33)
        xy = np.stack(np.meshgrid(f, f), axis=-1).reshape(-1, 2)
        local = np.column_stack((xy, np.zeros(len(xy))))
        local[3 * 33 + 3, 2] = 1e6
        _, nodes = partition(8)
        levels, _, error, rounds, _ = stitched(
            f, local, nodes, np.full(16, 10.0), np.full(16, 3), 600, 0.8, {}
        )
        self.assertEqual(levels[0], 0)
        self.assertLessEqual(error.max(), 0.5)
        self.assertGreater(rounds, 0)
        levels, _, error, _, _ = stitched(
            f, local, nodes, np.zeros(16), np.full(16, 3), 600, 0.8, {}
        )
        self.assertTrue(np.all(levels == 0))
        self.assertTrue(np.all(error == 0))

    def test_existing_patterns_and_invalid_boundaries(self):
        for stride in (1, 2, 4, 8):
            np.testing.assert_array_equal(
                triangles(9, stride), triangles(9, stride, boundary_steps=(1,) * 4)
            )
            np.testing.assert_array_equal(
                triangles(9, stride, False),
                triangles(9, stride, boundary_steps=(stride,) * 4),
            )
        for steps in ((1, 1), (0, 1, 1, 1), (3, 1, 1, 1), (16, 1, 1, 1)):
            with self.assertRaises(ValueError):
                triangles(9, 8, boundary_steps=steps)


if __name__ == "__main__":
    unittest.main()
