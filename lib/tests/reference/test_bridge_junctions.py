import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.transport.bridges import (
    WaterCrossing,
    guard_segments,
    plan_water_crossings,
)
from outshine.transport.rail_hardware import RailRecipe
from outshine.transport.tram_rails import RAIL_HEAD_RISE_M, RAIL_HEAD_WIDTH_M
from outshine.transport.water_span import WaterSpan
from outshine.world.mesh import assemble
from outshine.world.scene import support_height
from shapely.geometry import LineString, MultiLineString, box


class BridgeJunctionTest(unittest.TestCase):
    def test_wet_transverse_deck_edge_keeps_the_mapped_gate_open(self):
        deck = shapely.orient_polygons(box(-10, -4, 10, 4))
        water = box(-1, -7, 17, 7)
        entrance = box(9, -0.9, 11, 0.9)
        guards = shapely.union_all(
            list(guard_segments(deck, (1, 0), [entrance], water))
        )
        self.assertAlmostEqual(
            guards.intersection(LineString([(10, -4), (10, 4)])).length, 6.2, places=4
        )
        self.assertTrue(guards.intersection(entrance).is_empty)
        self.assertEqual(
            guards.intersection(LineString([(-10, -4), (-10, 4)])).length, 0
        )
        lane = box(-10, -2.5, 10, 2.5)
        road_guards = shapely.union_all(
            list(guard_segments(deck, (1, 0), [entrance], water, carriageway=lane))
        )
        self.assertTrue(road_guards.intersection(lane).is_empty)

    def test_pedestrian_polygons_keep_their_deck_without_using_a_lower_plaza(self):
        deck = box(-10, -4, 10, 4)
        path = LineString([(-10, 0), (10, 0)])
        paved = box(-10, -4, 10, -1)
        lower = box(-15, -1, 15, 4)
        tags = {"class": "path", "subclass": "pedestrian"}
        features = [
            (
                "transportation",
                deck,
                {"class": "bridge", "brunnel": "bridge", "layer": 1},
            ),
            ("transportation", path, tags | {"brunnel": "bridge", "layer": 1}),
            ("transportation", paved, tags),
            ("transportation", lower, tags | {"layer": 0}),
        ]
        (crossing,) = plan_water_crossings(
            features,
            box(-20, -20, 20, 20),
            box(-8, -20, 8, 20),
            lambda xy: np.zeros(len(xy)),
        )
        self.assertTrue(crossing.pedestrian_paving.equals(paved))
        self.assertTrue(crossing.pavement.is_empty)

    def test_unfitted_lower_walk_does_not_cover_the_upper_carriageway(self):
        deck = box(-20, -5, 20, 5)
        road = box(-20, -3, 20, 3)
        span = WaterSpan(np.array([-20, 0]), np.array([1, 0]), 40, 1, 0)
        source = SimpleNamespace(
            height=lambda xy: np.ones(len(xy)),
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000.0, 111000.0]),
            features=[
                ("water", box(-16, -10, 16, 10), {}),
                (
                    "transportation",
                    LineString([(0, -25), (0, 25)]),
                    {"class": "path", "brunnel": "bridge", "layer": 1},
                ),
            ],
        )
        meshes = assemble(source, 40, crossings=[WaterCrossing(deck, span, road, 2)])
        self.assertAlmostEqual(support_height(meshes["road"], (0, 0)).max(), 1)
        self.assertEqual(support_height(meshes["sidewalk"], (0, 0)).size, 0)
        self.assertGreater(support_height(meshes["sidewalk"], (0, 15)).size, 0)

    def test_mapped_transport_crosses_a_short_gap_between_deck_and_bank(self):
        deck = box(-20, -5, 20, 5)
        span = WaterSpan(np.array([-20, 0]), np.array([1, 0]), 40, 1.2, 0)
        line = LineString([(-20.4, 0), (20.3, 0)])
        unrelated = LineString([(65, 35), (130, 35)])
        roles = (
            (
                {"class": "path", "subclass": "pedestrian", "surface": "paved"},
                0.9,
                "bridge deck",
                "paving",
            ),
            ({"class": "secondary"}, 3.25, "road", "road"),
        )
        for tags, half_width, deck_material, approach_material in roles:
            with self.subTest(kind=tags["class"]):
                pavement = (
                    deck.difference(deck)
                    if tags["class"] == "path"
                    else line.buffer(half_width, cap_style="flat").intersection(deck)
                )
                crossing = WaterCrossing(deck, span, pavement, 1)
                source = SimpleNamespace(
                    height=lambda xy: np.where(abs(xy[:, 0]) < 12, -2, 0.4),
                    datum=0,
                    lat=0,
                    lon=0,
                    metres=np.array([111000.0, 111000.0]),
                    features=[
                        ("water", box(-12, -150, 20.25, 150), {}),
                        (
                            "transportation",
                            MultiLineString([unrelated, line]),
                            tags | {"brunnel": "bridge", "layer": 1},
                        ),
                        (
                            "transportation",
                            LineString([(-120, 0), (-20.4, 0)]),
                            tags,
                        ),
                        ("transportation", LineString([(20.3, 0), (120, 0)]), tags),
                    ],
                )
                mesh = assemble(source, 150, crossings=[crossing])
                width = half_width - 0.1
                for edge, sign in ((-20.0, -1), (20.0 + half_width, 1)):
                    for y in np.linspace(-width, width, 9):
                        deck_top = support_height(
                            mesh[deck_material], (edge - sign * 0.02, y)
                        ).max()
                        xy = edge + sign * 0.02, y
                        bank = support_height(mesh[approach_material], xy)
                        self.assertLess(abs(deck_top - bank.max()), 0.005)
                        if deck_material == "road":
                            self.assertLess(np.ptp(bank), 0.005)
                fallback = "sidewalk" if tags["class"] == "path" else "road"
                far = support_height(mesh[fallback], (80, 35))
                self.assertGreater(far.size, 0)
                rails, _ = mesh["bridge metal"]
                self.assertLess(np.asarray(rails)[:, 0].max(), 20.05)

    def test_tagged_road_fragment_uses_the_fitted_bank_surface(self):
        deck = box(-20, -5, 20, 5)
        carriageway = LineString([(-20.4, 0), (20.3, 0)])
        span = WaterSpan(np.array([-20, 0]), np.array([1, 0]), 40, 1.2, 0.01)
        crossing = WaterCrossing(
            deck, span, carriageway.buffer(2).intersection(deck), 1
        )
        source = SimpleNamespace(
            height=lambda xy: 1.60 + 0.012 * xy[:, 0] + 0.002 * xy[:, 1],
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000.0, 111000.0]),
            features=[
                ("water", box(-16, -150, 16, 150), {}),
                (
                    "transportation",
                    carriageway,
                    {"class": "secondary", "brunnel": "bridge", "layer": 1},
                ),
                (
                    "transportation",
                    LineString([(-20.4, 0), (-130, 0)]),
                    {"class": "secondary"},
                ),
                (
                    "transportation",
                    LineString([(20.3, 0), (130, 0)]),
                    {"class": "secondary"},
                ),
                (
                    "transportation",
                    LineString([(-20, -1), (20, -1)]),
                    {
                        "class": "transit",
                        "subclass": "tram",
                        "brunnel": "bridge",
                        "layer": 1,
                    },
                ),
            ],
        )
        for points in ([(-130, -1), (-20, -1)], [(20, -1), (130, -1)]):
            source.features.append(
                (
                    "transportation",
                    LineString(points),
                    {"class": "transit", "subclass": "tram"},
                )
            )
        mesh = assemble(source, 150, crossings=[crossing])
        vertices, faces = mesh["bridge deck"]
        triangles = np.asarray(vertices)[faces]
        for x in (-20.0, 20.0):
            left = support_height(mesh["road"], (x - 0.02, 0)).max()
            right = support_height(mesh["road"], (x + 0.02, 0)).max()
            self.assertLess(abs(left - right), 0.005)
            self.assertAlmostEqual(
                (left + right) / 2, span.height(np.array([x, 0])), places=2
            )
            mouth = triangles[np.isclose(triangles[:, :, 0], x, atol=1e-6).all(axis=1)]
            self.assertGreater(len(mouth), 0)
            self.assertLessEqual(
                mouth[:, :, 2].max(), span.height(np.array([x, 0])) + 1e-6
            )
            centre = (RailRecipe().gauge_m + RAIL_HEAD_WIDTH_M) / 2
            for y in (-1 - centre, -1 + centre):
                for sample in (x - 0.02, x + 0.02):
                    xy = (sample, y)
                    rail = support_height(mesh["rail head"], xy).max()
                    road = support_height(mesh["road"], xy).max()
                    self.assertAlmostEqual(rail - road, RAIL_HEAD_RISE_M, places=5)
