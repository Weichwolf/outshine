import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from woodland_sites import spaced_sites, woodland_sites


class WoodlandSitesTest(unittest.TestCase):
    def test_spacing_is_independent_of_candidate_order(self):
        xy = np.array([[0, 0], [0, 1], [6, 1], [6, 2], [9, 8]], dtype=float)
        ranks = np.array([0.2, 0.8, 0.3, 0.3, 0.1])
        keep = spaced_sites(xy, ranks, 2.9)
        np.testing.assert_array_equal(keep, [True, False, True, False, True])
        np.testing.assert_array_equal(
            spaced_sites(xy[::-1], ranks[::-1], 2.9)[::-1], keep
        )

    def test_crop_keeps_neighbour_competition_and_the_walking_reservation(self):
        xy = np.array([[0, 0], [4, 0], [9, 0], [11, 0], [18, 0], [8, 8]], dtype=float)
        marks = np.zeros((len(xy), 3))
        domain = shapely.box(-30, -30, 30, 30).difference(shapely.box(3, -1, 5, 1))
        source = SimpleNamespace(lon=0, lat=0, metres=np.ones(2))

        def collect(_, centre, radius, seed, **settings):
            inside = np.linalg.norm(xy - centre, axis=1) < radius
            yield xy[inside], marks[inside]

        def field(_, positions, seed, *, scale):
            return (
                (30 - positions[:, 0]) / 30
                if seed == 0xA91B
                else np.full(len(positions), 0.5)
            )

        with (
            patch("woodland_sites.tile_sites", side_effect=collect),
            patch("woodland_sites.patch_density", side_effect=field),
        ):
            full, _ = woodland_sites(source, domain, np.zeros(2), 20, 0)
            inner, _ = woodland_sites(source, domain, np.zeros(2), 10, 0)

        np.testing.assert_array_equal(full, [[0, 0], [11, 0], [18, 0], [8, 8]])
        np.testing.assert_array_equal(inner, [[0, 0], [8, 8]])
