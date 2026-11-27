import unittest
from types import SimpleNamespace

import numpy as np
from bridge_passages import CLEARANCE_M, FLOOR_ABOVE_WATER_M, MAXIMUM_GRADE
from bridges import WaterCrossing
from outshine_ref.transport.water_span import WaterSpan
from reference_scene import support_height
from scene_mesh import assemble
from shapely.geometry import LineString, box


class BridgePassageTest(unittest.TestCase):
    def test_dry_footway_and_raised_road_have_separate_usable_heights(self):
        footprint = box(-20, -5, 20, 5)
        span = WaterSpan(np.array([-20, 0]), np.array([1, 0]), 40, 1.25, 0)
        crossing = WaterCrossing(footprint, span, box(-20, -3, 20, 3), 1)
        source = SimpleNamespace(
            height=lambda xy: np.where(
                abs(xy[:, 0]) < 16,
                -2,
                0.002 * xy[:, 1]
                + 0.035 * xy[:, 0]
                + 0.8 * np.exp(-(((xy[:, 1] - 57.4) / 6) ** 2)),
            ),
            datum=0,
            lat=0,
            lon=0,
            metres=np.array([111000.0, 111000.0]),
            features=[
                ("water", box(-16, -150, 16, 150), {}),
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
                    LineString([(18, -100), (18, 100)]),
                    {"class": "path", "subclass": "footway"},
                ),
                (
                    "transportation",
                    LineString([(17, -8), (19, -8)]),
                    {"class": "path", "brunnel": "bridge", "layer": 1},
                ),
            ],
        )
        mesh = assemble(source, 150, crossings=[crossing])
        for y in np.linspace(-4.99, 4.99, 21):
            xy = (18, y)
            foot = support_height(mesh["sidewalk"], xy).max()
            underside = support_height(mesh["bridge deck"], xy).min()
            self.assertGreaterEqual(underside - foot, CLEARANCE_M)
            self.assertGreaterEqual(foot, -2 + FLOOR_ABOVE_WATER_M - 1e-6)
        for y in (-2.5, 0, 2.5):
            contact = (17.1, y)
            bank = support_height(mesh["ground"], contact).max()
            path = support_height(mesh["sidewalk"], contact).max()
            self.assertAlmostEqual(bank, path, places=5)
        sidewalk = support_height(mesh["sidewalk"], (18, -8))
        self.assertGreater(len(sidewalk), 0)
        self.assertLessEqual(np.ptp(sidewalk), 1e-6)
        stations = np.linspace(-59.9, 59.9, 601)
        for x in (17.2, 18, 18.8):
            z = [support_height(mesh["sidewalk"], (x, y)).max() for y in stations]
            self.assertLessEqual(
                np.max(abs(np.diff(z)) / np.diff(stations)), MAXIMUM_GRADE + 1e-5
            )
        for y in (-60, 60):
            for x in np.linspace(17.12, 18.88, 21):
                heights = support_height(mesh["sidewalk"], (x, y))
                self.assertGreater(len(heights), 1)
                self.assertAlmostEqual(np.ptp(heights), 0, places=5)
        for x in (-20, 20):
            left = support_height(mesh["road"], (x - 0.02, 0)).max()
            right = support_height(mesh["road"], (x + 0.02, 0)).max()
            self.assertLess(abs(left - right), 0.005)
