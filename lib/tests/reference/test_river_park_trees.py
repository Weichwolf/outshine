import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from outshine.vegetation.river_park_trees import riverside_verges, verge_tree_sites


class RiverParkTreeTest(unittest.TestCase):
    def setUp(self):
        self.extent = shapely.box(0, 0, 125, 100)
        self.river = shapely.box(-10, -20, 130, 4)
        self.path = shapely.LineString([(0, 12), (125, 12)])
        self.park = ("landcover", self.extent, {"class": "grass", "subclass": "park"})
        self.walk = (
            "transportation",
            self.path,
            {"class": "path", "subclass": "cycleway"},
        )

    def test_verges_reserve_the_path_water_wood_and_open_recreation_core(self):
        features = (
            self.park,
            self.walk,
            ("water", self.river, {"class": "river"}),
            ("building", shapely.box(17, 16, 26, 24), {}),
            ("landcover", shapely.box(33, 16, 44, 24), {"class": "wood"}),
            (
                "transportation",
                shapely.LineString([(103, -15), (103, 45)]),
                {"class": "primary", "brunnel": "bridge"},
            ),
            (
                "landcover",
                shapely.box(57, 8, 80, 32),
                {"class": "grass", "subclass": "recreation_ground"},
            ),
        )
        domain = riverside_verges(features, self.extent)
        self.assertFalse(shapely.contains_xy(domain, 103, 8))
        self.assertTrue(shapely.contains_xy(domain, 118, 8))
        xy = np.array(
            [
                [5, 8],
                [12, 8],
                [20, 17],
                [40, 19],
                [64, 20],
                [90, 12],
                [99, 4],
                [112, 40],
            ]
        )
        marks = np.full((len(xy), 3), 0.5)
        with patch(
            "outshine.vegetation.river_park_trees.tile_sites",
            return_value=[(xy, marks)],
        ):
            source = SimpleNamespace(lat=48, lon=16, metres=np.array([74_000, 111_000]))
            sites, placements = verge_tree_sites(source, domain, 12, [])
        np.testing.assert_array_equal(sites, xy[:2])
        np.testing.assert_allclose(placements[:, 2], 1.095)

    def test_lake_and_elevated_path_do_not_create_river_verges(self):
        lake = ("water", self.river, {"class": "lake"})
        river = ("water", self.river, {"class": "river"})
        bridge = ("transportation", self.path, self.walk[2] | {"brunnel": "bridge"})
        self.assertTrue(
            riverside_verges((self.park, self.walk, lake), self.extent).is_empty
        )
        self.assertTrue(
            riverside_verges((self.park, bridge, river), self.extent).is_empty
        )
