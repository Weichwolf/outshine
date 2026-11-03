"""Analytic raster controls for the clipmap image diagnostic."""

import unittest

import numpy as np

from terrain_clipmap import layout, positions
from terrain_clipmap_view import render, surface_points


class ClipmapImages(unittest.TestCase):
    def test_front_faces_cover_a_flat_surface_without_ring_cracks(self):
        sample = lambda xy: np.zeros(len(xy))
        mesh = layout(half_cells=8, levels=3)
        vertices = positions(mesh, sample)
        view = (np.array([0, 0, 10]), np.diag([1, 1, -1]), 16, 32, 32)
        image, depth, counters = render(vertices, mesh.triangles, view, sample)
        np.testing.assert_allclose(depth, 0.01, rtol=0, atol=1e-16)
        np.testing.assert_array_equal(image, np.broadcast_to(image[0, 0], image.shape))
        self.assertEqual(counters["covered_pixels"], 32 * 32)
        self.assertEqual(counters["unknown_normal_pixels"], 0)
        _, backwards, _ = render(vertices, mesh.triangles[:, ::-1], view, sample)
        self.assertFalse(np.any(backwards))

    def test_depth_reconstruction_intersects_the_analytic_plane(self):
        view = (np.array([2, 3, 10]), np.diag([1, 1, -1]), 16, 32, 32)
        _, points = surface_points(np.full((32, 32), 0.01), view)
        np.testing.assert_array_equal(points[:, 2], 0)
        np.testing.assert_array_equal(points[0], [-7.6875, 12.6875, 0])


if __name__ == "__main__":
    unittest.main()
