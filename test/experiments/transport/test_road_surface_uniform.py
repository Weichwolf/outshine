"""Same-quality uniform controls and complete original adaptive byte pins."""

import unittest
from itertools import pairwise

import numpy as np
from road_surface_checks import observed_error
from road_surface_lod import envelope, prepare, prepare_uniform
from road_surface_model import Curve
from test_road_surface_lod import recipe


class UniformRoadSurface(unittest.TestCase):
    def test_straight_seams_do_not_force_stations_and_outputs_match(self):
        source = recipe(knots=101)
        a = prepare(Curve(source), 1e-6)
        b = prepare_uniform(Curve(source), 1e-6)
        self.assertEqual(b["whole_grid_levels"], 1)
        for name in ("stations_m", "positions_enh_m", "indices"):
            self.assertEqual(a[name].dtype, b[name].dtype)
            self.assertEqual(a[name].tobytes(), b[name].tobytes())

    def test_same_bound_and_observed_surface_error_for_curved_banked_profiles(self):
        for source in (
            recipe(curvature=0.1),
            recipe(height=0.1),
            recipe(bank_rate=0.05),
            recipe(curvature=0.03, height=0.02, bank_rate=0.04, knots=7),
        ):
            with self.subTest(source=source["seams"][-1]):
                curve = Curve(source)
                plan = prepare_uniform(curve, 0.05)
                stations, points = plan["stations_m"], plan["positions_enh_m"]
                self.assertLessEqual(plan["maximum_stored_bound_m"], 0.05)
                actual, _, _ = observed_error(curve, stations, points)
                self.assertLessEqual(actual, plan["maximum_stored_bound_m"] + 1e-12)
                np.testing.assert_allclose(
                    np.diff(stations), np.diff(stations)[0], atol=2e-15, rtol=0
                )
                self.assertEqual(points.dtype, np.float32)
                self.assertEqual(plan["indices"].dtype, np.uint32)
                count = len(stations) - 1
                self.assertEqual(count & (count - 1), 0)
                if count > 1:
                    coarse = stations[::2]
                    self.assertTrue(
                        any(
                            envelope(curve, a, b)[0]
                            + np.linalg.norm(p - p.astype(np.float32), axis=2).max()
                            > 0.05
                            for a, b in pairwise(coarse)
                            for p in [curve.at(np.array([a, b]))]
                        )
                    )

    def test_same_active_cross_section_bands_and_original_endpoints(self):
        source = recipe(height=0.1)
        source["section_m"][1] = 0.5
        curve = Curve(source)
        plan = prepare_uniform(curve, 0.1)
        self.assertEqual(len(plan["indices"]), (len(plan["stations_m"]) - 1) * 6)
        self.assertEqual(
            plan["positions_enh_m"][[0, -1]].tobytes(),
            curve.at(np.array([0, 10])).astype(np.float32).tobytes(),
        )

    def test_caps_invalid_error_and_unrepresentable_subdivision_reject(self):
        for error, capacity in (
            (0, 4),
            (float("nan"), 4),
            (0.1, 1),
            (0.1, True),
            (0.1, 262145),
        ):
            with (
                self.subTest(error=error, capacity=capacity),
                self.assertRaises(ValueError),
            ):
                prepare_uniform(Curve(recipe()), error, capacity)
        with self.assertRaisesRegex(RuntimeError, "capacity"):
            prepare_uniform(Curve(recipe(height=1)), 0.01, max_stations=2)


if __name__ == "__main__":
    unittest.main()
