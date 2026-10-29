"""Original-node distance bounds are separate from whole-way influence."""

import math
import unittest
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from outshine_ref.transport.canonical_ports._geodesy import (
    A,
    positions_in_frame,
    surface_reach_bound,
)


def on_sphere(anchor, distance_m, bearings, radius_m):
    lon, lat = np.deg2rad(anchor)
    centre = np.array(
        [math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat)]
    )
    east = np.array([-math.sin(lon), math.cos(lon), 0])
    north = np.array(
        [-math.sin(lat) * math.cos(lon), -math.sin(lat) * math.sin(lon), math.cos(lat)]
    )
    directions = np.cos(bearings)[:, None] * north + np.sin(bearings)[:, None] * east
    angle = distance_m / radius_m
    target = math.cos(angle) * centre + math.sin(angle) * directions
    return np.rad2deg(
        np.c_[
            np.arctan2(target[:, 1], target[:, 0]),
            np.arctan2(target[:, 2], np.hypot(target[:, 0], target[:, 1])),
        ]
    )


class SurfaceReachTests(unittest.TestCase):
    def test_global_surface_metric_and_enu_projection(self):
        bearings = np.linspace(0, 2 * math.pi, 16, endpoint=False)
        anchors = (
            (0, 0),
            (179.999999, 0),
            (-179.999999, 65),
            (27, 89.999999),
            (-84, -89.999999),
            (0, 90),
            (0, -90),
        )
        for anchor in anchors:
            for distance in (0.01, 0.5, 2, 100, 100000):
                for radius in (A, 6371000):
                    with self.subTest(anchor=anchor, distance=distance, radius=radius):
                        target = on_sphere(anchor, distance, bearings, radius)
                        xyz = positions_in_frame(anchor, target, np.zeros(len(target)))
                        bound = surface_reach_bound(distance, radius)
                        self.assertTrue(np.all(np.linalg.norm(xyz, axis=1) <= bound))
                        self.assertTrue(
                            np.all(np.linalg.norm(xyz[:, :2], axis=1) <= bound)
                        )

    def test_two_spherical_metres_are_not_two_ellipsoidal_metres(self):
        anchor = (0, 89.999)
        target = on_sphere(anchor, 2, np.array([0.0]), A)
        distance = float(
            np.linalg.norm(positions_in_frame(anchor, target, np.zeros(1))[0])
        )
        self.assertGreater(distance, 2.006)
        self.assertLess(distance, surface_reach_bound(2, A))

    def test_unrepresentable_or_invalid_bounds_are_not_zeroed(self):
        for distance, radius in (
            (-1, A),
            (math.nan, A),
            (math.inf, A),
            (1, 0),
            (1, -1),
            (1, math.nan),
            (1, math.inf),
            (1e308, 1),
            (1e-300, 1e308),
        ):
            with (
                self.subTest(distance=distance, radius=radius),
                self.assertRaises(ValueError),
            ):
                surface_reach_bound(distance, radius)
        self.assertEqual(surface_reach_bound(0, A), 0)

    def test_actual_published_points_fit_the_current_producer_snap_bound(self):
        fixture = Path(__file__).parent / "fixtures/original-point-source-facts"
        source, groups, _ways, _binding, _work = load_bridge_source_facts(fixture)
        by_group = {int(g.identifier.rsplit(":", 1)[1]): g.position for g in groups}
        measured, unobserved = [], 0
        for missing in source.binding["missing_original_ways"]:
            for raw, point in zip(
                missing["Points"], missing["OriginalPoints"], strict=True
            ):
                if point["State"] == 1:
                    unobserved += 1
                    continue
                self.assertEqual(point["State"], 2)
                anchor = raw["LongitudeDeg"], raw["LatitudeDeg"]
                target = np.array([by_group[point["Group"]]])
                xyz = positions_in_frame(anchor, target, np.zeros(1))[0]
                measured.append(float(np.linalg.norm(xyz)))
        self.assertEqual((len(measured), unobserved), (74, 8))
        self.assertGreater(max(measured), 1.99)
        self.assertLessEqual(max(measured), surface_reach_bound(2, A))


if __name__ == "__main__":
    unittest.main()
