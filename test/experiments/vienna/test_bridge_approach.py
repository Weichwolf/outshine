import unittest

import numpy as np
import shapely
from bridge_approach import bridge_approaches, fit_approach
from bridge_contacts import bank_cuts
from bridges import WaterCrossing
from outshine_ref.transport.water_span import WaterSpan
from shapely.affinity import affine_transform
from shapely.geometry import LineString, box


class BridgeApproachTest(unittest.TestCase):
    def test_bank_cut_exposes_a_buried_deck_without_cutting_the_lower_route(self):
        extent = box(0, 0, 4, 2)
        span = WaterSpan(np.array([0, 1]), np.array([1, 0]), 4, 3, 0)
        crossing = WaterCrossing(extent, span, extent, 1)
        vertices = np.array([[0, 0, 0], [4, 0, 4], [4, 2, 4], [0, 2, 0]])
        [cut] = bank_cuts(vertices, [[0, 1, 2, 3]], [crossing], extent)
        self.assertFalse(cut.footprint.covers(shapely.Point(2, 1)))
        self.assertTrue(cut.footprint.covers(shapely.Point(3.5, 1)))
        self.assertAlmostEqual(cut.heights(np.array([[3.5, 1, 3.5]]))[0], 2.88)

    def test_oblique_deck_contact_uses_the_whole_carrying_edge(self):
        along = np.array([0.77772399, 0.62860591])
        along /= np.linalg.norm(along)
        east, north = along
        origin = np.array([205.69, -308.65])
        transform = (east, -north, north, east, *origin)
        deck = affine_transform(box(-50, -8, 0, 8), transform)
        line = affine_transform(LineString([(0.09, 0), (110, 0)]), transform)
        span = WaterSpan(origin - 50 * along, along, 50, 3.5, 0.01)
        approach = fit_approach(
            line, deck, span, lambda xy: np.full(len(xy), 2.0), 7, 90
        )
        self.assertIsNotNone(approach)
        self.assertAlmostEqual(approach.mouth_edge.length, 7)
        points = np.array(approach.mouth_edge.coords)
        np.testing.assert_allclose(
            approach.heights(np.column_stack((points, np.zeros(len(points))))),
            span.height(points),
        )

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
