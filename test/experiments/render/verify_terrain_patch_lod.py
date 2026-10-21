"""Independent dense interpolation and boundary checks for adaptive terrain."""

import unittest

import numpy as np
from matplotlib import tri
from terrain_grid_lod import stencil, triangles
from terrain_patch_lod import patch_bounds, projected_error


class TerrainPatchLod(unittest.TestCase):
    def test_different_irregular_axes_bound_dense_vector_interpolation(self):
        fx = np.array([0, 0.07, 0.23, 0.29, 0.48, 0.61, 0.79, 0.93, 1])
        fy = np.array([0, 0.04, 0.19, 0.38, 0.47, 0.58, 0.71, 0.88, 1])
        xy = np.stack(np.meshgrid(fx, fy), axis=-1).reshape(-1, 2)
        rng = np.random.default_rng(515)
        values = rng.normal(size=(81, 3))
        points = rng.uniform(0, 1, (12000, 2))
        fine_mesh = tri.Triangulation(xy[:, 0], xy[:, 1], triangles(9, 1))
        for stride in (2, 4, 8):
            indices, weights, _ = stencil(fx, stride, vertical=fy)
            bound = np.linalg.norm(np.einsum('ijk,ij->ik', values[indices], weights), axis=1).max()
            coarse_mesh = tri.Triangulation(xy[:, 0], xy[:, 1], triangles(9, stride))
            observed = np.column_stack([
                tri.LinearTriInterpolator(fine_mesh, values[:, i])(points[:, 0], points[:, 1])
                - tri.LinearTriInterpolator(coarse_mesh, values[:, i])(points[:, 0], points[:, 1])
                for i in range(3)
            ])
            self.assertLessEqual(np.linalg.norm(observed, axis=1).max(), bound + 1e-12)

    def test_affine_subpatches_retain_every_shared_boundary(self):
        f = np.floor(np.arange(33) * 255 / 32) / 255
        xy = np.stack(np.meshgrid(f, f), axis=-1).reshape(-1, 2)
        local = np.column_stack((xy, 17 + 3 * xy[:, 0] - 2 * xy[:, 1]))
        templates = {}
        for cells in (4, 8, 16, 32):
            errors, counts, nodes, strides = patch_bounds(f, local, cells, templates)
            self.assertLess(errors.max(), 2e-9)
            self.assertEqual(int(counts[:, 0].sum()), 2048)
            for patch in nodes:
                for stride in strides[1:]:
                    faces = patch[triangles(cells + 1, stride)]
                    a, b, c = local[faces].transpose(1, 0, 2)
                    self.assertTrue(np.all(np.cross(b - a, c - a)[:, 2] < 0))
                    edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
                    _, multiplicity = np.unique(edges, axis=0, return_counts=True)
                    self.assertEqual(int((multiplicity == 1).sum()), 4 * cells)
                    self.assertTrue(np.all(multiplicity <= 2))

    def test_interior_peak_is_not_lost_by_a_flat_boundary(self):
        f = np.linspace(0, 1, 33)
        xy = np.stack(np.meshgrid(f, f), axis=-1).reshape(-1, 2)
        local = np.column_stack((xy, np.zeros(len(xy))))
        local[3 * 33 + 3, 2] = 19
        errors, _, _, _ = patch_bounds(f, local, 8, {})
        self.assertGreater(errors[0, -1], 18)
        self.assertLess(errors[1:, -1].max(), 2e-9)

    def test_camera_inside_bound_keeps_full_detail(self):
        errors = np.array([[0, 1, 2], [0, 1, 2]], dtype=float)
        result = projected_error(errors, np.array([0, 100]), 600, 0.7)
        self.assertEqual(result[0, 0], 0)
        self.assertTrue(np.isinf(result[0, 1:]).all())
        self.assertTrue(np.isfinite(result[1]).all())


if __name__ == '__main__':
    unittest.main()
