import unittest
from types import SimpleNamespace

import numpy as np
from bridges import WaterCrossing
from outshine_ref.transport.water_span import WaterSpan
from reference_scene import support_height
from scene_mesh import assemble
from shapely.geometry import LineString, box


class DeckWalkLinkTest(unittest.TestCase):
    def test_mapped_stair_approach_reaches_the_bridge_through_its_guard(self):
        crossing = WaterCrossing(
            box(-20, -5, 20, 5),
            WaterSpan(np.array([-20, 0]), np.array([1, 0]), 40, 1.10, 0),
            box(-20, -3, 20, 3),
            1,
        )
        source = SimpleNamespace(
            height=lambda xy: np.where(
                abs(xy[:, 0]) < 12, -2, 0.63 - np.maximum(xy[:, 1] - 13, 0) * 0.10
            ),
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000.0, 111000.0]),
            features=[
                ("water", box(-12, -150, 12, 150), {}),
                (
                    "transportation",
                    LineString([(-20, 0), (20, 0)]),
                    {"class": "secondary", "brunnel": "bridge", "layer": 1},
                ),
                (
                    "transportation",
                    LineString([(-120, 0), (-20, 0)]),
                    {"class": "secondary"},
                ),
                (
                    "transportation",
                    LineString([(20, 0), (120, 0)]),
                    {"class": "secondary"},
                ),
                (
                    "transportation",
                    LineString([(18, 22), (18, 13)]),
                    {"class": "path", "subclass": "steps", "surface": "paved"},
                ),
                (
                    "transportation",
                    LineString([(18, 13), (18, 4.5)]),
                    {"class": "path", "subclass": "footway"},
                ),
                (
                    "transportation",
                    LineString([(18, 4.5), (-18, 4.5)]),
                    {
                        "class": "path",
                        "subclass": "footway",
                        "brunnel": "bridge",
                        "layer": 1,
                    },
                ),
            ],
        )
        mesh = assemble(source, 150, crossings=[crossing])
        stair = support_height(mesh["paving"], (18, 13)).max()
        pavement = [
            support_height(mesh["sidewalk"], (18, y)).max()
            for y in np.linspace(5, 13, 81)
        ]
        self.assertAlmostEqual(pavement[0], 1.1, places=5)
        self.assertAlmostEqual(pavement[-1], stair, places=5)
        self.assertLessEqual(np.max(abs(np.diff(pavement))) / 0.1, 0.08001)
        self.assertEqual(len(support_height(mesh["bridge metal"], (18, 4.92))), 0)
        self.assertGreater(len(support_height(mesh["bridge metal"], (14, 4.92))), 0)
