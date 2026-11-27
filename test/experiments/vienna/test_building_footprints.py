import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from building_footprints import building_footprints
from outshine_ref.places.camera import axes, ecef
from regional_detail import building_shells
from shapely.geometry import mapping


class BuildingFootprintTest(unittest.TestCase):
    def test_skyline_keeps_the_tall_footprint_in_the_terrain_enu_frame(self):
        source = SimpleNamespace(
            lon=16.24, lat=48.18, datum=400, metres=np.array([74000.0, 111200.0])
        )
        dem = SimpleNamespace(
            sample_with_zoom=lambda lat, lon, **kw: (np.full(len(lat), 170.0), None)
        )
        tile = {
            "building": {
                "extent": 100,
                "features": [
                    {
                        "geometry": mapping(shapely.box(*bounds)),
                        "properties": {"render_height": height},
                    }
                    for bounds, height in (
                        ((10, 20, 20, 30), 80),
                        ((30, 20, 40, 30), 12),
                    )
                ],
            }
        }
        nw, se = np.array([12000.0, 600.0]), np.array([13000.0, -400.0])
        roof, wall = building_shells(tile, nw, se, source, dem, 250, 700, distant=True)
        self.assertEqual(
            (roof[0], len(roof[2]), wall[0], len(wall[2])),
            ("regional roofs", 2, "regional walls", 4),
        )
        xy = nw + np.array([[10, 20], [20, 20], [10, 30], [20, 30]]) / 100 * (se - nw)
        lon, lat = (xy / source.metres + [source.lon, source.lat]).T
        expected = (
            ecef(lat, lon, 250.0) - ecef(source.lat, source.lon, source.datum)
        ) @ axes(source.lat, source.lon).T
        np.testing.assert_allclose(
            np.unique(roof[1], axis=0), np.unique(expected, axis=0)
        )

    def test_touching_buildings_retain_their_individual_boundaries(self):
        a = shapely.box(0, 0, 4, 4)
        b = shapely.box(4, 0, 8, 4)
        grouped = shapely.MultiPolygon([a, b])
        footprints, owners = building_footprints([grouped, shapely.box(9, 0, 12, 4)])
        self.assertEqual(owners.tolist(), [0, 0, 1])
        self.assertTrue(footprints[0].equals(a))
        self.assertTrue(footprints[1].equals(b))

    def test_distant_members_of_a_tile_feature_set_their_own_base_elevation(self):
        footprint = shapely.MultiPolygon(
            [
                shapely.box(10, 20, 20, 30),
                shapely.box(70, 20, 80, 30),
                shapely.box(95, 20, 140, 30),
                shapely.box(170, 20, 180, 30),
            ]
        )
        data = {
            "building": {
                "extent": 100,
                "features": [
                    {
                        "geometry": mapping(footprint),
                        "properties": {"render_height": 5, "render_min_height": 0},
                    }
                ],
            }
        }
        source = SimpleNamespace(lon=0, lat=0, datum=0, metres=np.ones(2))

        def sample(lat, lon):
            self.assertTrue((lon < 100).all())
            return lon * 0.2

        def sample_with_zoom(lat, lon, *, allow_missing):
            self.assertTrue(allow_missing)
            self.assertTrue((lon < 140).all())
            return np.where(lon < 100, lon * 0.2, np.nan), np.where(lon < 100, 12, -1)

        dem = SimpleNamespace(sample=sample, sample_with_zoom=sample_with_zoom)
        roofs, walls, *_ = building_shells(
            data, np.array([0, 100]), np.array([100, 0]), source, dem, 1, 600
        )
        for vertices, elevation in ((roofs[1], 8), (walls[1], 3)):
            near = vertices[vertices[:, 0] < 40, 2]
            far = vertices[vertices[:, 0] > 60, 2]
            clipped = vertices[vertices[:, 0] >= 95, 2]
            self.assertAlmostEqual(near.min(), elevation, delta=0.002)
            self.assertAlmostEqual(far.min(), elevation + 12, delta=0.002)
            self.assertAlmostEqual(clipped.min(), elevation + 16.5, delta=0.002)
