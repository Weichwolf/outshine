"""Stable common functions, independent geometry and bounded fallbacks (#1542)."""

import unittest
from unittest.mock import patch

import numpy as np
from road_bank_profile import BankLimits, prior_controls
from road_joint_profile import plan_profile
from road_shared_profile import (
    ProfileRecipe,
    SharedCubicSurface,
    restriction,
    shared_candidate,
)
from road_surface_checks import observed_error
from road_surface_frames import differentials
from road_surface_lod import prepare
from road_surface_model import Curve
from test_road_bank_profile import bump
from test_road_joint_profile import height_bump
from test_road_surface_lod import recipe


def microscopic_source():
    source = recipe(curvature=0.03, height=0.001, bank_rate=0.003, knots=4)
    stations = [0.0, 5.0, 5.0 + 5e-12, 10.0]
    for pose, s in zip(source["seams"], stations, strict=True):
        pose[:5] = [
            s,
            np.sin(0.03 * s) / 0.03,
            (1 - np.cos(0.03 * s)) / 0.03,
            0.001 * s * s,
            0.03 * s,
        ]
        pose[7], pose[9] = 0.002 * s, 0.003 * s
    for interval, a, b in zip(
        source["intervals"], stations[:-1], stations[1:], strict=True
    ):
        interval["range_m"] = [a, b]
        middle = (a + b) / 2
        interval["middle"] = [
            middle,
            np.sin(0.03 * middle) / 0.03,
            (1 - np.cos(0.03 * middle)) / 0.03,
            0.001 * middle**2,
            0.03 * middle,
            0.03,
            0.0,
            0.002 * middle,
            0.002,
            0.003 * middle,
            0.003,
        ]
    return source


class SharedProfileTests(unittest.TestCase):
    def test_single_shared_function_avoids_local_rounding_amplification(self):
        surface = SharedCubicSurface(
            microscopic_source(), [0, 0, 0.1 / 3, 0.1], [0, 0.01, 0.02, 0.03]
        )
        seams = surface.stations[1:-1]
        left, _ = differentials(surface, seams, side="left", lateral_m=[0])
        right, _ = differentials(surface, seams, side="right", lateral_m=[0])
        np.testing.assert_allclose(left, right, atol=1e-14, rtol=0)
        np.testing.assert_allclose(left[:, 0, 2], 0.002 * seams, atol=1e-16, rtol=0)
        local = restriction(surface.height, surface.stations / 10, 10, surface.lengths)
        reconstructed = 3 * np.diff(local, axis=1) / surface.lengths[:, None]
        self.assertGreater(abs(reconstructed[1, 0] - 0.01), 1e-9)

    def test_analytic_differentials_match_positions_across_tiny_interval(self):
        surface = SharedCubicSurface(
            microscopic_source(), [0, 0, 0.1 / 3, 0.1], [0, 0.01, 0.02, 0.03]
        )
        stations = np.array([0.2, 2.7, 5.0, 6.1, 9.8])
        analytic, _ = differentials(surface, stations)
        epsilon = 1e-5
        observed = (surface.at(stations + epsilon) - surface.at(stations - epsilon)) / (
            2 * epsilon
        )
        np.testing.assert_allclose(analytic, observed, atol=2e-9, rtol=1e-9)
        plan = prepare(surface, 0.001)
        error, _, _ = observed_error(
            surface, plan["stations_m"], plan["positions_enh_m"]
        )
        self.assertLessEqual(error, plan["maximum_stored_bound_m"] + 1e-10)
        self.assertLessEqual(plan["maximum_stored_bound_m"], 0.001)

    def test_common_height_repairs_real_bump_without_linear_solver(self):
        source = height_bump()
        curve = Curve(source)
        with patch("road_joint_profile.linprog", side_effect=AssertionError("no LP")):
            product, note = plan_profile(curve, BankLimits(0.02, 0.1, 0.04), 0.2)
        self.assertEqual(note["selected"], "shared_cubic")
        self.assertEqual(product.basis, "shared-cubic")
        self.assertEqual(product.height.shape, (4,))
        fitted = product.surface(source)
        stations = np.linspace(0, 10, 1001)
        np.testing.assert_array_equal(
            curve.at(stations)[:, :, :2], fitted.at(stations)[:, :, :2]
        )
        np.testing.assert_array_equal(
            curve.at(np.array([0, 10])), fitted.at(np.array([0, 10]))
        )
        self.assertLessEqual(abs(curve.at(stations) - fitted.at(stations)).max(), 0.2)

    def test_shared_fidelity_failure_still_allows_existing_local_program(self):
        product, note = plan_profile(
            Curve(height_bump()), BankLimits(0.05, 0.1, 0.04), 0.13
        )
        self.assertIsNotNone(product, note)
        self.assertEqual(note["selected"], "coupled")
        self.assertEqual(product.basis, "source-cubics")
        self.assertEqual(note["coupled_lp_calls"], 1)
        self.assertGreater(
            note["shared_candidate"]["residual_components"]["height_allowance"], 0
        )

    def test_shared_certificate_rejects_grade_and_crossfall_fidelity(self):
        for source, limits, height, component in (
            (recipe(height=0.1, knots=3), BankLimits(0.1, 0.1, 0.025), 0.5, "grade"),
            (bump(), BankLimits(0.04, 0.1, 0.001), 0.5, "crossfall_allowance"),
        ):
            product, note = shared_candidate(Curve(source), limits, height)
            self.assertIsNone(product)
            self.assertGreater(note["residual_components"][component], 1e-9)

    def test_old_bank_product_keeps_its_basis_and_exact_controls(self):
        curve = Curve(recipe(knots=3))
        product, note = plan_profile(curve, BankLimits(0.35, 0.12, 0.025), 0.5)
        self.assertEqual(note["selected"], "bank_only")
        self.assertEqual(product.basis, "source-cubics")
        np.testing.assert_array_equal(product.height, curve.height)
        np.testing.assert_array_equal(product.bank, prior_controls(curve))

    def test_representation_is_explicit_and_malformed_controls_fail(self):
        source = recipe()
        for height in ([0, 0, 0], [0, 0, 0, np.nan]):
            with self.assertRaises(ValueError):
                SharedCubicSurface(source, height, np.zeros(4))
        with self.assertRaises(ValueError):
            ProfileRecipe("unknown", np.zeros(4), np.zeros(4)).surface(source)
        with self.assertRaises(ValueError):
            SharedCubicSurface(source, np.zeros(4), np.zeros(4)).height_at(
                np.array([0]), np.array([0.5]), derivative=2
            )


if __name__ == "__main__":
    unittest.main()
