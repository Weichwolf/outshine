import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from park_avenue import avenue_sites


class ParkAvenueTest(unittest.TestCase):
    def test_shared_route_has_two_rows_with_clear_paths_and_buildings(self):
        route = shapely.LineString([(-70, 0), (70, 0)])
        crosswalk = shapely.LineString([(11, -15), (11, 15)])
        building = shapely.box(-28, 3, -18, 10)
        water = shapely.box(24, -12, 39, -3)
        wood = shapely.box(-60, 4, -38, 12)
        road_tags = {"class": "minor", "foot": "yes", "bicycle": "yes", "horse": "yes"}
        features = [
            (
                "landcover",
                shapely.box(-70, -20, 70, 20),
                {"subclass": "recreation_ground"},
            ),
            ("landcover", wood, {"class": "wood"}),
            ("transportation", route, road_tags),
            ("transportation", shapely.reverse(route), road_tags),
            ("transportation", crosswalk, {"class": "path"}),
            ("building", building, {}),
            ("water", water, {}),
        ]
        source = SimpleNamespace(
            features=features, lon=16.4, lat=48.2, metres=np.array([74022, 111184])
        )
        lawn, spots, marks = avenue_sites(source, 75, 99)
        points = shapely.points(spots)
        self.assertEqual(len(points), len(marks))
        self.assertGreater(len(points), 10)
        self.assertEqual(len(np.unique(spots, axis=0)), len(spots))
        self.assertEqual(set(np.sign(spots[:, 1])), {-1, 1})
        self.assertTrue(shapely.covers(lawn, points).all())
        self.assertTrue((shapely.distance(route, points) > 5).all())
        self.assertTrue((shapely.distance(crosswalk, points) > 0.9).all())
        self.assertTrue((shapely.distance(building, points) > 4.5).all())
        self.assertTrue((shapely.distance(water, points) > 4.5).all())
        self.assertFalse(shapely.intersects(wood, points).any())

        # An ordinary route through the same lawn is insufficient evidence.
        source.features = [
            (name, geometry, tags | {"horse": "no"})
            for name, geometry, tags in features
        ]
        self.assertEqual(len(avenue_sites(source, 75, 99)[1]), 0)
