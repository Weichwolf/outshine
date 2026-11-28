"""A walk follows mapped centreline and must stay on solved ground between frames."""

import unittest
from types import SimpleNamespace

import numpy as np
from shapely.geometry import LineString
from walk_reference import plan_walk


def fixture(mesh):
    path = LineString([(0, 0), (7, 0)])
    source = SimpleNamespace(features=[("transportation", path, {"class": "path"})])
    return SimpleNamespace(
        source=source,
        eye=np.array([1.0, 0.0, 2.0]),
        support="unpaved path",
        local={"unpaved path": mesh},
    )


def strip(start, end):
    return [[x, y, 0.25 * x] for x, y in ((start, -1), (end, -1), (end, 1), (start, 1))]


class WalkReferenceTest(unittest.TestCase):
    def test_samples_height_on_travel_from_the_mapped_station(self):
        world = fixture((strip(0, 7), [[0, 1, 2], [0, 2, 3]]))
        profile = {"station_offsets_m": [3, 0], "step_m": 0.6}
        walk = plan_walk(world, {"eye_height_m": 1.7}, profile)
        np.testing.assert_allclose(walk.eyes[:, 0], np.linspace(4, 1, 6))
        np.testing.assert_allclose(walk.eyes[:, 2], 1.7 + 0.25 * walk.eyes[:, 0])
        self.assertEqual(len(walk.foot_heights), 31)

    def test_detects_a_support_gap_between_frame_and_height_samples(self):
        vertices = strip(0, 2.019) + strip(2.031, 7)
        world = fixture((vertices, [[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]]))
        profile = {"station_offsets_m": [0, 3], "step_m": 0.6}
        with self.assertRaisesRegex(ValueError, "gap in the solved support mesh"):
            plan_walk(world, {"eye_height_m": 1.7}, profile)
