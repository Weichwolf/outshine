import copy
import json
import unittest

import numpy as np
from compare_mvt_t_profile_contacts import finite_prior_bound
from mvt_native_cut_domains import cut_ribbon
from mvt_native_cut_layout import prepare_native_cut_plan
from mvt_native_cut_profiles import cut_profiles
from mvt_stair_native_crosscuts import FIXTURE
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_contacts import add_contacts, contact_residuals, prepare_contacts
from mvt_t_profile_solve import solve_profiles
from scipy.optimize import linprog
from stair_junction_solve import Conditions


class NativeCutLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.sources = inputs()
        cls.native = json.loads(FIXTURE.read_text())
        cls.proposal = plan_connections(cls.sources)
        cls.proposal_before = copy.deepcopy(cls.proposal)
        cls.plan, cls.contacts = prepare_native_cut_plan(
            cls.case, cls.sources, cls.proposal, cls.native
        )

    def test_whole_original_sources_and_input_proposal_are_retained(self):
        self.assertEqual(self.plan["native_cut_context"]["original_case"], self.case)
        self.assertEqual(
            self.plan["source_metadata"], {r["source_id"]: r for r in self.sources}
        )
        self.assertEqual(self.proposal, self.proposal_before)
        self.assertEqual(
            [s["source_id"] for s in self.sources],
            ["native-part:240", "native-part:431", "native-part:432"],
        )
        self.assertEqual(len(self.contacts), 4)
        self.assertEqual(len(self.plan["stairs"]), 1)

    def test_all_native_vertices_are_exact_owned_boundary_vertices(self):
        for profile in self.plan["native_cut_context"]["profiles"]:
            owner = next(
                i
                for i, r in enumerate(self.plan["rows"])
                if r["source_id"] == profile["source_id"]
            )
            vertices = {
                tuple(xy)
                for xy in self.plan["xy"][np.unique(self.plan["parts"][owner])]
            }
            boundary = {tuple(xy) for xy in self.plan["domains"][owner].exterior.coords}
            for point in profile["profile_xyz_m"]:
                self.assertIn(tuple(point[:2]), vertices)
                self.assertIn(tuple(point[:2]), boundary)
        for error in self.plan["station_refinement"]["source_coverage_errors_m2"]:
            self.assertLess(error, 1e-7)

    def test_native_width_and_original_cut_stations_are_used(self):
        context = self.plan["native_cut_context"]
        self.assertEqual(
            context["source_scopes"]["native-part:431"]["source_station_range_m"],
            [25, 40],
        )
        self.assertEqual(
            context["source_scopes"]["native-part:432"]["source_station_range_m"],
            [17, 48],
        )
        self.assertTrue(all(r["width_m"] > 2 for r in self.plan["rows"][1:]))
        self.assertTrue(
            all(
                "not a qualified native navstation" in c["provenance"]
                for c in self.contacts
            )
        )

    def test_native_bindings_are_mutually_consistent(self):
        contacts, work = prepare_contacts(self.plan, self.contacts)
        system = Conditions(len(self.plan["xy"]))
        add_contacts(system, contacts, 30, row_scale=1)
        answer = linprog(
            np.zeros(system.variables),
            A_ub=system.matrix(),
            b_ub=system.bounds,
            bounds=[(None, None)] * system.variables,
            method="highs-ds",
        )
        self.assertTrue(answer.success, answer.message)
        self.assertGreater(work["samples"], 32)
        self.assertLess(max(contact_residuals(contacts, answer.x + 30).values()), 1e-10)

    def test_fixed_native_input_proves_two_metre_prior_budget_insufficient(self):
        contacts, _work = prepare_contacts(self.plan, self.contacts)
        lower = max(finite_prior_bound(self.plan, c)["lower_bound_m"] for c in contacts)
        self.assertGreater(lower, 2.79)
        self.assertLess(lower, 2.80)
        heights, note = solve_profiles(self.plan, fixed_contacts=self.contacts)
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])
        self.assertEqual(note["max_prior_change_budget_m"], 2)

    def test_wrong_frame_axis_or_source_is_not_accepted(self):
        for change, message in (
            ("frame", "coordinate frame"),
            ("axis", "original axis"),
            ("source", "retained source parts"),
        ):
            with self.subTest(change=change):
                native = copy.deepcopy(self.native)
                if change == "frame":
                    native["provenance"]["frame_longitude_latitude_deg"][0] += 0.01
                elif change == "axis":
                    native["cuts"][0]["original_source_xy_m"][0][0] += 1
                else:
                    native["cuts"][0]["native_way"] = 999
                with self.assertRaisesRegex(ValueError, message):
                    prepare_native_cut_plan(
                        self.case, self.sources, self.proposal, native
                    )

    def test_cropped_source_needs_both_ordered_interfaces(self):
        native = copy.deepcopy(self.native)
        del native["cuts"][0]
        with self.assertRaisesRegex(ValueError, "two actual interfaces"):
            prepare_native_cut_plan(self.case, self.sources, self.proposal, native)
        first, last = cut_profiles(self.native)[:2]
        with self.assertRaisesRegex(ValueError, "bounded increasing cuts"):
            cut_ribbon(self.proposal["axes_xy_m"][first["source_id"]], last, first)

    def test_changed_target_axis_needs_an_explicit_cut_mapping(self):
        proposal = copy.deepcopy(self.proposal)
        proposal["axes_xy_m"]["native-part:431"][0][0] += 0.1
        with self.assertRaisesRegex(ValueError, "unchanged target axis"):
            prepare_native_cut_plan(self.case, self.sources, proposal, self.native)

    def test_provenance_and_source_data_are_owned_copies(self):
        native = copy.deepcopy(self.native)
        plan, _contacts = prepare_native_cut_plan(
            self.case, self.sources, self.proposal, native
        )
        native["provenance"]["units"] = "wrong"
        self.assertEqual(
            plan["native_cut_context"]["native_provenance"], self.native["provenance"]
        )
        plan["native_cut_context"]["original_case"]["ways"][0]["native_way"] = 999
        self.assertEqual(self.case["ways"][0]["native_way"], 240)
