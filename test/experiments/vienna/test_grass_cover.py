"""Grass roots follow the rendered terrain, including material cutouts."""

import unittest

import numpy as np
from plant_sites import root_elevations


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
            root_elevations(mesh, sites), [0.75, 2, -np.inf, 4.5]
        )
