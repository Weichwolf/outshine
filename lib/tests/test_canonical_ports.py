"""Canonical source-derived heights, frame signs and admission before sampling."""

import math
import unittest
from dataclasses import replace

import numpy as np
from outshine_ref.transport.canonical_ports import (
    PortBudget,
    PortDemand,
    PortNode,
    PortSource,
    PortSpan,
    derive_canonical_ports,
)
from outshine_ref.transport.canonical_ports._geodesy import (
    frame,
    ground_geographic_at,
    positions_in_frame,
)
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.canonical_ports._queries import canonical_port
from test_port_affine import terminal

TERRAIN_BINDING = {
    "sources": [{"dataset": "analytic-model", "revision": "1"}],
    "source_datum": "analytic, not an observed elevation datum",
    "height_mapping": "explicit analytic model geodetic metres/1",
}


def analytic_source():
    nodes = tuple(
        PortNode(2**63 + i + 1, (16.37 + i * 0.0007, 48.21)) for i in range(3)
    )
    spans = tuple(
        PortSpan(
            (2**60 + i + 1, 0),
            (nodes[i].identifier, nodes[i + 1].identifier),
            3.5,
            0.2,
            False,
            False,
            {"source_segment_ordinal": i + 5},
        )
        for i in range(2)
    )
    return PortSource(
        nodes, spans, (), {"dataset": "analytic-normalized", "revision": "1"}
    )


def demand_for(source, index=0):
    span = source.spans[index]
    nodes = {node.identifier: node.longitude_latitude for node in source.nodes}
    a, b = (nodes[node] for node in span.nodes)
    level = 20
    cells = 2**level
    line = math.ceil((a[0] + 180) * cells / 360)
    longitude = -180 + line * 360 / cells
    fraction = (longitude - a[0]) / (b[0] - a[0])
    segment = math.floor((a[1] + 90) * cells / 180)
    return PortDemand(
        span.identifier, fraction, (longitude, a[1]), (level, 0, line, segment)
    )


def terrain(points):
    return 170 + (points[:, 0] - 16.37) * 10000 + (points[:, 1] - 48.21) * 500


def derive(source, demands, sampler=terrain, **kwargs):
    return derive_canonical_ports(
        source,
        tuple(demands),
        sampler,
        terrain_binding=TERRAIN_BINDING,
        maximum_prior_change_m=0.1,
        **kwargs,
    )


class CanonicalPorts(unittest.TestCase):
    def test_direct_affine_prior_is_a_complete_canonical_field_without_iterations(self):
        for method in ("cone", "lp"):
            with self.subTest(method=method):
                field, report = fit_ground(terminal(), 0.1, method, PortBudget())
                self.assertIsNotNone(field)
                solvers = [
                    r["fitting"]["solver"] for r in report["rounds"] if r["fitting"]
                ]
                self.assertTrue(solvers)
                self.assertTrue(
                    all(
                        s["status"] == "affine_prior" and s["termination_complete"]
                        for s in solvers
                    )
                )

    def test_port_ray_matches_independent_ecef_plane_intersection(self):
        class Plane:
            def evaluate(self, xy):
                return 170 + 0.02 * xy[0] - 0.01 * xy[1], np.array([0.02, -0.01])

        # At (0,0), source ENU is exactly (ECEF Y, ECEF Z, ECEF X).
        a, eccentricity_squared = 6378137.0, 6.6943799901413165e-3
        plane_point = np.array([a + 170, 0, 0])
        normal = np.array([1, -0.02, 0.01])
        for anchor in ((0.5, 0.25), (-0.4, 0.3)):
            lon, lat = np.deg2rad(anchor)
            sin_lon, cos_lon = math.sin(lon), math.cos(lon)
            sin_lat, cos_lat = math.sin(lat), math.cos(lat)
            radius = a / math.sqrt(1 - eccentricity_squared * sin_lat**2)
            origin = radius * np.array(
                [
                    cos_lat * cos_lon,
                    cos_lat * sin_lon,
                    (1 - eccentricity_squared) * sin_lat,
                ]
            )
            east = np.array([-sin_lon, cos_lon, 0])
            north = np.array([-sin_lat * cos_lon, -sin_lat * sin_lon, cos_lat])
            up = np.array([cos_lat * cos_lon, cos_lat * sin_lon, sin_lat])
            height = float(normal @ (plane_point - origin) / (normal @ up))
            expected = -np.array([normal @ east, normal @ north]) / (normal @ up)
            value = canonical_port(
                Plane(),
                (0, 0),
                anchor,
                np.array(
                    [[anchor[0] - 0.001, anchor[1]], [anchor[0] + 0.001, anchor[1]]]
                ),
                1.75,
                170,
            )
            self.assertAlmostEqual(value["height_m"], height, delta=2e-8)
            np.testing.assert_allclose(value["plane_EN"], expected, rtol=0, atol=2e-15)
            self.assertGreater(abs(height - 170), 1)

    def test_actual_sampling_produces_nonzero_slopes_and_original_integer_ids(self):
        source = analytic_source()
        demand = demand_for(source)
        result = derive(source, [demand])
        self.assertEqual(result.reasons, ("",))
        value = result.values[0]
        self.assertEqual(value.edge, demand.edge)
        self.assertGreater(value.edge[0], 2**53)
        self.assertEqual(value.source_attributes["source_segment_ordinal"], 5)
        expected = float(terrain(np.array([demand.anchor_longitude_latitude]))[0])
        self.assertAlmostEqual(value.height_m, expected, delta=0.002)
        self.assertGreater(abs(value.longitudinal_grade), 0.01)
        self.assertGreater(abs(value.crossfall_rad), 0.001)
        t = np.array(value.direction_en)
        p = np.array(value.plane_en)
        self.assertAlmostEqual(np.linalg.norm(t), 1)
        self.assertAlmostEqual(value.longitudinal_grade, p @ t)
        self.assertAlmostEqual(value.crossfall_rad, -math.atan(p @ [-t[1], t[0]]))

    def test_source_order_and_core_request_order_do_not_change_values(self):
        source = analytic_source()
        first = demand_for(source)
        second = demand_for(source, 1)
        # Use the same actual cut twice, as two neighboring core consumers do.
        forward = derive(source, [first, second, first])
        backward = derive(
            replace(source, nodes=source.nodes[::-1], spans=source.spans[::-1]),
            [first, second, first][::-1],
        )
        self.assertEqual(forward.values, backward.values[::-1])
        self.assertEqual(forward.values[0], forward.values[2])
        self.assertNotEqual(second.edge, first.edge)

    def test_unknown_closure_and_admission_do_not_sample_terrain(self):
        source = analytic_source()
        demand = demand_for(source)
        calls = []

        def forbidden(points):
            calls.append(len(points))
            raise AssertionError(
                "terrain must remain untouched before source admission"
            )

        cases = [
            (
                replace(source, unknown_nodes=(source.nodes[-1].identifier,)),
                PortBudget(),
                "unknown_source_frontier",
            ),
            (source, PortBudget(input_nodes=1), "source_input_budget"),
            (source, PortBudget(selected_spans=1), "selected_span_budget"),
            (source, PortBudget(terrain_samples=1), "terrain_sample_budget"),
        ]
        for current, budget, expected in cases:
            with self.subTest(reason=expected):
                result = derive(current, [demand], forbidden, budget=budget)
                self.assertIsNone(result.values[0])
                self.assertIn(expected, result.reasons[0])
        self.assertEqual(calls, [])

    def test_source_direction_reversal_changes_signed_grades_not_the_plane(self):
        source = analytic_source()
        demand = demand_for(source)
        forward = derive(source, [demand]).values[0]
        reverse = replace(
            source,
            spans=tuple(replace(span, nodes=span.nodes[::-1]) for span in source.spans),
        )
        backward = derive(
            reverse, [replace(demand, fraction=1 - demand.fraction)]
        ).values[0]
        self.assertIsNotNone(forward)
        self.assertIsNotNone(backward)
        self.assertEqual(forward.height_m, backward.height_m)
        self.assertEqual(forward.plane_en, backward.plane_en)
        np.testing.assert_array_equal(
            forward.direction_en, -np.array(backward.direction_en)
        )
        self.assertEqual(forward.longitudinal_grade, -backward.longitudinal_grade)
        self.assertEqual(forward.crossfall_rad, -backward.crossfall_rad)

    def test_missing_clip_recipe_layer_bridge_and_tunnel_are_explicit_pending(self):
        source = analytic_source()
        demand = demand_for(source)
        for change, reason in [
            ({"maximum_gradient": None}, "missing_source_profile_recipe"),
            ({"layer": 1}, "layer_height_recipe_required"),
            ({"bridge": True}, "bridge_anchor_recipe_required"),
            ({"tunnel": True}, "tunnel_height_recipe_required"),
        ]:
            current = replace(
                source, spans=(replace(source.spans[0], **change), source.spans[1])
            )
            result = derive(
                current, [demand], lambda _p: self.fail("unqualified source sampled")
            )
            self.assertIsNone(result.values[0])
            self.assertEqual(result.reasons[0], reason)
        bad = replace(demand, fraction=1 - demand.fraction)
        result = derive(source, [bad], lambda _p: self.fail("bad clip sampled"))
        self.assertIn("source_clip", result.reasons[0])

    def test_missing_dem_and_unmet_quality_never_emit_zero_height(self):
        source = analytic_source()
        demand = demand_for(source)

        def unavailable(_points):
            raise FileNotFoundError("required bound DEM absent")

        missing = derive(source, [demand], unavailable)
        self.assertIsNone(missing.values[0])
        self.assertIn("required bound DEM absent", missing.reasons[0])

        def rough(points):
            return 170 + 5 * np.cos((points[:, 0] - 16.37) * 100000)

        poor = derive(source, [demand], rough, method="affine")
        self.assertIsNone(poor.values[0])
        self.assertIn("ground_prior_error_limit", poor.reasons[0])

    def test_geodetic_ground_projection_and_distinct_frames(self):
        origin, axes = frame((0, 0))
        np.testing.assert_array_equal(origin, [6378137, 0, 0])
        np.testing.assert_array_equal(axes[:, 2], [1, 0, 0])
        for anchor in [(16.37, 48.21), (179.99, 2), (30, 89.5)]:
            xy = np.array([[0, 0], [100, -80], [-200, 300]], float)
            geographic = ground_geographic_at(anchor, xy)
            reconstructed = positions_in_frame(anchor, geographic, np.zeros(len(xy)))
            np.testing.assert_allclose(reconstructed[:, :2], xy, atol=2e-8, rtol=0)

    def test_invalid_original_ids_domains_and_duplicate_spans_are_rejected(self):
        with self.assertRaises(ValueError):
            PortNode(float(2**63), (0, 0))
        with self.assertRaises(ValueError):
            PortNode(1, (0, 91))
        source = analytic_source()
        duplicated = replace(source, spans=(source.spans[0], source.spans[0]))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            derive(duplicated, [demand_for(source)])


if __name__ == "__main__":
    unittest.main()
