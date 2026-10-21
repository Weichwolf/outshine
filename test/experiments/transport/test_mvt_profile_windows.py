import copy
import json
import unittest

import numpy as np
from compare_mvt_profile_windows import fit_attempts
from mvt_native_cut_profiles import cut_profiles
from mvt_profile_windows import select_window
from mvt_t_connection_inputs import FIXTURE, inputs, source_rows
from mvt_t_connections import plan_connections
from stair_test_transform import rigid_coordinates
from test_mvt_native_body import construct, transform


class BoundedProfileWindows(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = json.loads(
            FIXTURE.with_name("mvt_stair_boundary_neighbours.json").read_text()
        )
        cls.proposal = plan_connections(source_rows(cls.case["ways"]))
        cls.seed = cls.proposal["connections"][0]["source_id"]
        cls.native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        cls.ports = cut_profiles(cls.native)

    def selected(self, **kwargs):
        return select_window(
            self.case,
            self.proposal,
            self.seed,
            padding_m=2,
            interfaces=self.ports,
            **kwargs,
        )

    def test_seven_source_context_requests_four_new_ports_without_inventing_heights(
        self,
    ):
        result = select_window(self.case, self.proposal, self.seed, padding_m=2)
        self.assertEqual(result["status"], "requires_contract_or_expansion")
        self.assertEqual(len(result["profile_requests"]), 4)
        self.assertEqual(len(result["unchanged_source_ids"]), 4)
        for request in result["profile_requests"]:
            self.assertEqual(request["status"], "native_complete_profile_required")
            self.assertNotIn("profile_xyz_m", request)
        self.assertEqual(result["work"]["input_sources"], 7)

    def test_captured_candidate_is_deterministic_under_all_container_permutations(self):
        expected = self.selected()
        case = copy.deepcopy(self.case)
        case["ways"].reverse()
        proposal = copy.deepcopy(self.proposal)
        proposal["connections"].reverse()
        actual = select_window(
            case, proposal, self.seed, padding_m=2, interfaces=self.ports[::-1]
        )
        self.assertEqual(actual, expected)
        self.assertEqual(actual["status"], "captured_candidate")
        self.assertEqual(len(actual["profile_requests"]), 0)
        keys = {
            tuple(key)
            for row in actual["windows"]
            for key in row.get("captured_profile_keys", [])
        }
        self.assertEqual(
            keys, {(p["source_id"], p["source_station_m"]) for p in self.ports}
        )

    def test_source_and_span_caps_return_no_partial_window_or_requests(self):
        for limits in ({"max_sources": 2}, {"max_span_m": 1}):
            result = self.selected(**limits)
            self.assertEqual(result["status"], "deferred")
            self.assertEqual(result["windows"], [])
            self.assertEqual(result["profile_requests"], [])

    def test_source_ends_and_observed_outside_neighbours_are_not_free(self):
        result = select_window(
            self.case, self.proposal, self.seed, padding_m=128, max_span_m=256
        )
        self.assertEqual(result["status"], "requires_contract_or_expansion")
        self.assertTrue(
            any(
                reason.startswith("source_end_context_required")
                for reason in result["reasons"]
            )
        )
        self.assertIn("observed_contact_requires_joint_plan", result["reasons"])
        self.assertTrue(
            all(
                request["status"] == "native_complete_profile_required"
                for request in result["profile_requests"]
            )
        )

    def test_one_accepted_seed_end_does_not_qualify_the_other(self):
        proposal = copy.deepcopy(self.proposal)
        proposal["connections"] = proposal["connections"][:1]
        result = select_window(
            self.case, proposal, self.seed, padding_m=2, interfaces=self.ports
        )
        self.assertIn("seed_endpoint_requires_boundary_contract", result["reasons"])
        self.assertEqual(
            result["windows"][0]["endpoint_roles"]["-1"],
            "unknown_boundary_requires_contract",
        )

    def test_changed_candidate_axis_or_missing_profile_evidence_is_rejected(self):
        for missing in (False, True):
            ports = copy.deepcopy(self.ports)
            if missing:
                del ports[0]["profile_xyz_m"]
            else:
                ports[0]["original_source_xy_m"][0][0] += 0.001
            with self.assertRaises(ValueError):
                select_window(
                    self.case, self.proposal, self.seed, padding_m=2, interfaces=ports
                )

    def test_rigid_coordinates_preserve_station_choices_and_exact_node_ids(self):
        case = copy.deepcopy(self.case)
        coordinates, _rotation = rigid_coordinates(0.437, [37, -81])
        for way in case["ways"]:
            way["source_xy_m"] = coordinates(way["source_xy_m"])
            original = way["sampled_design_xy_height_node"]
            xyz = coordinates([row[:3] for row in original])
            way["sampled_design_xy_height_node"] = [
                [*point, row[3]] for point, row in zip(xyz, original, strict=True)
            ]
        original, _sources = inputs()
        _moved, native = transform(original, self.native, 0.437, [37, -81])
        proposal = plan_connections(source_rows(case["ways"]))
        ports = cut_profiles(native)
        actual = select_window(case, proposal, self.seed, padding_m=2, interfaces=ports)
        expected = self.selected()
        self.assertEqual(actual["status"], expected["status"])
        for before, after in zip(expected["windows"], actual["windows"], strict=True):
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(
                before.get("captured_profile_keys"), after.get("captured_profile_keys")
            )
            np.testing.assert_allclose(
                before["source_station_range_m"],
                after["source_station_range_m"],
                rtol=0,
                atol=1e-10,
            )

    def test_automatically_selected_original_control_ports_build_the_same_local_body(
        self,
    ):
        result = self.selected()
        keys = {
            tuple(key)
            for window in result["windows"]
            for key in window.get("captured_profile_keys", [])
        }
        native = copy.deepcopy(self.native)
        native["cuts"] = [
            cut
            for cut in native["cuts"]
            if (f"native-part:{cut['native_way']}", cut["source_station_m"]) in keys
        ]
        case, _sources = inputs()
        selected = {window["source_id"] for window in result["windows"]}
        self.assertEqual(
            selected, {f"native-part:{way['native_way']}" for way in case["ways"]}
        )
        captured = {
            f"native-part:{way['native_way']}": way for way in self.case["ways"]
        }
        for way in case["ways"]:
            observed = captured[f"native-part:{way['native_way']}"]
            self.assertEqual(way["source_xy_m"], observed["source_xy_m"])
            self.assertEqual(
                way["sampled_design_xyz_m"],
                [row[:3] for row in observed["sampled_design_xy_height_node"]],
            )
        _solver, _plan, _contacts, _heights, data, _geometry, proof, fixed, products = (
            construct(case, native)
        )
        self.assertEqual(data["body_vertices"].shape, (394, 3))
        self.assertEqual(data["body_faces"].shape, (784, 3))
        self.assertEqual(proof["body"]["treads"]["risers"], 18)
        self.assertLess(max(fixed["height_errors_m"].values()), 1e-10)
        self.assertEqual(len(products["external_contacts"]), 4)

    def test_failed_fit_expands_finitely_reuses_known_sides_and_skips_same_boundary_set(
        self,
    ):
        calls = []

        def reject(_case, _native, candidate):
            calls.append(candidate)
            return None, {"valid": False, "reason": "analytic_test_rejection"}

        products, proof = fit_attempts(
            self.case,
            self.native,
            self.proposal,
            self.seed,
            self.ports,
            (2, 4, 8, 16),
            qualifier=reject,
        )
        self.assertIsNone(products)
        self.assertFalse(proof["valid"])
        self.assertEqual(len(calls), 1)
        self.assertEqual(
            [a["status"] for a in proof["attempts"]],
            [
                "fit_rejected",
                "same_rejected_boundaries_skipped",
                "input_required_or_deferred",
            ],
        )
        self.assertEqual(len(proof["attempts"][-1]["selection"]["profile_requests"]), 1)

    def test_observed_identity_is_not_allowed_to_round_through_float64(self):
        case = copy.deepcopy(self.case)
        case["ways"][0]["sampled_design_xy_height_node"][0][3] = float(2**63)
        with self.assertRaisesRegex(ValueError, "uint64"):
            select_window(case, self.proposal, self.seed, padding_m=2)


if __name__ == "__main__":
    unittest.main()
