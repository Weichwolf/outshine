"""Clipmap block rejection keeps coverage and unknown geometry."""

import unittest

import numpy as np

from current_visibility import project, raster
from terrain_clipmap import layout, positions
from terrain_clipmap_visibility import Blocks, partition, visible


class ClipmapVisibility(unittest.TestCase):
    def test_culling_preserves_rasterized_coverage_and_depth(self):
        mesh = layout((1.9, -3.9), levels=4, half_cells=16)
        vertices = positions(mesh, lambda xy: 0.05 * xy[:, 0] - 0.1 * xy[:, 1])
        for yaw in np.arange(0, 360, 15):
            angle = np.deg2rad(yaw)
            basis = np.array([[np.cos(angle), np.sin(angle), 0],
                              [-np.sin(angle), np.cos(angle), 0], [0, 0, -1]])
            view = (np.array([1.9, -3.9, 40]), basis, 48, 96, 54)
            projected, depth = project(vertices, view)
            reference = raster(projected, depth, mesh.triangles, view[3], view[4])
            self.assertTrue(np.all(reference > 0), "the reference must cover the whole image")
            for cells in (4, 8, 16):
                blocks = partition(mesh, vertices, cells)
                selected = visible(blocks, view)[blocks.owners]
                self.assertLess(selected.sum(), len(selected))
                actual = raster(projected, depth, mesh.triangles[selected], view[3], view[4])
                np.testing.assert_array_equal(actual, reference)

    def test_boxes_crossing_the_near_plane_and_unknown_bounds_survive(self):
        lower = np.array([[-10, -10, -1], [-1, -1, -10], [np.nan, 0, 0]])
        upper = np.array([[10, 10, 1], [1, 1, -1], [np.nan, 1, 1]])
        blocks = Blocks(lower, upper, np.arange(3), np.ones(3, dtype=int))
        view = (np.zeros(3), np.eye(3), 50, 100, 100)
        np.testing.assert_array_equal(visible(blocks, view), [True, False, True])

    def test_every_reference_triangle_belongs_to_one_enclosing_block(self):
        mesh = layout((17.9, -13.9), levels=5, half_cells=8)
        vertices = positions(mesh, lambda xy: np.sin(xy[:, 0]) * np.cos(xy[:, 1]) * 20)
        blocks = partition(mesh, vertices, 4)
        self.assertEqual(blocks.triangles.sum(), len(mesh.triangles))
        self.assertTrue(np.all(vertices[mesh.triangles] >= blocks.lower[blocks.owners, None]))
        self.assertTrue(np.all(vertices[mesh.triangles] <= blocks.upper[blocks.owners, None]))

    def test_invalid_views_do_not_silently_reject_geometry(self):
        bounds = np.zeros((1, 3))
        blocks = Blocks(bounds, bounds, np.array([0]), np.array([1]))
        for basis, focal in [(np.zeros((3, 3)), 50), (np.eye(3), 0)]:
            with self.assertRaises(ValueError):
                visible(blocks, (np.zeros(3), basis, focal, 100, 100))


if __name__ == "__main__":
    unittest.main()
