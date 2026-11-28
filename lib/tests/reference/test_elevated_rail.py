import unittest
from functools import partial
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.terrain.ground_surface import route_area
from outshine.transport.elevated_rail import plan_elevated_rail
from outshine.transport.viaducts import SLAB_DEPTH
from outshine.world.mesh import assemble, terrain_grid, terrain_height
from outshine.world.scene import support_height


class ElevatedRailTest(unittest.TestCase):
    def test_parallel_tracks_share_dry_deck_and_clear_the_road(self):
        tracks = [shapely.LineString([(-15, y), (15, y)]) for y in (-3, 3)]
        road = shapely.LineString([(0, -20), (0, 20)])
        continuations = [
            (
                "transportation",
                shapely.LineString([(15, y), (32, y)]),
                {"class": "rail"},
            )
            for y in (-3, 3)
        ]
        source = SimpleNamespace(
            height=lambda xy: 0.01 * xy[:, 1],
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000, 111000]),
            features=[
                *(
                    ("transportation", line, {"class": "rail", "brunnel": "bridge"})
                    for line in tracks
                ),
                ("transportation", road, {"class": "minor"}),
                *continuations,
            ],
        )
        terrain, quads = terrain_grid(source, 40)
        empty = shapely.Polygon()
        [deck] = plan_elevated_rail(
            source.features,
            shapely.box(-40, -40, 40, 40),
            empty,
            [],
            empty,
            terrain,
            quads,
            partial(terrain_height, terrain),
        )
        self.assertEqual(len(deck.tracks), 2)
        self.assertAlmostEqual(deck.elevation, 5.2 * 0.01 + 4.8 + SLAB_DEPTH + 0.15)
        road_area = route_area(road, 5.5)
        self.assertEqual({np.sign(pier.centroid.x) for pier in deck.supports}, {-1, 1})
        self.assertTrue(all(not road_area.intersects(pier) for pier in deck.supports))

        meshes = assemble(source, 40)
        for station in np.linspace(-14.9, 14.9, 11):
            underside, top = sorted(
                set(support_height(meshes["bridge deck"], (station, 0)))
            )
            self.assertAlmostEqual(top, deck.elevation)
            self.assertAlmostEqual(top - underside, SLAB_DEPTH)
        self.assertAlmostEqual(support_height(meshes["road"], (0, 0)).max(), 0)
        self.assertGreater(len(meshes["rail head"][1]), 0)
        self.assertGreater(len(meshes["rail sleeper"][1]), 0)

    def test_dry_approach_descends_from_the_deck_at_bounded_grade(self):
        source = SimpleNamespace(
            height=lambda xy: np.zeros(len(xy)),
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000, 111000]),
            features=[
                (
                    "transportation",
                    shapely.LineString([(-15, 0), (15, 0)]),
                    {"class": "rail", "brunnel": "bridge"},
                ),
                (
                    "transportation",
                    shapely.LineString([(15, 0), (250, 0)]),
                    {"class": "rail"},
                ),
                (
                    "transportation",
                    shapely.LineString([(0, -12), (0, 12)]),
                    {"class": "minor"},
                ),
            ],
        )
        meshes = assemble(source, 260)
        bed = meshes["rail ballast"]
        stations = np.r_[14.9999, 15.0001, np.arange(16.0, 228, 3)]
        z = [support_height(bed, (x, 0)).max() for x in stations]
        self.assertAlmostEqual(z[0], z[1], places=5)
        self.assertLessEqual(np.max(abs(np.diff(z)) / np.diff(stations)), 0.030001)
        self.assertLess(z[-1], 0.24)
        self.assertEqual(support_height(meshes["ground"], (100, 0)).size, 0)

        source.features.append(
            (
                "transportation",
                shapely.LineString([(75, -20), (75, 20)]),
                {"class": "minor"},
            )
        )
        blocked = assemble(source, 260)
        self.assertEqual(support_height(blocked["rail ballast"], (100, 0)).size, 0)
        self.assertAlmostEqual(support_height(blocked["road"], (75, 0)).max(), 0)
