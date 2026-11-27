import unittest

import numpy as np
from outshine_ref.transport.water_span import solve_water_span
from shapely import affinity
from shapely.geometry import LineString, box


class WaterCrossing(unittest.TestCase):
    def test_oblique_deck_meets_both_banks_over_an_eroded_channel(self):
        deck, route, water = (
            affinity.rotate(g, 23, origin=(0, 0))
            for g in (
                box(-25, -4, 25, 4),
                LineString([(-30, 0), (30, 0)]),
                box(-15, -20, 15, 20),
            )
        )
        axis = np.array([np.cos(np.deg2rad(23)), np.sin(np.deg2rad(23))])

        def terrain(xy):
            station = xy @ axis
            return 50 + 0.03 * station - np.where(abs(station) < 15, 6, 0)

        for path in (route, route.reverse()):
            span = solve_water_span(deck, [path], water, terrain)
            self.assertIsNotNone(span)
            xy = np.array([-25, 0, 25])[:, None] * axis
            np.testing.assert_allclose(span.height(xy), [49.25, 50, 50.75])

    def test_no_opposing_dry_landing(self):
        deck = box(0, -2, 20, 2)
        route = LineString([(0, 0), (20, 0)])
        tide = box(7, -30, 50, 30)
        self.assertIsNone(
            solve_water_span(deck, [route], tide, lambda xy: np.zeros(len(xy)))
        )
