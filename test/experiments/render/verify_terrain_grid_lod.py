"""Independent interpolation and crossing negatives for terrain index LOD bounds."""

import unittest

import numpy as np
from matplotlib import tri
from terrain_grid_lod import stencil, triangles


def interpolate(fractions, faces, values, points):
    xy = np.stack(np.meshgrid(fractions, fractions), axis=-1).reshape(-1, 2)
    mesh = tri.Triangulation(xy[:, 0], xy[:, 1], faces)
    result = tri.LinearTriInterpolator(mesh, values)(points[:, 0], points[:, 1])
    assert not np.any(result.mask)
    return np.asarray(result)


class TerrainGridLod(unittest.TestCase):
    def test_coarse_boundary_diagnostic_retains_its_declared_surface_partition(self):
        fractions = np.linspace(0, 1, 9)
        xy = np.stack(np.meshgrid(fractions, fractions), axis=-1).reshape(-1, 2)
        values = xy[:, 0] * xy[:, 1]
        points = np.random.default_rng(32).uniform(0, 1, (10000, 2))
        for stride in (2, 4, 8):
            with self.subTest(stride=stride):
                indices, weights, count = stencil(fractions, stride, False)
                self.assertEqual(count, 2 * (8 // stride) ** 2)
                bound = np.abs(np.sum(values[indices] * weights, axis=1)).max()
                a = interpolate(fractions, triangles(9, 1), values, points)
                b = interpolate(fractions, triangles(9, stride, False), values, points)
                self.assertLessEqual(float(np.abs(a - b).max()), bound + 1e-12)

    def test_crossing_can_be_wrong_without_any_wrong_grid_vertex(self):
        fractions = np.array((0, 0.5, 1))
        fine, coarse = triangles(3, 1), triangles(3, 2)
        values = np.array((0, 1, 0, 0, 0, 1, 0, 0, 0), dtype=float)
        xy = np.stack(np.meshgrid(fractions, fractions), axis=-1).reshape(-1, 2)
        np.testing.assert_allclose(
            interpolate(fractions, coarse, values, xy), values, atol=1e-12
        )
        crossing = np.array(((0.75, 0.25),))
        a = interpolate(fractions, fine, values, crossing)
        b = interpolate(fractions, coarse, values, crossing)
        self.assertAlmostEqual(float(np.abs(a - b).max()), 1)
        indices, weights, _ = stencil(fractions, 2)
        bound = np.abs(np.sum(values[indices] * weights, axis=1)).max()
        self.assertAlmostEqual(float(bound), 1)

    def test_affine_fields_remain_affine_on_irregular_native_fractions(self):
        fractions = np.floor(np.arange(33) * 255 / 32) / 255
        xy = np.stack(np.meshgrid(fractions, fractions), axis=-1).reshape(-1, 2)
        values = 17 + 2 * xy[:, 0] - 3 * xy[:, 1]
        for stride in (2, 4, 8, 16, 32):
            with self.subTest(stride=stride):
                indices, weights, _ = stencil(fractions, stride)
                self.assertLess(
                    np.abs(np.sum(values[indices] * weights, axis=1)).max(), 1e-12
                )

    def test_bound_covers_independent_dense_scalar_and_vector_fields(self):
        fractions = np.linspace(0, 1, 9)
        rng = np.random.default_rng(12)
        values = rng.normal(size=(81, 3))
        points = rng.uniform(0, 1, (20000, 2))
        fine = triangles(9, 1)
        for stride in (2, 4, 8):
            with self.subTest(stride=stride):
                coarse = triangles(9, stride)
                indices, weights, _ = stencil(fractions, stride)
                bound = np.linalg.norm(
                    np.einsum("ijk,ij->ik", values[indices], weights), axis=1
                ).max()
                observed = np.column_stack(
                    [
                        interpolate(fractions, fine, values[:, c], points)
                        - interpolate(fractions, coarse, values[:, c], points)
                        for c in range(3)
                    ]
                )
                self.assertLessEqual(
                    np.linalg.norm(observed, axis=1).max(), bound + 1e-12
                )

    def test_omitted_peak_prevents_a_false_zero_error(self):
        fractions = np.linspace(0, 1, 9)
        values = np.zeros(81)
        values[4 * 9 + 3] = 19
        indices, weights, _ = stencil(fractions, 8)
        self.assertAlmostEqual(
            float(np.abs(np.sum(values[indices] * weights, axis=1)).max()), 19
        )

    def test_full_resolution_winding_and_boundary_are_retained(self):
        fractions = np.linspace(0, 1, 33)
        xy = np.stack(np.meshgrid(fractions, fractions), axis=-1).reshape(-1, 2)
        expected = (2048, 696, 280, 168, 136, 128)
        for stride, count in zip((1, 2, 4, 8, 16, 32), expected):
            with self.subTest(stride=stride):
                faces = triangles(33, stride)
                self.assertEqual(len(faces), count)
                a, b, c = xy[faces[:, 0]], xy[faces[:, 1]], xy[faces[:, 2]]
                cross = (b[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1]) - (
                    b[:, 1] - a[:, 1]
                ) * (c[:, 0] - a[:, 0])
                self.assertTrue(np.all(cross < 0))
                edges = np.sort(
                    faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1
                )
                unique, counts = np.unique(edges, axis=0, return_counts=True)
                self.assertEqual(int((counts == 1).sum()), 128)
                self.assertTrue(np.all(counts <= 2))
                for a, b in unique[counts == 1]:
                    self.assertTrue(
                        np.any((xy[a] == xy[b]) & ((xy[a] == 0) | (xy[a] == 1)))
                    )


if __name__ == "__main__":
    unittest.main()
