import copy
import json
import unittest

import numpy as np
from mvt_native_body_layout import prepare_native_body_plan
from mvt_native_body_products import audit_rendered_contacts, consumed_plan
from mvt_t_connection_inputs import FIXTURE, inputs, source_rows
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from stair_junction_body import build
from stair_network_geometry import vertex_links


def construct(case, native):
    sources = source_rows(case["ways"])
    solver_plan, contacts = prepare_native_body_plan(
        case, sources, plan_connections(sources), native
    )
    heights, solve = solve_profiles(
        solver_plan,
        fixed_contacts=contacts,
        max_prior_change_m=3,
        gradient_model="axis_crossfall",
    )
    if heights is None:
        raise ValueError(solve)
    body, data, geometry = build(solver_plan, heights)
    plan, heights = consumed_plan(solver_plan, data)
    proof = audit_profiles(
        plan, heights, data, max_prior_change_m=3, gradient_model="axis_crossfall"
    )
    fixed = audit_rendered_contacts(plan, data, contacts)
    vertex_links(body.vertices, body.triangles)
    products = shared_products(plan, heights, data)
    return solver_plan, plan, contacts, heights, data, geometry, proof, fixed, products


def transform(case, native, angle, translation):
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
    )

    def coordinates(value):
        result = np.array(value)
        result[..., :2] = result[..., :2] @ rotation.T + translation
        return result.tolist()

    case, native = copy.deepcopy(case), copy.deepcopy(native)
    for way in case["ways"]:
        for key in ("source_xy_m", "sampled_design_xyz_m", "prepared_stations_xyz_m"):
            way[key] = coordinates(way[key])
    for cut in native["cuts"]:
        cut["center_xy_m"] = coordinates(cut["center_xy_m"])
        cut["original_source_xy_m"] = coordinates(cut["original_source_xy_m"])
        cut["tangent_xy"] = (np.array(cut["tangent_xy"]) @ rotation.T).tolist()
        for triangle in cut["triangles"]:
            triangle["enu_m"] = coordinates(triangle["enu_m"])
    return case, native


class NativeBoundedBodyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, _sources = inputs()
        cls.native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        (
            cls.solver_plan,
            cls.plan,
            cls.contacts,
            cls.heights,
            cls.data,
            cls.geometry,
            cls.proof,
            cls.fixed,
            cls.products,
        ) = construct(cls.case, cls.native)

    def test_actual_float32_body_has_full_tread_support_and_source_coverage(self):
        body = self.proof["body"]
        self.assertEqual(body["treads"]["risers"], 18)
        self.assertLess(body["treads"]["maximum_float32_riser_m"], 0.18)
        self.assertGreater(body["treads"]["minimum_support_disk_area_m2"], 0)
        self.assertLess(max(body["source_coverage_error_m2"]), 1e-7)
        self.assertGreater(body["volume_m3"], 0)
        self.assertEqual(self.geometry["level_weld_m"], 0)
        self.assertNotIn("float32_edge_cleanup", self.geometry)
        remesh = self.geometry["band_remesh"]
        self.assertLess(remesh["output_faces"], remesh["input_faces"] / 2)
        self.assertLess(remesh["maximum_chain_vertex_deviation_m"], 1e-10)
        self.assertLess(remesh["maximum_loft_height_residual_m"], 1e-8)

    def test_all_native_knots_and_whole_curves_match_the_actual_rendered_cap(self):
        self.assertLess(max(self.fixed["height_errors_m"].values()), 1e-10)
        self.assertGreaterEqual(self.fixed["samples"], 32)
        for profile in self.plan["native_cut_context"]["profiles"]:
            owner = next(
                i
                for i, r in enumerate(self.plan["rows"])
                if r["source_id"] == profile["source_id"]
            )
            top = self.data["top_faces"][
                (self.data["face_sources"] & (1 << owner)) != 0
            ]
            actual = self.data["body_vertices"][np.unique(top)]
            for point in profile["profile_xyz_m"]:
                same = actual[np.all(actual[:, :2] == point[:2], axis=1)]
                self.assertGreater(len(same), 0)
                self.assertLess(abs(same[:, 2] - point[2]).max(), 1e-10)

    def test_whole_norm_and_two_metre_defaults_are_not_silently_relaxed(self):
        with self.assertRaisesRegex(ValueError, "whole profile exceeds"):
            audit_profiles(self.plan, self.heights, self.data, max_prior_change_m=3)
        heights, note = solve_profiles(
            self.solver_plan,
            fixed_contacts=self.contacts,
            gradient_model="axis_crossfall",
        )
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])
        self.assertEqual(note["max_prior_change_budget_m"], 2)

    def test_declared_longitudinal_and_crossfall_limits_hold_in_actual_float32(self):
        for source in self.proof["directional_profiles"].values():
            for field in source.values():
                self.assertLess(field["maximum_longitudinal_m_per_m"], 0.2 + 1e-7)
                self.assertLess(field["maximum_crossfall_m_per_m"], 0.2 + 1e-7)
        self.assertLess(
            self.proof["whole_flight_transverse_gradients"]["native-part:240"], 1e-7
        )

    def test_navigation_is_connected_and_external_ports_keep_original_cut_stations(
        self,
    ):
        navigation = self.products["navigation"]
        edges = navigation["edges"]
        visited = {0}
        while True:
            previous = len(visited)
            for edge in edges:
                if visited.intersection(edge["nodes"]):
                    visited.update(edge["nodes"])
            if len(visited) == previous:
                break
        self.assertEqual(len(visited), len(navigation["nodes_xyz_m"]))
        self.assertEqual(len(visited), 7)
        actual = {
            (p["source_id"], p["source_station_m"])
            for p in self.products["external_contacts"]
        }
        expected = {
            (f"native-part:{c['native_way']}", c["source_station_m"])
            for c in self.native["cuts"]
        }
        self.assertEqual(actual, expected)
        self.assertTrue(
            all("endpoint" not in p for p in self.products["external_contacts"])
        )

    def test_terrain_and_navigation_use_the_reduced_field_and_original_sources(self):
        self.assertEqual(self.products["source_metadata"], self.plan["source_metadata"])
        self.assertEqual(
            self.products["native_cut_context"]["original_case"], self.case
        )
        self.assertEqual(self.products["consumer_sampling_tolerance_m"], 1e-12)
        self.assertEqual(
            len(self.plan["xy"]), self.geometry["band_remesh"]["output_vertices"]
        )
        self.assertEqual(len(self.plan["primary_source"]), len(self.plan["xy"]))
        for reservation, faces in zip(
            self.products["terrain_reservations"], self.plan["parts"], strict=True
        ):
            self.assertEqual(reservation["surface_face_ids"], faces.tolist())

    def test_numeric_sampling_does_not_make_an_outside_navigation_axis_valid(self):
        plan = copy.deepcopy(self.plan)
        plan["rows"][0]["navigation_axis_xy_m"] = plan["rows"][0]["xy"].copy()
        plan["rows"][0]["navigation_axis_xy_m"][0] += [10, 0]
        with self.assertRaisesRegex(ValueError, "outside its input domain"):
            shared_products(plan, self.heights, self.data)
        plan["consumer_sampling_tolerance_m"] = 1e-8
        with self.assertRaisesRegex(ValueError, "bounded explicit numeric"):
            shared_products(plan, self.heights, self.data)

    def test_reversed_source_order_keeps_a_valid_body_and_all_contacts(self):
        case = copy.deepcopy(self.case)
        case["ways"].reverse()
        result = construct(case, self.native)
        self.assertLess(max(result[7]["height_errors_m"].values()), 1e-10)
        self.assertEqual(result[6]["body"]["treads"]["risers"], 18)

    def test_rotated_translated_native_input_keeps_a_valid_body(self):
        case, native = transform(self.case, self.native, 0.437, np.array([37.0, -81.0]))
        result = construct(case, native)
        self.assertLess(max(result[7]["height_errors_m"].values()), 1e-10)
        self.assertEqual(result[6]["body"]["treads"]["risers"], 18)

    def test_curved_stair_is_not_silently_replaced_by_a_straight_flight(self):
        sources = source_rows(self.case["ways"])
        proposal = plan_connections(sources)
        proposal["axes_xy_m"]["native-part:240"][1][0] += 0.1
        with self.assertRaisesRegex(ValueError, "cannot straighten a curved flight"):
            prepare_native_body_plan(self.case, sources, proposal, self.native)
