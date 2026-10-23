import copy
import json
import unittest

import numpy as np
from compare_mvt_profile_windows import fit_attempts
from compare_native_surface_queries import sample_requests
from mvt_native_cut_profiles import cut_profiles
from mvt_native_surface_queries import query_profiles
from mvt_port_alternatives import search_ports
from mvt_t_connection_inputs import FIXTURE, source_rows
from mvt_t_connections import plan_connections


class PortAlternatives(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = json.loads(
            FIXTURE.with_name("mvt_stair_boundary_neighbours.json").read_text()
        )
        native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        crop = json.loads(
            FIXTURE.with_name("mvt_stair_native_surface_crop.json").read_text()
        )
        cls.proposal = plan_connections(source_rows(cls.case["ways"]))
        cls.seed = cls.proposal["connections"][0]["source_id"]
        cls.native, cls.ports, _notes = query_profiles(
            native,
            crop,
            sample_requests(cls.case, cls.proposal, cls.seed, (2, 4, 8, 16)),
        )

    def search(self, **kwargs):
        return search_ports(self.case, self.native, self.proposal, self.seed, **kwargs)

    def test_actual_compact_body_matches_span_order_without_changing_physics(self):
        before = copy.deepcopy((self.case, self.native, self.proposal))
        full, exact = self.search(strategy="span_order", max_fits=64)
        fast, greedy = self.search()
        self.assertTrue(exact["valid"] and greedy["valid"])
        self.assertEqual(exact["selected"], greedy["selected"])
        self.assertLess(greedy["fit_calls"], exact["fit_calls"])
        self.assertAlmostEqual(
            greedy["selected"]["work"]["selected_original_length_m"], 52.343370168729734
        )
        for key in full[0]:
            np.testing.assert_array_equal(full[0][key], fast[0][key])
        self.assertEqual(full[1], fast[1])
        self.assertEqual((self.case, self.native, self.proposal), before)
        _products, old = fit_attempts(
            self.case,
            self.native,
            self.proposal,
            self.seed,
            cut_profiles(self.native),
            (2, 4, 8, 16),
        )
        self.assertFalse(old["valid"])

    def test_fit_budget_retains_last_valid_body_and_never_overruns_calls(self):
        calls = []

        def accept(_case, _native, selection):
            calls.append(selection)
            return selection, {"valid": True}

        product, report = self.search(max_fits=1, qualifier=accept)
        self.assertEqual(len(calls), 1)
        self.assertEqual(report["reason"], "work_budget")
        self.assertTrue(report["valid"])
        self.assertEqual(product, calls[0])
        self.assertEqual(product, report["selected"])
        calls.clear()
        product, report = self.search(max_candidates=1, qualifier=accept)
        self.assertEqual(len(calls), 1)
        self.assertEqual(report["candidates_examined"], 1)
        self.assertTrue(report["valid"])

    def test_failed_outer_does_not_prune_inner_feasibility(self):
        def near_only(_case, _native, selection):
            valid = selection["work"]["selected_original_length_m"] < 30
            return (selection if valid else None), {"valid": valid}

        product, report = self.search(qualifier=near_only)
        self.assertTrue(report["valid"])
        self.assertLess(product["work"]["selected_original_length_m"], 30)
        self.assertEqual(report["fit_calls"], 2)

    def test_one_pass_is_not_catalog_optimality(self):
        def two_islands(_case, _native, selection):
            span = selection["work"]["selected_original_length_m"]
            valid = span < 30 or span > 64
            return (selection if valid else None), {"valid": valid}

        outer, a = self.search(qualifier=two_islands)
        near, b = self.search(strategy="span_order", qualifier=two_islands)
        self.assertTrue(a["valid"] and b["valid"])
        self.assertGreater(
            outer["work"]["selected_original_length_m"],
            near["work"]["selected_original_length_m"],
        )

    def test_catalog_permutation_and_duplicates_do_not_change_order(self):
        def accept(_case, _native, selection):
            return selection, {"valid": True}

        expected, report = self.search(qualifier=accept)
        native = copy.deepcopy(self.native)
        native["cuts"].reverse()
        actual, shuffled = search_ports(
            self.case, native, self.proposal, self.seed, qualifier=accept
        )
        self.assertEqual(expected, actual)
        self.assertEqual(report["fit_calls"], shuffled["fit_calls"])
        native["cuts"].append(native["cuts"][0])
        with self.assertRaises(ValueError):
            search_ports(self.case, native, self.proposal, self.seed, qualifier=accept)

    def test_new_observed_contact_requires_joint_plan_before_fitting(self):
        case = copy.deepcopy(self.case)
        target = next(w for w in case["ways"] if w["native_way"] == 431)
        other = next(w for w in case["ways"] if w["native_way"] == 535)
        xy = target["source_xy_m"][2]
        observation = next(
            r for r in target["sampled_design_xy_height_node"] if r[:2] == xy
        )
        other["source_xy_m"][0] = list(xy)
        other["sampled_design_xy_height_node"].append(copy.deepcopy(observation))

        def outside_contact_only(_case, _native, selection):
            target_window = next(
                w for w in selection["windows"] if w["source_id"] == "native-part:431"
            )
            low, high = target_window["source_station_range_m"]
            self.assertFalse(low <= 21.81241752 <= high)
            return selection, {"valid": True}

        product, report = search_ports(
            case, self.native, self.proposal, self.seed, qualifier=outside_contact_only
        )
        self.assertIsNotNone(product)
        self.assertTrue(report["valid"])
        self.assertEqual(report["fit_calls"], 1)
        first = report["attempts"][0]
        self.assertEqual(first["status"], "contract_or_budget_declined")
        self.assertIn(
            "observed_contact_requires_joint_plan", first["selection"]["reasons"]
        )

    def test_contracts_and_caps_precede_any_physical_attempt(self):
        def forbidden(*_args):
            self.fail("invalid contract must not reach physical qualifier")

        for options in (
            {"max_fits": 0},
            {"max_candidates": True},
            {"strategy": "outermost"},
            {"max_span_m": float("nan")},
        ):
            with self.assertRaises(ValueError):
                self.search(qualifier=forbidden, **options)
        product, report = self.search(max_span_m=10, qualifier=forbidden)
        self.assertIsNone(product)
        self.assertFalse(report["valid"])
        product, report = self.search(
            strategy="span_order", max_candidates=4, qualifier=forbidden
        )
        self.assertIsNone(product)
        self.assertEqual(report["reason"], "work_budget")
        proposal = copy.deepcopy(self.proposal)
        proposal["connections"].pop()
        product, report = search_ports(
            self.case, self.native, proposal, self.seed, qualifier=forbidden
        )
        self.assertIsNone(product)
        self.assertFalse(report["valid"])
        malformed = copy.deepcopy(self.native)
        malformed["cuts"][0]["source_station_m"] += 1
        with self.assertRaises(ValueError):
            search_ports(
                self.case, malformed, self.proposal, self.seed, qualifier=forbidden
            )


if __name__ == "__main__":
    unittest.main()
