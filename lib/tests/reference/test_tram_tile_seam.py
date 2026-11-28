import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.places.sources import tile_geography
from outshine.terrain.lattice import lattice, terrain_height
from outshine.world.road_terrain import RoadTerrain, TerrainTile, _geography


class TramTileSeamTest(unittest.TestCase):
    def test_fits_a_shared_rail_bed_across_a_terrain_tile_boundary(self):
        parent = (14, 8192, 8191)
        left = (16, 4 * parent[1] + 1, 4 * parent[2] + 2)
        right = (16, left[1] + 1, left[2])
        tags = {"class": "transit", "subclass": "tram"}
        course = [(1988, 2060), (2108, 2060)]
        feature = {
            "id": 1,
            "properties": tags,
            "geometry": {"type": "LineString", "coordinates": course},
        }
        layer = {"extent": 4096, "features": [feature]}
        lat, lon = tile_geography(course, 4096, parent)

        def features(receipt, names, data, *, project):
            yield "transportation", shapely.LineString(project(lon, lat)), tags

        terrain = RoadTerrain(
            SimpleNamespace(metric_features=features),
            {parent: {"transportation": layer}},
        )
        project = terrain._parent(parent)[0]

        def grade(key):
            longitude, latitude = _geography(key)
            xy = project(longitude, latitude)
            height = np.broadcast_to(xy[:, 1, None] * 0.12, (len(xy), len(xy)))
            return TerrainTile(longitude, latitude, height)

        terrain._surface_grades = grade
        west, east = grade(left), grade(right)
        heights_east = terrain._tram_seam_heights(right, east.height)
        heights_west = terrain._tram_seam_heights(left, west.height)
        np.testing.assert_array_equal(heights_west[:, -1], heights_east[:, 0])
        self.assertTrue(np.any(heights_east[:, 0] != east.height[:, 0]))
        np.testing.assert_array_equal(heights_west[:, 0], west.height[:, 0])

        longitude = np.r_[west.longitude[:-1], east.longitude]
        xy = project(longitude, np.full_like(longitude, west.latitude[0]))
        north = project(west.longitude, west.latitude)[:, 1]
        heights = np.hstack((heights_west[:, :-1], heights_east))
        terrain_vertices, _ = lattice(xy[:, 0], north, heights)
        centre = project(*tile_geography([(2048, 2060)], 4096, parent)[::-1])[0]
        rail_points = centre + [[0, -0.7535], [0, 0.7535]]
        self.assertLessEqual(
            abs(np.diff(terrain_height(terrain_vertices, rail_points)))[0], 0.07501
        )
