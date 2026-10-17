"""Terrain partitions preserve the complete native interior without new edges."""

import unittest

import numpy as np
from terrain_grid_lod import triangles
from terrain_visibility import partition


def ordered_rows(rows):
    return rows[np.lexsort(rows.T)]


class TerrainVisibility(unittest.TestCase):
    def test_every_partition_is_the_exact_original_triangle_union(self):
        original = ordered_rows(triangles(33, 1))
        for cells in (4, 8, 16, 32):
            faces, nodes = partition(cells)
            self.assertEqual(faces.shape, ((32 // cells) ** 2, 2 * cells * cells, 3))
            np.testing.assert_array_equal(ordered_rows(faces.reshape(-1, 3)), original)
            for face, points in zip(faces, nodes):
                np.testing.assert_array_equal(np.unique(face), np.sort(points))

    def test_patch_bounds_enclose_every_point_of_the_unchanged_surface(self):
        points = np.random.default_rng(12).normal(size=(1089, 3))
        for cells in (4, 8, 16, 32):
            faces, nodes = partition(cells)
            lo = points[nodes].min(axis=1)
            hi = points[nodes].max(axis=1)
            for i, face in enumerate(faces):
                self.assertTrue(np.all(points[face] >= lo[i]))
                self.assertTrue(np.all(points[face] <= hi[i]))
                weights = np.array((0.2, 0.3, 0.5))
                interior = np.einsum("ijk,j->ik", points[face], weights)
                self.assertTrue(np.all(interior >= lo[i]))
                self.assertTrue(np.all(interior <= hi[i]))


if __name__ == "__main__":
    unittest.main()
