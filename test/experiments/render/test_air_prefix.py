"""Closed forms, source shadow cuts and monotone-query contracts for #206/#193."""

import unittest
from pathlib import Path

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import depth_limit
from air_path_integral import engine_clear_air
from air_path_prefix import AirCells, AirPrefix, build_cells
from air_path_segments import view_edges

ROOT = Path(__file__).resolve().parents[3]


def constant_cells():
    edges = np.array([0.0, 0.1, 1.0, 3.0, 10.0])
    extinction = np.broadcast_to([0.0, 0.01, 0.03], (4, 2, 3)).copy()
    emission = np.broadcast_to([0.02, 0.03, 0.04], (4, 2, 3)).copy()
    return AirCells(edges, extinction, emission, 0)


class AirPrefixContracts(unittest.TestCase):
    def test_constant_coefficients_have_closed_form_including_zero_extinction(self):
        cells = constant_cells()
        prefix = AirPrefix(cells)
        beta, source = cells.extinction[0, 0], cells.emission[0, 0]
        for distance in (0.0, 0.001, 0.5, 1.9, 7.0, 10.0):
            radiance, through = prefix.advance(distance)
            expected = (
                np.divide(
                    -np.expm1(-beta * distance),
                    beta,
                    out=np.full(3, distance),
                    where=beta > 0,
                )
                * source
            )
            np.testing.assert_allclose(radiance, expected, rtol=1e-12, atol=1e-14)
            np.testing.assert_allclose(through, np.exp(-beta * distance), rtol=1e-14)

    def test_linear_extinction_with_constant_source_radiance_has_closed_form(self):
        edges = np.array([0.0, 0.1, 1.0, 3.0, 10.0])
        base, slope, source = (
            np.array([0.01, 0.02, 0.03]),
            np.array([0.0001, 0.0002, 0.0003]),
            np.array([0.2, 0.3, 0.4]),
        )
        extinction = base + np.stack([edges[:-1], edges[1:]], axis=1)[..., None] * slope
        prefix = AirPrefix(AirCells(edges, extinction, extinction * source, 0))
        for distance in (0.001, 0.5, 2.0, 8.0, 10.0):
            radiance, through = prefix.advance(distance)
            expected_t = np.exp(-base * distance - 0.5 * slope * distance**2)
            np.testing.assert_allclose(through, expected_t, rtol=1e-14)
            np.testing.assert_allclose(
                radiance, source * (1.0 - expected_t), rtol=1e-11, atol=1e-14
            )

    def test_cloud_event_queries_preserve_the_same_final_front(self):
        cells = constant_cells()
        direct = AirPrefix(cells).advance(10.0)
        prefix = AirPrefix(cells)
        for distance in np.linspace(0.0, 10.0, 65):
            first = prefix.advance(distance)
            repeat = prefix.advance(distance)
            for actual, expected in zip(first, repeat, strict=True):
                np.testing.assert_array_equal(actual, expected)
        for actual, expected in zip(prefix.advance(10.0), direct, strict=True):
            np.testing.assert_allclose(actual, expected, rtol=1e-14, atol=1e-14)

    def test_backward_nonfinite_and_outside_queries_are_rejected(self):
        prefix = AirPrefix(constant_cells())
        prefix.advance(0.5)
        for distance in (0.4, np.nan, np.inf, 11.0):
            with self.assertRaises(ValueError):
                prefix.advance(distance)
            self.assertEqual(prefix.distance_km, 0.5)

    def test_planetary_shadow_is_not_interpolated_across_its_boundary(self):
        medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
        cases, _ = place_cases(ROOT)
        case = next(
            case
            for case in cases
            if case.name == "Koerbersee/2026-09-07T18:00:00Z/elevation=-0.5"
        )
        physical = view_edges(medium, *case.arguments(), 200.0)
        self.assertEqual(len(physical), 3)
        boundary = physical[1]
        cells = build_cells(medium, case.arguments(), 200.0, 32)
        prefix = AirPrefix(cells)
        for distance in (boundary - 0.001, boundary):
            radiance, _ = prefix.advance(distance)
            np.testing.assert_array_equal(radiance, np.zeros(3))
        radiance, through = prefix.advance(boundary + 0.001)
        self.assertGreater(radiance[0], 0.0)
        self.assertTrue(np.all((through > 0.0) & (through <= 1.0)))
        self.assertLessEqual(cells.source_evaluations, 64)

    def test_descending_night_ray_does_not_light_its_ground_endpoint(self):
        medium = engine_clear_air(ROOT / "src/world/sky/Atmosphere.h")
        cases, _ = place_cases(ROOT)
        case = next(
            case
            for case in cases
            if case.name == "Rosenheim/2026-09-07T18:00:00Z/elevation=-1.6"
        )
        distance = depth_limit(medium, case.height_km, case.arguments()[1], 200.0)
        cells = build_cells(medium, case.arguments(), distance, 47)
        prefix = AirPrefix(cells)
        for point in (0.001, distance * 0.5, distance):
            radiance, _ = prefix.advance(point)
            np.testing.assert_array_equal(radiance, np.zeros(3))


if __name__ == "__main__":
    unittest.main()
