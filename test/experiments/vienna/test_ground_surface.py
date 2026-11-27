import unittest

import numpy as np
import shapely
from ground_surface import partition_ground
from shapely.geometry import Polygon, box


class GroundSurfaceTest(unittest.TestCase):
    def test_shoreline_and_road_partition_sloping_terrain_without_overlap(self):
        vertices = np.array(
            [[0, 0, 0], [10, 0, 1], [20, 0, 2], [0, 10, 2], [10, 10, 4], [20, 10, 6]]
        )
        quads = np.array([[0, 1, 4, 3], [1, 2, 5, 4]])
        water = Polygon([(0, 2), (20, 6), (20, 10), (0, 10)]).difference(
            box(11, 7, 14, 9)
        )
        road = box(3, 0, 5, 10).union(box(16, 0, 18, 10))
        products = partition_ground(
            vertices,
            quads,
            np.array([0, 1]),
            ("grass", "soil", "water", "road"),
            {2: water, 3: road},
        )
        expected = {
            "water": water,
            "road": road.difference(water),
            "grass": box(0, 0, 10, 10).difference(water.union(road)),
            "soil": box(10, 0, 20, 10).difference(water.union(road)),
        }
        areas = {name: [] for name in expected}
        for name, points, faces in products:
            areas[name].extend(shapely.polygons(points[faces][:, :, :2]))
            for x, y, z in points[faces].reshape(-1, 3):
                if x <= 10:
                    height = x / 10 + 0.3 * y if x >= y else 0.2 * (x + y)
                else:
                    height = x / 10 + 0.4 * y if x - 10 >= y else 0.2 * x + 0.3 * y - 1
                self.assertAlmostEqual(z, height)
        for name, parts in areas.items():
            self.assertAlmostEqual(shapely.area(parts).sum(), expected[name].area)
            self.assertAlmostEqual(
                shapely.union_all(parts).symmetric_difference(expected[name]).area, 0
            )
