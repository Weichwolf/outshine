"""Actual neighbour capture, independent contact scan and six-source geometry."""

import copy
import unittest
from itertools import combinations
from pathlib import Path
from unittest.mock import patch

import shapely
from compare_mvt_t_region_boundaries import outside_axis_contacts
from mvt_t_connection_inputs import inputs
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_region import region_inputs
from mvt_t_profile_route_bound import route_prior_bound
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build
from stair_network_geometry import vertex_links

FIXTURE = Path(__file__).with_name("fixtures") / "mvt_stair_boundary_neighbours.json"
SELECTION = [f"native-part:{i}" for i in (240, 431, 432, 402, 535, 537)]


class ObservedJointRegion(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture, _sources = inputs(FIXTURE)
        cls.case, cls.sources, cls.proposal = region_inputs(cls.capture, SELECTION)
        cls.plan = prepare_profiles(
            cls.case, cls.sources, cls.proposal, mesh_recipe="outward_binary_grid"
        )
        cls.heights, cls.solve = solve_profiles(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.solve)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)
        cls.products = shared_products(cls.plan, cls.heights, cls.data)

    def test_full_capture_and_original_axis_priors_remain_exact(self):
        context = self.proposal["region_context"]
        self.assertEqual(context["capture"], self.capture)
        self.assertEqual(context["selected_source_ids"], sorted(SELECTION))
        self.assertEqual(
            context["unselected_sources"],
            [
                {
                    "source_id": "native-part:239",
                    "reason": "outside_caller_selected_joint_region",
                }
            ],
        )
        originals = {w["native_way"]: w for w in self.capture["ways"]}
        for way in self.case["ways"]:
            original = originals[way["native_way"]]
            self.assertEqual(way["source_xy_m"], original["source_xy_m"])
            self.assertEqual(
                way["sampled_design_xyz_m"],
                [row[:3] for row in original["sampled_design_xy_height_node"]],
            )

    def test_hash_join_matches_independent_source_pair_coordinate_scan(self):
        expected = set()
        for a, b in combinations(self.capture["ways"], 2):
            for xy in a["source_xy_m"]:
                if xy in b["source_xy_m"]:
                    expected.add(
                        (tuple(xy), tuple(sorted((a["native_way"], b["native_way"]))))
                    )
        actual = set()
        for group in self.proposal["region_context"]["observed_contacts"]:
            self.assertEqual(group["status"], "observed_compatible_native_contact")
            node = group["native_node"]
            self.assertIs(type(node), int)
            self.assertGreater(node, 2**53)
            for a, b in combinations(group["members"], 2):
                actual.add(
                    (
                        tuple(group["xy_m"]),
                        tuple(
                            sorted(
                                (
                                    int(a["source_id"].split(":")[-1]),
                                    int(b["source_id"].split(":")[-1]),
                                )
                            )
                        ),
                    )
                )
        self.assertEqual(actual, expected)

    def test_original_outer_contacts_are_joint_and_only_real_frontiers_remain(self):
        roles = {
            (r["source_id"], r["endpoint"]): r["status"]
            for r in self.proposal["region_context"]["endpoint_roles"]
        }
        for key in (
            ("native-part:431", 0),
            ("native-part:431", -1),
            ("native-part:432", 0),
        ):
            self.assertEqual(roles[key], "joint_observed_contact")
        ports = self.products["external_contacts"]
        self.assertEqual(len(ports), 4)
        self.assertEqual(
            sum(p["boundary_role"] == "unknown_neighbour_not_free_end" for p in ports),
            3,
        )
        external = next(p for p in ports if p["source_id"] == "native-part:535")
        self.assertEqual(external["endpoint"], 0)
        self.assertEqual(
            external["boundary_role"],
            "observed_outside_contact_requires_port_or_expansion",
        )
        self.assertIn(
            "No boundary or native admission", self.products["region_context"]["scope"]
        )

    def test_actual_body_whole_profiles_and_connected_navigation(self):
        proof = audit_profiles(self.plan, self.heights, self.data)
        self.assertLessEqual(proof["maximum_prior_error_m"], 2 + 1e-7)
        self.assertLessEqual(proof["whole_landing_error_m"], 1e-7)
        self.assertLessEqual(max(proof["body"]["source_coverage_error_m2"]), 1e-7)
        vertex_links(self.body.vertices, self.body.triangles)
        graph = self.products["navigation"]
        adjacency = {i: set() for i in range(len(graph["nodes_xyz_m"]))}
        for edge in graph["edges"]:
            a, b = edge["nodes"]
            adjacency[a].add(b)
            adjacency[b].add(a)
        seen, todo = set(), [0]
        while todo:
            node = todo.pop()
            if node not in seen:
                seen.add(node)
                todo.extend(adjacency[node] - seen)
        self.assertEqual(len(seen), len(adjacency))
        self.assertEqual(len(self.products["landings"]), 6)

    def test_conflicting_native_node_is_never_an_observed_shared_junction(self):
        case = copy.deepcopy(self.capture)
        way = next(w for w in case["ways"] if w["native_way"] == 537)
        way["sampled_design_xy_height_node"][0][3] += 1
        _case, _sources, proposal = region_inputs(case, SELECTION)
        xy = way["source_xy_m"][0]
        group = next(
            g
            for g in proposal["region_context"]["observed_contacts"]
            if g["xy_m"] == xy
        )
        self.assertEqual(group["status"], "unqualified_coincidence")
        self.assertFalse(any(j["xy_m"] == xy for j in proposal["observed_junctions"]))

    def test_different_layer_does_not_create_a_shared_native_junction(self):
        case = copy.deepcopy(self.capture)
        way = next(w for w in case["ways"] if w["native_way"] == 537)
        way["derived_layer"] += 1
        _case, _sources, proposal = region_inputs(case, SELECTION)
        xy = way["source_xy_m"][0]
        self.assertFalse(any(j["xy_m"] == xy for j in proposal["observed_junctions"]))

    def test_small_coordinate_gap_does_not_invent_an_exact_observed_contact(self):
        case = copy.deepcopy(self.capture)
        way = next(w for w in case["ways"] if w["native_way"] == 537)
        way["source_xy_m"][0][0] += 1e-5
        _case, _sources, proposal = region_inputs(case, SELECTION)
        self.assertFalse(
            any(
                set(j["source_ids"]) == {"native-part:431", "native-part:537"}
                for j in proposal["observed_junctions"]
            )
        )

    def test_budget_and_duplicate_selection_refuse_before_construction(self):
        case = copy.deepcopy(self.capture)
        case["ways"] *= 10
        with patch("mvt_t_profile_region.plan_connections", side_effect=AssertionError):
            with self.assertRaisesRegex(ValueError, "budget"):
                region_inputs(case, SELECTION)
            with self.assertRaisesRegex(ValueError, "selection"):
                region_inputs(self.capture, SELECTION + SELECTION[:1])

    def test_float_native_node_is_not_accepted_as_uint64_identity(self):
        case = copy.deepcopy(self.capture)
        case["ways"][0]["sampled_design_xy_height_node"][0][3] = float(
            case["ways"][0]["sampled_design_xy_height_node"][0][3]
        )
        with self.assertRaisesRegex(ValueError, "uint64"):
            region_inputs(case, SELECTION)

    def test_actual_outside_axis_point_has_an_independent_necessary_prior_bound(self):
        contacts = outside_axis_contacts(self.capture, self.proposal["region_context"])
        self.assertEqual(len(contacts), 1)
        contact = contacts[0]
        junction = next(
            i
            for i, owners in enumerate(self.plan["incident"])
            if {self.plan["rows"][a]["source_id"] for a in owners}
            == {"native-part:432", "native-part:535"}
        )
        witness = route_prior_bound(self.plan, contact, junction, "native-part:432")
        self.assertGreater(witness["necessary_prior_error_m"], 2.5)
        target = witness["target_vertex"]
        self.assertIn(target, self.plan["parts"][2])
        p, via, q = contact["xyz_m"][0][:2], witness["via_xy_m"], witness["target_xy_m"]
        lengths = [
            shapely.LineString([p, via]).length,
            shapely.LineString([via, q]).length,
        ]
        expected = abs(witness["target_prior_height_m"] - contact["xyz_m"][0][2])
        expected -= sum(
            a * b
            for a, b in zip(lengths, witness["whole_gradient_limits"], strict=True)
        )
        self.assertAlmostEqual(expected, witness["necessary_prior_error_m"], places=12)
        self.assertIn("single point", contact["provenance"])

    def test_actual_outside_point_is_refused_by_the_unchanged_two_metre_recipe(self):
        contacts = outside_axis_contacts(self.capture, self.proposal["region_context"])
        heights, note = solve_profiles(self.plan, fixed_contacts=contacts)
        self.assertIsNone(heights)
        self.assertIn("Infeasible", note["status"])
        self.assertEqual(note["max_prior_change_budget_m"], 2)

    def test_route_bound_budget_refuses_before_constructing_geometry(self):
        contact = outside_axis_contacts(self.capture, self.proposal["region_context"])[
            0
        ]
        junction = next(
            i
            for i, owners in enumerate(self.plan["incident"])
            if {self.plan["rows"][a]["source_id"] for a in owners}
            == {"native-part:432", "native-part:535"}
        )
        with (
            patch(
                "mvt_t_profile_route_bound.shapely.LineString",
                side_effect=AssertionError,
            ),
            self.assertRaisesRegex(ValueError, "budget"),
        ):
            route_prior_bound(
                self.plan, contact, junction, "native-part:432", max_vertices=0
            )


if __name__ == "__main__":
    unittest.main()
