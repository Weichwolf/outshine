"""Source-derived long bridge values, finite closure and independent C1 controls."""

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
from outshine_ref.transport.canonical_ports._bridge_lift import (
    LiftedProfile,
    clear_coarse_observations,
    limits,
)
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from outshine_ref.transport.endpoint_grades import plan_endpoint_transition

RADIUS = 6378137.0
BINDING = {
    "sources": [{"dataset": "analytical-abutments", "revision": "1"}],
    "source_datum": "analytic model, not observed earth elevations",
    "height_mapping": "analytic model geodetic metres/1",
}


def source():
    run = (-21000, -20000, -200, 200, 30000, 31000)
    nodes = tuple(
        PortNode(i + 1, (math.degrees(x / RADIUS), 0.0)) for i, x in enumerate(run)
    )
    spans = (
        (PortSpan((100, 0), (1, 2), 8, 0.05, False, False, {"origin": "analytic"}),)
        + tuple(
            PortSpan(
                (200 + i, 0),
                (i + 2, i + 3),
                8,
                0.05,
                True,
                False,
                {"origin": "analytic"},
                layer=1,
            )
            for i in range(3)
        )
        + (PortSpan((300, 0), (6, 5), 8, 0.05, False, False, {"origin": "analytic"}),)
    )
    return PortSource(
        nodes, spans, (), {"dataset": "analytical-bridge", "revision": "r1"}
    )


def terrain(points):
    station = np.deg2rad(points[:, 0]) * RADIUS
    return 2000 * np.clip((station + 20000) / 50000, 0, 1) ** 4


def demand():
    return PortDemand((201, 0), 0.5, (0.0, 0.0), (10, 0, 512, 512))


def derive(inputs, demands, sampler=terrain, **kwargs):
    return derive_canonical_ports(
        inputs,
        tuple(demands),
        sampler,
        terrain_binding=BINDING,
        maximum_prior_change_m=0.5,
        bridge_second_derivative_per_m=0.002,
        **kwargs,
    )


class CanonicalBridgePorts(unittest.TestCase):
    def test_native_covered_bridge_flags_are_preserved_and_height_recipe_is_pending(
        self,
    ):
        inputs = source()
        covered = replace(
            inputs,
            spans=tuple(
                replace(span, tunnel=True) if span.bridge else span
                for span in inputs.spans
            ),
        )
        self.assertTrue(
            all(span.bridge and span.tunnel for span in covered.spans if span.bridge)
        )
        answer = derive(
            covered,
            [demand()],
            lambda _p: self.fail("covered recipe must qualify before DEM"),
        )
        self.assertIsNone(answer.values[0])
        self.assertEqual(answer.reasons[0], "covered_bridge_height_recipe_required")

    def test_quartic_lift_preserves_c1_ends_and_bounds_whole_intervals(self):
        profile, _report = plan_endpoint_transition(
            162, 162, 900, 0, -0.008, grade=0.12, second_derivative=0.002
        )
        lifted = LiftedProfile(profile, 4.0)
        for derivative in (0, 1):
            np.testing.assert_array_equal(
                lifted.evaluate([0, 900], derivative),
                profile.evaluate([0, 900], derivative),
            )
        bound = limits(profile, 4.0)
        for derivative, upper in enumerate(bound, 1):
            dense = float(
                abs(lifted.evaluate(np.linspace(0, 900, 8193), derivative)).max()
            )
            self.assertGreaterEqual(upper, dense)
        self.assertLess(bound[0], 0.12)
        self.assertLess(bound[1], 0.002)

    def test_unreachable_coarse_dem_obstacle_is_rejected(self):
        profile, _report = plan_endpoint_transition(
            0, 0, 1000, 0, 0, grade=0.05, second_derivative=0.002
        )
        with self.assertRaisesRegex(
            ValueError, "coarse_DEM_lift_exceeds_profile_derivative_bounds"
        ):
            clear_coarse_observations(
                profile,
                np.array([[500.0, 0.0, 1000.0]]),
                np.zeros(2),
                np.array([1.0, 0.0]),
                np.array([0.0, 1.0]),
                0,
                1000,
                [(0, 0), (0, 0)],
                0,
                0.05,
                0.002,
            )

    def test_forty_bridge_spans_use_both_distant_incoming_ground_connections(self):
        run = [-21000, *np.linspace(-20000, 30000, 41), 31000]
        nodes = tuple(
            PortNode(i + 1, (math.degrees(float(x) / RADIUS), 0.0))
            for i, x in enumerate(run)
        )
        spans = (
            PortSpan((100, 0), (1, 2), 8, 0.05, False, False, {"origin": "analytic"}),
            *(
                PortSpan(
                    (200 + i, 0),
                    (i + 2, i + 3),
                    8,
                    0.05,
                    True,
                    False,
                    {"origin": "analytic"},
                    layer=1,
                )
                for i in range(40)
            ),
            PortSpan((300, 0), (43, 42), 8, 0.05, False, False, {"origin": "analytic"}),
        )
        inputs = PortSource(
            nodes, spans, (), {"dataset": "analytical-40-span-bridge", "revision": "1"}
        )
        at = PortDemand((216, 0), 0.0, (0.0, 0.0), (10, 0, 512, 512))
        answer = derive(inputs, [at])
        self.assertEqual(answer.reasons, ("",))
        self.assertGreater(answer.values[0].height_m, 500)
        component = answer.report["components"][0]
        self.assertEqual(len(component["selected"]), 42)
        self.assertEqual(component["closure_work"]["incidence_visits"], 84)
        self.assertLess(component["field"]["terrain_samples"], 2000)
        self.assertGreater(
            component["field"]["coarse_DEM_lift"]["coarse_DEM_samples"], 1000
        )
        self.assertLessEqual(component["field"]["profile"]["intervals"], 8)

    def test_remote_dem_landing_changes_local_port_and_order_is_exact(self):
        inputs = source()
        forward = derive(inputs, [demand(), demand()])
        backward = derive(
            replace(inputs, nodes=inputs.nodes[::-1], spans=inputs.spans[::-1]),
            [demand()],
        )
        self.assertEqual(forward.reasons, ("", ""))
        self.assertEqual(forward.values[0], forward.values[1])
        self.assertEqual(forward.values[0], backward.values[0])
        port = forward.values[0]
        self.assertGreater(port.height_m, 500)
        self.assertLess(port.height_m, 1000)
        self.assertGreater(port.longitudinal_grade, 0.03)
        self.assertLess(port.longitudinal_grade, 0.05)
        self.assertAlmostEqual(port.crossfall_rad, 0, delta=1e-12)
        field = forward.report["components"][0]["field"]
        self.assertEqual(field["selected_method"], "bridge_endpoint_envelope")
        self.assertGreater(field["profile"]["length_m"], 50000)
        self.assertLessEqual(field["profile"]["intervals"], 8)
        self.assertLessEqual(
            field["profile"]["maximum_grade"], field["maximum_source_grade"]
        )
        self.assertLess(field["maximum_source_grade"], 0.05)
        self.assertLess(field["landing_maximum_DEM_plane_error_m"], 0.5)
        self.assertEqual(
            forward.report["work"]["terrain_samples"], field["terrain_samples"] + 2
        )
        self.assertLessEqual(
            forward.report["work"]["terrain_samples"], PortBudget().terrain_samples
        )

    def test_far_condition_excludes_local_zero_in_the_same_source_frame(self):
        inputs = source()
        answer = derive(inputs, [demand()])
        row = answer.report["components"][0]["field"]
        ends = np.array(row["source_landing_positions"])
        anchor = inputs.nodes[0].longitude_latitude
        zero = positions_in_frame(anchor, np.array([[0.0, 0.0]]), np.array([0.0]))[0]
        current = positions_in_frame(
            anchor, np.array([[0.0, 0.0]]), np.array([answer.values[0].height_m])
        )[0]
        grade = row["maximum_source_grade"]
        reach = np.linalg.norm(ends[:, :2] - current[:2], axis=1) * grade
        lower, upper = max(ends[:, 2] - reach), min(ends[:, 2] + reach)
        self.assertGreater(lower, zero[2])
        self.assertLessEqual(lower, current[2])
        self.assertGreaterEqual(upper, current[2])

    def test_missing_ground_connection_unknown_and_budget_stay_pending_before_dem(self):
        inputs = source()
        cases = [
            (
                replace(inputs, spans=inputs.spans[:-1]),
                PortBudget(),
                "ground_connections_required",
            ),
            (
                replace(inputs, unknown_nodes=(6,)),
                PortBudget(),
                "unknown_source_frontier",
            ),
            (inputs, PortBudget(selected_spans=3), "selected_span_budget"),
            (inputs, PortBudget(terrain_samples=1), "terrain_sample_budget"),
        ]
        for changed, budget, reason in cases:
            with self.subTest(reason=reason):
                answer = derive(
                    changed,
                    [demand()],
                    lambda _p: self.fail("DEM before admission"),
                    budget=budget,
                )
                self.assertIsNone(answer.values[0])
                self.assertIn(reason, answer.reasons[0])

    def test_missing_actual_remote_dem_stays_pending(self):
        def missing(points):
            if np.any(points[:, 0] > math.degrees(25000 / RADIUS)):
                raise FileNotFoundError("remote bound DEM unavailable")
            return terrain(points)

        answer = derive(source(), [demand()], missing)
        self.assertIsNone(answer.values[0])
        self.assertIn("remote bound DEM unavailable", answer.reasons[0])


if __name__ == "__main__":
    unittest.main()
