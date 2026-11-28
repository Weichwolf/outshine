import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from park_avenue import avenue_sites


class ParkAvenueTest(unittest.TestCase):
    def test_parallel_footways_define_staggered_rows_in_the_verges(self):
        road = {"class": "minor", "foot": "yes", "bicycle": "yes", "horse": "yes"}
        path = {"class": "path", "subclass": "footway"}
        features = [
            (
                "landcover",
                shapely.box(-80, -32, 80, 32),
                {"subclass": "recreation_ground"},
            ),
            ("transportation", shapely.LineString([(-80, 0), (80, 0)]), road),
            ("transportation", shapely.LineString([(0, -30), (0, 30)]), path),
            *(
                ("transportation", shapely.LineString([(-80, y), (80, y)]), path)
                for y in (-19, -11, 11, 19)
            ),
        ]
        source = SimpleNamespace(
            features=features, lon=16.4, lat=48.2, metres=np.array([74022, 111184])
        )
        _, spots, _ = avenue_sites(source, 75, 99)
        points = shapely.points(spots)
        np.testing.assert_allclose(
            np.unique(spots[:, 1]), [-22.4, -15, -7.6, 7.6, 15, 22.4]
        )
        for _, transport, _ in features[1:]:
            self.assertTrue((shapely.distance(points, transport) > 1.75).all())
        inner = spots[np.isclose(spots[:, 1], 7.6), 0]
        middle = spots[np.isclose(spots[:, 1], 15), 0]
        self.assertAlmostEqual(np.min(abs(inner[:, None] - middle)), 3.8)

        trail = (
            "transportation",
            shapely.LineString([(-80, 24), (80, 24)]),
            {"class": "path", "subclass": "path"},
        )
        source.features = [*features, trail]
        _, bounded, _ = avenue_sites(source, 75, 99)
        np.testing.assert_allclose(
            np.unique(bounded[:, 1]), [-22.4, -15, -7.6, 7.6, 15, 21.5]
        )
        source.features = [*features[:3], trail]
        _, crossing_only, _ = avenue_sites(source, 75, 99)
        np.testing.assert_allclose(np.unique(crossing_only[:, 1]), [-5.75, 5.75])

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

        source.features = [
            (name, shapely.transform(geometry, lambda xy: xy + [30, 56]), tags)
            for name, geometry, tags in features
        ]
        _, corner_spots, _ = avenue_sites(source, 75, 99)
        self.assertTrue(np.any(np.linalg.norm(corner_spots, axis=1) > 75))
        self.assertTrue(np.all(np.abs(corner_spots) < 75))

        # An ordinary route through the same lawn is insufficient evidence.
        source.features = [
            (name, geometry, tags | {"horse": "no"})
            for name, geometry, tags in features
        ]
        self.assertEqual(len(avenue_sites(source, 75, 99)[1]), 0)
