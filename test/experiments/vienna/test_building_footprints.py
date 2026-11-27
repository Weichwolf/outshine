import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from building_footprints import building_footprints
from regional_detail import building_shells
from shapely.geometry import mapping


class BuildingFootprintTest(unittest.TestCase):
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
