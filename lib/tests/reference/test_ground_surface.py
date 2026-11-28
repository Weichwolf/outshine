import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.terrain.ground_surface import (
    MEADOW_MARGIN_M,
    TRAIL_SHOULDER_M,
    at_grade_routes,
    forest_trail_edges,
    margin_weights,
    partition_ground,
)
from outshine.terrain.land_cover import cover_material
from outshine.vegetation.woodland import woodland_surface
from outshine.water.water_surface import WaterBody
from outshine.world.mesh import assemble
from outshine.world.regional import REGIONAL_LAND, class_fields
from outshine.world.scene import support_height
from shapely.geometry import LineString, Point, Polygon, box


class GroundSurfaceTest(unittest.TestCase):
    def test_meadow_litter_follows_the_woodland_edge_through_a_path_cut(self):
        source = SimpleNamespace(
            height=lambda xy: 0.12 * xy[:, 0] + 0.15 * xy[:, 1],
            features=[
                ("landcover", box(-8, -8, 3, 8), {"class": "wood"}),
                (
                    "landcover",
                    box(0, -6, 8, 6),
                    {"class": "grass", "subclass": "meadow"},
                ),
                (
                    "transportation",
                    LineString([(-9, 0), (9, 0)]),
                    {"class": "path", "subclass": "path"},
                ),
            ],
        )
        meshes = assemble(source, 10)
        edge = meshes.meadow_woodland_edges
        self.assertFalse(edge.intersects(Point(0, 0)))
        positions = [(0, 3), (MEADOW_MARGIN_M / 2, 3), (4, 3), (8, 3)]
        np.testing.assert_allclose(
            margin_weights(edge, positions, MEADOW_MARGIN_M), [1, 0.5, 0, 0]
        )
        for x, y in positions[:-1]:
            self.assertAlmostEqual(
                support_height(meshes["meadow"], (x, y)).max(), 0.12 * x + 0.15 * y
            )
        self.assertEqual(len(support_height(meshes["meadow"], (1, 0))), 0)

    def test_meadow_without_a_woodland_contact_has_no_litter_edge(self):
        meadow = box(0, -6, 8, 6)
        for woodland in (meadow, box(-8, -6, -3, 6)):
            with self.subTest(woodland=woodland):
                source = SimpleNamespace(
                    height=lambda xy: np.zeros(len(xy)),
                    features=[
                        ("landcover", woodland, {"class": "wood"}),
                        (
                            "landcover",
                            meadow,
                            {"class": "grass", "subclass": "meadow"},
                        ),
                        (
                            "transportation",
                            LineString([(-9, 0), (9, 0)]),
                            {"class": "path", "subclass": "path"},
                        ),
                    ],
                )
                meshes = assemble(source, 10)
                self.assertIsNone(meshes.meadow_woodland_edges)
                self.assertEqual(support_height(meshes["meadow"], (1, 3)).max(), 0)

    def test_forest_floor_keeps_the_walk_clear_and_supports_plants(self):
        source = SimpleNamespace(
            height=lambda xy: 0.15 * xy[:, 0] + 0.2 * xy[:, 1],
            features=[
                ("landcover", box(-6, -6, 6, 6), {"class": "wood"}),
                (
                    "landcover",
                    box(2, 2, 5, 5),
                    {"class": "grass", "subclass": "meadow"},
                ),
                (
                    "transportation",
                    LineString([(-8, 0), (8, 0)]),
                    {"class": "path", "subclass": "path"},
                ),
            ],
        )
        meshes = assemble(source, 10)
        forest = meshes["forest floor"]
        self.assertAlmostEqual(support_height(forest, (-3, 4)).max(), 0.35)
        self.assertEqual(len(support_height(forest, (-3, 0))), 0)
        self.assertEqual(len(support_height(forest, (3, 3))), 0)
        self.assertAlmostEqual(
            support_height(meshes["unpaved path"], (-3, 0)).max(), -0.45
        )
        height = woodland_surface(meshes).sample([[-3, 4], [8, 4]]).points_enu_m[:, 2]
        np.testing.assert_allclose(height, [0.35, 2.0])

    def test_woodland_shoulders_stop_at_open_land_and_paved_crossings(self):
        path = box(-6, -0.9, 6, 0.9)
        woods = box(-4, -0.9, 4, 3)
        paving = box(-0.3, -1.5, 0.3, 1.5)
        edge = forest_trail_edges(path.difference(paving), woods, paving)
        positions = [
            (-2, 0.9, 0),
            (-2, 0.9 - TRAIL_SHOULDER_M / 2, 0),
            (-2, -0.9, 0),
            (-5, 0.9, 0),
            (0.35, 0.9, 0),
        ]
        np.testing.assert_allclose(
            margin_weights(edge, positions, TRAIL_SHOULDER_M), [1, 0.5, 0, 0, 0]
        )

    def test_diagonal_trail_retains_both_edges_across_the_woodland_cut(self):
        centreline = LineString([(-5, -1), (4, 3)])
        path = centreline.buffer(0.9, cap_style="flat")
        woods = box(-2, -4, 2, 4)
        edge = forest_trail_edges(path, woods, Polygon())
        self.assertEqual(len(shapely.get_parts(edge)), 2)
        expected = 2 * 4 * centreline.length / 9
        self.assertAlmostEqual(edge.length, expected, delta=0.001)

    def test_grass_and_soil_follow_the_same_grade_at_the_waterline(self):
        water = WaterBody(box(-40, -40, 0, 40), 0.0)
        source = SimpleNamespace(
            height=lambda xy: np.full(len(xy), 3.0),
            lon=16.4,
            lat=48.2,
            metres=np.array([74300, 111180]),
            datum=160,
            features=[("landcover", box(-8, -20, 8, 0), {"class": "grass"})],
        )
        meshes = assemble(source, 24, water_plan=(water,))
        for material, north in (("grass", -4), ("ground", 4)):
            point = (0, north)
            self.assertAlmostEqual(
                support_height(meshes[material], point).max(),
                support_height(meshes["water"], point).max(),
                delta=0.001,
            )
        for east in (0.8, 2.5, 6.0):
            a = support_height(meshes["grass"], (east, 0)).max()
            b = support_height(meshes["ground"], (east, 0)).max()
            self.assertAlmostEqual(a, b, places=6)

    def test_regional_meadow_remains_distinct_under_wood_priority(self):
        def feature(geometry, **properties):
            return {
                "geometry": shapely.geometry.mapping(geometry),
                "properties": properties,
            }

        fields = {
            "landcover": {
                "extent": 16,
                "features": [
                    feature(
                        box(2, 0, 16, 16), **{"class": "grass", "subclass": "meadow"}
                    ),
                    feature(box(4, 0, 6, 16), **{"class": "wood"}),
                    feature(box(0, 0, 16, 16), **{"class": "grass"}),
                ],
            },
        }
        land, _ = class_fields(fields, 16, set(), False)
        self.assertEqual(
            [REGIONAL_LAND[land[8, x]] for x in (1, 3, 5)],
            ["grass", "meadow", "regional wood"],
        )

    def test_regional_field_retains_cover_inside_woodland_clearance(self):
        def feature(geometry, id, **tags):
            return {
                "geometry": shapely.geometry.mapping(geometry),
                "id": id,
                "properties": tags,
            }

        tags = {"class": "farmland"}
        crop = feature(box(0, 0, 16, 16), 6382, **tags)
        woodland = box(2, 2, 14, 14).difference(box(6, 5, 11, 12))
        data = {
            "landcover": {
                "extent": 16,
                "features": [feature(woodland, 12, **{"class": "wood"}), crop],
            },
            "landuse": {
                "extent": 16,
                "features": [
                    feature(box(0, 0, 16, 16), 24, **{"class": "residential"})
                ],
            },
        }
        land, _ = class_fields(data, 16, set(), False)
        field = cover_material("landcover", tags, crop["id"])
        self.assertEqual(
            [REGIONAL_LAND[land[8, x]] for x in (1, 4, 8)],
            [field, "regional wood", field],
        )

    def test_tagged_meadow_overrides_generic_grass_and_keeps_the_path(self):
        area = box(-8, -8, 8, 8)
        source = SimpleNamespace(
            height=lambda xy: 0.1 * xy[:, 0],
            features=[
                ("landcover", area, {"class": "grass"}),
                (
                    "landcover",
                    box(0, -8, 8, 8),
                    {"class": "grass", "subclass": "meadow"},
                ),
                (
                    "transportation",
                    LineString([(-8, 0), (8, 0)]),
                    {"class": "path", "subclass": "path"},
                ),
            ],
        )
        meshes = assemble(source, 12)
        self.assertAlmostEqual(support_height(meshes["meadow"], (5, 4)).max(), 0.5)
        self.assertEqual(len(support_height(meshes["grass"], (5, 4))), 0)
        self.assertAlmostEqual(support_height(meshes["grass"], (-5, 4)).max(), -0.5)
        self.assertEqual(len(support_height(meshes["meadow"], (5, 0))), 0)
        self.assertAlmostEqual(
            support_height(meshes["unpaved path"], (5, 0)).max(), 0.5
        )

    def test_mapped_grass_shares_the_slope_and_leaves_the_road_clear(self):
        source = SimpleNamespace(
            height=lambda xy: xy[:, 0] * 0.12 + xy[:, 1] * 0.08,
            features=[
                ("landcover", box(-6, -6, 6, 6), {"class": "grass"}),
                ("transportation", LineString([(-6, 0), (6, 0)]), {"class": "minor"}),
            ],
        )
        meshes = assemble(source, 12)
        self.assertAlmostEqual(support_height(meshes["grass"], (0, 4)).max(), 0.32)
        self.assertEqual(len(support_height(meshes["grass"], (0, 1))), 0)
        self.assertAlmostEqual(support_height(meshes["road"], (0, 1)).max(), 0.08)
        self.assertAlmostEqual(support_height(meshes["ground"], (0, 7)).max(), 0.56)

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

    def test_crossing_and_terminating_footways_keep_the_carriageway(self):
        road = LineString([(-8, 0), (8, 0)])
        crossing = LineString([(-2, -5), (0.8, 5)])
        terminating = LineString([(4, -5), (5, 0.3)])
        products = at_grade_routes(
            [
                (road, {"class": "service"}),
                (crossing, {"class": "path", "subclass": "footway"}),
                (terminating, {"class": "path", "subclass": "footway"}),
            ],
            box(-9, -6, 9, 6),
        )
        self.assertTrue(products["road"].covers(road))
        self.assertTrue(products["sidewalk"].covers(Point(-2, -4.8)))
        self.assertTrue(products["sidewalk"].covers(Point(0.7, 4.8)))
        self.assertTrue(products["sidewalk"].covers(Point(4.1, -4.8)))
        self.assertEqual(products["road"].intersection(products["sidewalk"]).area, 0)

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
