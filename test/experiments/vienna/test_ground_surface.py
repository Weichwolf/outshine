import unittest

import numpy as np
import shapely
from ground_surface import at_grade_routes, partition_ground
from shapely.geometry import LineString, Point, Polygon, box


class GroundSurfaceTest(unittest.TestCase):
    def test_mapped_footway_survives_inferred_road_width(self):
        road = LineString([(0, 0), (20, 0)])
        footway = LineString([(0, 2), (20, 2)])
        deck = LineString([(0, 8), (20, 8)])
        surfaces = at_grade_routes(
            [
                (road, {"class": "minor"}),
                (footway, {"class": "path"}),
                (deck, {"class": "primary", "brunnel": "bridge", "layer": 1}),
            ],
            box(0, -5, 20, 15),
        )
        self.assertTrue(surfaces["sidewalk"].covers(Point(10, 2)))
        self.assertFalse(surfaces["road"].covers(Point(10, 2)))
        self.assertTrue(surfaces["road"].covers(Point(10, 0)))
        self.assertFalse(surfaces["road"].covers(Point(10, 8)))

    def test_stone_surface_only_on_the_tagged_path(self):
        paved = LineString([(0, 0), (20, 0)])
        forest = LineString([(0, 3), (20, 3)])
        products = at_grade_routes(
            [
                (paved, {"class": "path", "surface": "sett"}),
                (forest, {"class": "path", "surface": "compacted"}),
            ],
            box(0, -5, 20, 8),
        )
        self.assertTrue(products["paving"].covers(Point(10, 0)))
        self.assertFalse(products["paving"].covers(Point(10, 3)))
        self.assertTrue(products["unpaved path"].covers(Point(10, 3)))

    def test_pedestrian_area_takes_precedence_over_an_unpaved_path(self):
        trail = LineString([(0, 0), (20, 0)])
        square = box(8, -2, 12, 2)
        products = at_grade_routes(
            [
                (trail, {"class": "path", "subclass": "path"}),
                (square, {"class": "path", "subclass": "pedestrian"}),
            ],
            box(0, -5, 20, 5),
        )
        self.assertTrue(products["unpaved path"].covers(Point(6, 0)))
        self.assertFalse(products["unpaved path"].covers(Point(10, 0)))
        self.assertTrue(products["paving"].covers(Point(10, 0)))

    def test_path_touching_tile_boundary_contributes_no_surface(self):
        sidewalk = box(2, 2, 8, 3)
        touching = box(10, 5, 12, 6)
        tagged_sidewalk = {"class": "path", "subclass": "footway"}
        products = at_grade_routes(
            [(sidewalk, tagged_sidewalk), (touching, tagged_sidewalk)],
            box(0, 0, 10, 10),
        )
        self.assertTrue(products["sidewalk"].equals(sidewalk))
        self.assertTrue(products["unpaved path"].is_empty)
        self.assertTrue(products["road"].is_empty)

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
