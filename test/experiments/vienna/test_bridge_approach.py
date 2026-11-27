import unittest

import numpy as np
from bridge_approach import bridge_approaches
from outshine_ref.transport.water_span import WaterSpan
from shapely.geometry import LineString, box


class BridgeApproachTest(unittest.TestCase):
    def test_tagged_exit_does_not_cut_the_through_carriageway_short(self):
        deck = box(0, -8, 50, 8)
        span = WaterSpan(np.array([0, 0]), np.array([1, 0]), 50, 4, 0)
        features = [
            ("transportation", LineString(points), tags)
            for points, tags in [
                ([(50, 0), (64, 0)], {"class": "motorway"}),
                ([(64, 0), (165, 0)], {"class": "motorway"}),
                ([(64, 0), (94, 15)], {"class": "motorway", "ramp": 1}),
            ]
        ]
        [approach] = bridge_approaches(
            [(deck, span)], features, lambda xy: np.zeros(len(xy)), box(0, 20, 50, 30)
        )
        self.assertGreater(approach.profile.length_m, 50)
        np.testing.assert_allclose(approach.direction, [1, 0])

    def test_matching_ground_route_gets_a_bounded_profile_at_the_mapped_deck_edge(self):
        deck = box(0, -6, 50, 6)
        water = box(5, -100, 45, 100)
        span = WaterSpan(np.array([0, 0]), np.array([1, 0]), 50, 4, 0.01)
        features = [
            ("transportation", LineString([(50.2, 0), (145, 0)]), {"class": "primary"}),
            (
                "transportation",
                LineString([(50.2, 6), (50.2, 30)]),
                {"class": "primary"},
            ),
            (
                "transportation",
                LineString([(50.2, 4), (145, 4)]),
                {"class": "primary", "brunnel": "tunnel"},
            ),
        ]
        terrain = lambda xy: 7 + 0.003 * xy[:, 0]
        approaches = bridge_approaches([(deck, span)], features, terrain, water)
        self.assertEqual(len(approaches), 1)
        approach = approaches[0]
        self.assertAlmostEqual(approach.mouth_edge.length, 7)
        self.assertEqual(approach.footprint.intersection(deck).area, 0)
        self.assertAlmostEqual(
            approach.deck_contact.intersection(approach.footprint).length,
            approach.mouth_edge.length,
        )
        start, end = (
            approach.mouth_edge.bounds[0],
            approach.origin[0] + approach.profile.length_m,
        )
        points = np.array([[start, -3.5], [start, 3.5], [end, 2]])
        ground = np.c_[points, terrain(points)]
        self.assertTrue(np.allclose(approach.heights(ground), [4.5, 4.5, ground[2, 2]]))
        self.assertLessEqual(approach.profile.grade_bound, 0.08)
