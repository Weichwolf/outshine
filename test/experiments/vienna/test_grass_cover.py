"""Grass roots follow the rendered terrain, including material cutouts."""

import unittest
from unittest.mock import patch

import numpy as np
from ground_surface import surface_elevations
from meadow import meadow_cover


class GrassCoverTest(unittest.TestCase):
    def test_roots_follow_slopes_without_filling_a_path(self):
        vertices = [
            (0, 0, 0),
            (2, 0, 2),
            (0, 2, 2),
            (2, 2, 4),
            (3, 0, 3),
            (5, 0, 5),
            (3, 2, 5),
        ]
        mesh = vertices, [(0, 1, 2), (1, 3, 2), (4, 5, 6)]
        sites = np.array([(0.25, 0.5), (1, 1), (2.5, 1), (3.5, 1)])
        np.testing.assert_allclose(
            surface_elevations(mesh, sites), [0.75, 2, -np.inf, 4.5]
        )

    def test_meadow_leaves_and_flowers_stop_at_the_solved_path_edges(self):
        vertices = [(x, y, 0.15 * x) for x in (0, 2, 3, 5) for y in (0, 2)]
        mesh = vertices, [(0, 2, 1), (1, 2, 3), (4, 6, 5), (5, 6, 7)]
        xy = np.array([(1, 1), (1.97, 1), (2.5, 1), (3.03, 1), (4, 1)])
        marks = np.tile((0.98, 0.65, 0.10), (len(xy), 1))
        with (
            patch("meadow.tile_sites", return_value=[(xy, marks)]),
            patch("meadow.patch_density", return_value=np.full(len(xy) - 1, 0.5)),
        ):
            products = list(
                meadow_cover(mesh, None, np.array([2.5, 1, 2]), 17, focal_pixels=700)
            )
        self.assertIn("meadow purple flowers", {name for name, _, _ in products})
        for _, points, _ in products:
            x = points[:, 0]
            self.assertTrue(np.all((x < 1.5) | (x > 3.5)))
