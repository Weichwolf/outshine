"""Construction proposals checked against independent line geometry and graph."""

import copy
import math
import unittest

from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from shapely import LineString, Point


def row(key, xy, kind="path", **changes):
    return dict(
        source_id=key,
        xy_m=xy,
        kind=kind,
        half_width_m=0.75,
        layer=0,
        bridge=False,
        tunnel=False,
        foot=True,
        position_error_bound_m=None,
        endpoint_roles=["connectable", "connectable"],
        **changes,
    )


def simple():
    return [
        row("branch", [(0, -5), (0, -0.25)], "steps"),
        row("through", [(-5, 0), (5, 0)]),
    ]


def stable(plan):
    return {
        k: v
        for k, v in plan.items()
        if k not in ("work", "input_prepare_s", "index_build_s", "elapsed_s")
    }


class OriginalCase(unittest.TestCase):
    def setUp(self):
        self.case, self.rows = inputs()

    def test_original_knots_and_source_metadata_retained_without_mutation(self):
        original = copy.deepcopy(self.rows)
        plan = plan_connections(self.rows)
        self.assertEqual(self.rows, original)
        self.assertEqual(len(plan["source_usage"]), 3)
        for source in self.rows:
            constructed = plan["axes_xy_m"][source["source_id"]]
            cursor = iter(constructed)
            self.assertTrue(
                all(any(list(p) == q for q in cursor) for p in source["xy_m"])
            )

    def test_two_contacts_link_all_three_original_parts(self):
        plan = plan_connections(self.rows)
        self.assertEqual(len(plan["connections"]), 2)
        connections = plan["connections"]
        self.assertEqual(
            {c["target_source_id"] for c in connections},
            {"native-part:431", "native-part:432"},
        )
        self.assertEqual(
            sorted(c["advance_m"] == 0 for c in connections), [False, True]
        )
        for c in connections:
            target = LineString(
                next(
                    r["xy_m"]
                    for r in self.rows
                    if r["source_id"] == c["target_source_id"]
                )
            )
            branch = plan["axes_xy_m"][c["source_id"]]
            self.assertLess(target.distance(Point(c["xy_m"])), 1e-10)
            self.assertAlmostEqual(
                target.project(Point(c["xy_m"])), c["target_station_m"], delta=1e-10
            )
            self.assertEqual(branch[c["source_endpoint"]], c["xy_m"])
            self.assertIn(c["xy_m"], plan["axes_xy_m"][c["target_source_id"]])
            self.assertFalse(c["source_node_identity_claimed"])
            self.assertFalse(c["provider_position_bound_known"])
        upper = next(c for c in connections if c["advance_m"] > 0)
        self.assertGreater(upper["advance_m"], 0.28)
        self.assertLess(upper["advance_m"], 0.29)

    def test_extension_matches_independent_geos_tangent_intersection(self):
        plan = plan_connections(self.rows)
        contact = next(c for c in plan["connections"] if c["advance_m"] > 0)
        branch = next(r for r in self.rows if r["kind"] == "steps")["xy_m"]
        a, b = branch[-2:]
        length = math.dist(a, b)
        far = [p + 0.5 * (p - q) / length for p, q in zip(b, a, strict=True)]
        target = next(
            r["xy_m"]
            for r in self.rows
            if r["source_id"] == contact["target_source_id"]
        )
        intersection = LineString((b, far)).intersection(LineString(target))
        self.assertEqual(intersection.geom_type, "Point")
        self.assertLess(intersection.distance(Point(contact["xy_m"])), 1e-10)

    def test_scan_and_index_identical_under_rotation_translation_and_input_order(self):
        baseline = plan_connections(self.rows)
        for angle in (0, 0.43, math.pi / 2, 2.91):
            for offset in ((0, 0), (1e6, -1e6)):
                rotated = copy.deepcopy(self.rows[::-1])
                for source in rotated:
                    source["xy_m"] = [
                        [
                            offset[0] + math.cos(angle) * x - math.sin(angle) * y,
                            offset[1] + math.sin(angle) * x + math.cos(angle) * y,
                        ]
                        for x, y in source["xy_m"]
                    ]
                plan = plan_connections(rotated)
                self.assertEqual(
                    stable(plan), stable(plan_connections(rotated, method="scan"))
                )
                self.assertEqual(len(plan["connections"]), 2)
                for before, after in zip(
                    baseline["connections"], plan["connections"], strict=True
                ):
                    self.assertAlmostEqual(
                        before["advance_m"], after["advance_m"], delta=1e-8
                    )

    def test_reach_sensitivity_is_explicit(self):
        for reach, expected in (
            (0, 1),
            (0.1, 1),
            (0.28, 1),
            (0.3, 2),
            (0.5, 2),
            (0.75, 2),
        ):
            self.assertEqual(
                len(plan_connections(self.rows, reach_m=reach)["connections"]), expected
            )


class AdmissionCounterexamples(unittest.TestCase):
    def test_basic_forward_target(self):
        plan = plan_connections(simple())
        self.assertEqual(len(plan["connections"]), 1)
        self.assertEqual(plan["connections"][0]["xy_m"], [0, 0])

    def test_separated_levels_bridges_tunnels_unknowns_and_denied_access(self):
        for owner in (0, 1):
            for field, value in (
                ("layer", 1),
                ("layer", None),
                ("bridge", True),
                ("bridge", None),
                ("tunnel", True),
                ("foot", False),
            ):
                rows = simple()
                rows[owner][field] = value
                self.assertEqual(
                    plan_connections(rows)["connections"], [], (owner, field)
                )

    def test_parallel_way_and_target_behind_endpoint(self):
        for target in ([(-5, -0.5), (5, -0.5)], [(0.25, -5), (0.25, 5)]):
            rows = simple()
            rows[1]["xy_m"] = target
            self.assertEqual(plan_connections(rows)["connections"], [])

    def test_terminal_fragment_unknown_roles_never_extend(self):
        for role in ("terminal", "fragment", "unknown"):
            rows = simple()
            rows[0]["endpoint_roles"] = [role, role]
            self.assertEqual(plan_connections(rows)["connections"], [])

    def test_near_target_endpoint_and_beyond_width_are_rejected(self):
        rows = simple()
        rows[1]["xy_m"] = [(-0.1, 0), (5, 0)]
        self.assertEqual(plan_connections(rows)["connections"], [])
        rows = simple()
        rows[0]["xy_m"][-1] = (0, -0.8)
        self.assertEqual(plan_connections(rows, reach_m=2)["connections"], [])

    def test_position_bounds_cap_reach_when_supplied(self):
        rows = simple()
        for r in rows:
            r["position_error_bound_m"] = 0.1
        self.assertEqual(plan_connections(rows)["connections"], [])
        for r in rows:
            r["position_error_bound_m"] = 0.2
        self.assertTrue(
            plan_connections(rows)["connections"][0]["provider_position_bound_known"]
        )

    def test_multiple_targets_are_unresolved_even_with_same_provider_id(self):
        rows = simple() + [row("other-part", [(-5, 0.1), (5, 0.1)])]
        for r in rows:
            r["provider_feature_id"] = 77
        plan = plan_connections(rows)
        self.assertEqual(plan["connections"], [])
        self.assertEqual(
            plan["endpoint_dispositions"][-1]["reason"], "ambiguous_targets"
        )

    def test_target_knot_is_one_contact_not_two_competing_segments(self):
        rows = simple()
        rows[1]["xy_m"] = [(-5, 0), (0, 0), (5, 0)]
        self.assertEqual(len(plan_connections(rows)["connections"]), 1)

    def test_budget_refuses_incomplete_candidate_set_instead_of_selecting_first(self):
        plan = plan_connections(simple(), max_candidates=0)
        self.assertEqual(plan["connections"], [])
        self.assertEqual(plan["work"]["candidate_tests"], 0)
        self.assertGreater(plan["work"]["budget_refusals"], 0)
        with self.assertRaises(ValueError):
            plan_connections(simple(), max_segments=1)

    def test_dense_query_budget_never_publishes_a_partial_nearest_target(self):
        rows = [simple()[0]] + [
            row(f"target:{i}", [(-5, i * 0.001), (5, i * 0.001)]) for i in range(100)
        ]
        plan = plan_connections(rows, max_candidates=5)
        self.assertEqual(plan["connections"], [])
        self.assertEqual(plan["work"]["candidate_tests"], 0)
        self.assertEqual(plan["work"]["max_query_indices"], 100)
        self.assertEqual(plan["work"]["budget_refusals"], 1)

    def test_scan_and_index_have_same_partial_budget_policy_when_complete(self):
        self.assertEqual(
            stable(plan_connections(simple())),
            stable(plan_connections(simple(), method="scan")),
        )

    def test_nonfinite_degenerate_and_invalid_budgets_refused(self):
        for xy in ([(0, 0), (0, 0)], [(0, 0), (float("nan"), 1)]):
            rows = simple()
            rows[0]["xy_m"] = xy
            with self.assertRaises(ValueError):
                plan_connections(rows)
        with self.assertRaises(ValueError):
            plan_connections(simple(), max_candidates=-1)


if __name__ == "__main__":
    unittest.main()
