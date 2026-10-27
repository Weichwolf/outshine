"""Analytic surface controls independent of the native mesh samples (#964)."""

import copy
import unittest
from itertools import pairwise

import numpy as np
from road_surface_checks import observed_error
from road_surface_lod import envelope, prepare, quad_bound
from road_surface_model import Curve


def recipe(curvature=0.0, height=0.0, bank_rate=0.0, knots=2):
    def pose(s):
        east = np.sin(curvature * s) / curvature if curvature else s
        north = (1 - np.cos(curvature * s)) / curvature if curvature else 0.0
        return [
            s,
            east,
            north,
            height * s * s,
            curvature * s,
            curvature,
            0.0,
            2 * height * s,
            2 * height,
            bank_rate * s,
            bank_rate,
        ]

    stations = np.linspace(0.0, 10.0, knots)
    return {
        "range_m": [0.0, 10.0],
        "section_m": [2.0, 0.0, 0.2],
        "seams": [pose(s) for s in stations],
        "intervals": [
            {
                "range_m": [a, b],
                "middle": pose((a + b) / 2),
                "max_abs_curvature_per_m": abs(curvature),
            }
            for a, b in pairwise(stations)
        ],
    }


class RoadSurfaceTests(unittest.TestCase):
    def test_tiny_spiral_uses_its_recorded_midpoint_station(self):
        original = recipe()
        start, end = 100.1, 100.1 + 3e-6
        middle = (start + end) / 2
        self.assertNotEqual(middle - start, (end - start) / 2)
        original["range_m"] = [start, end]
        original["intervals"][0]["range_m"] = [start, end]
        for pose, station in zip(
            (*original["seams"], original["intervals"][0]["middle"]),
            (start, end, middle),
            strict=True,
        ):
            t = station - start
            pose[:3] = [station, t, 0.3 * t * t / 2 - 1e5 * t**3 / 6]
            pose[4:7] = [0.3 * t - 1e5 * t * t / 2, 0.3 - 1e5 * t, -1e5]
        original["intervals"][0]["max_abs_curvature_per_m"] = 0.3
        self.assertAlmostEqual(Curve(original).curvature_start[0], 0.3, places=14)

    def test_float32_oracle_preserves_the_actual_stored_endpoint(self):
        original = recipe()
        for pose in (*original["seams"], original["intervals"][0]["middle"]):
            pose[1] += 0.3
        curve = Curve(original)
        stations = np.array([0.0, 10.0])
        vertices = curve.at(stations).astype(np.float32)
        expected = float(np.linalg.norm(curve.at(stations) - vertices, axis=2).max())
        actual, _, _ = observed_error(curve, stations, vertices)
        self.assertAlmostEqual(actual, expected, places=12)

    def test_straight_profile_seams_do_not_force_render_stations(self):
        source = recipe(knots=101)
        self.assertNotIn("upper_positions_m", source)
        plan = prepare(Curve(source), 1e-6)
        np.testing.assert_array_equal(plan["stations_m"], [0, 10])
        self.assertEqual(len(plan["indices"]), 2)
        self.assertEqual(plan["positions_enh_m"].dtype, np.float32)
        triangles = plan["positions_enh_m"].reshape(-1, 3)[plan["indices"]]
        self.assertTrue(
            np.all(
                np.cross(
                    triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
                )[:, 2]
                > 0
            )
        )

    def test_parabolic_height_reaches_longitudinal_bound(self):
        curve = Curve(recipe(height=0.1))
        bound, ends, _ = envelope(curve, 0, 10)
        midpoint = curve.at(np.array([5.0]))[0]
        error = np.linalg.norm(midpoint - ends.mean(axis=0), axis=1).max()
        self.assertAlmostEqual(bound, 2.5)
        self.assertAlmostEqual(error, bound)
        plan = prepare(curve, 0.25)
        self.assertGreater(len(plan["stations_m"]), 2)
        self.assertLessEqual(plan["maximum_stored_bound_m"], 0.25)

    def test_circular_surface_matches_closed_form(self):
        curve = Curve(recipe(curvature=0.1))
        points = curve.at(np.array([5.0]))[0]
        center = np.array([np.sin(0.5) / 0.1, (1 - np.cos(0.5)) / 0.1, 0])
        normal = np.array([-np.sin(0.5), np.cos(0.5), 0])
        np.testing.assert_allclose(
            points, center + curve.across[:, None] * normal, atol=3e-14, rtol=0
        )

    def test_ruled_twist_defeats_longitudinal_only_bound(self):
        length, across_delta, rate = 4.0, 6.0, 0.1
        actual_midpoint_height = rate * length * across_delta / 4
        triangle_midpoint_height = 0.0
        error = abs(actual_midpoint_height - triangle_midpoint_height)
        self.assertEqual(quad_bound(length, 0.0, across_delta, 0.0), 0.0)
        self.assertAlmostEqual(
            quad_bound(length, 0.0, across_delta, rate * length), error
        )

    def test_banked_straight_road_refines_despite_zero_axis_curvature(self):
        curve = Curve(recipe(bank_rate=0.01))
        plan = prepare(curve, 0.02)
        self.assertGreater(len(plan["stations_m"]), 2)
        self.assertLessEqual(plan["maximum_stored_bound_m"], 0.02)

    def test_capacity_does_not_return_an_uncertified_partial_strip(self):
        with self.assertRaisesRegex(RuntimeError, "capacity"):
            prepare(Curve(recipe(curvature=0.1)), 1e-4, max_stations=2)
        for error in (0.0, -1.0, float("nan")):
            with self.subTest(error=error), self.assertRaises(ValueError):
                prepare(Curve(recipe()), error)

    def test_source_malformed_spans_and_bank_poles_are_refused(self):
        original = recipe()
        malformed = copy.deepcopy(original)
        malformed["intervals"][0]["range_m"][1] = 9.0
        with self.assertRaisesRegex(ValueError, "same seams"):
            Curve(malformed)
        with self.assertRaisesRegex(ValueError, "pole"):
            Curve(recipe(bank_rate=0.2))
        malformed = recipe(curvature=0.1)
        malformed["intervals"][0]["max_abs_curvature_per_m"] = 0
        with self.assertRaisesRegex(ValueError, "curvature envelope"):
            Curve(malformed)


if __name__ == "__main__":
    unittest.main()
