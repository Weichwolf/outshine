import copy
import json
import time
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from mvt_native_body_layout import prepare_native_body_plan
from mvt_native_body_products import audit_rendered_contacts, consumed_plan
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_solve import solve_profiles
from mvt_t_stair_loft import height_projection
from scipy.optimize import linprog
from stair_junction_body import build
from stair_profile_quality import path_creases
from stair_profile_variation import select_variation


class ProfileVariationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        case, sources = inputs()
        native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        cls.plan, cls.contacts = prepare_native_body_plan(
            case, sources, plan_connections(sources), native
        )
        cls.options = {
            "fixed_contacts": cls.contacts,
            "max_prior_change_m": 3,
            "gradient_model": "axis_crossfall",
        }
        cls.captured = {}

        def capture(objective, **kwargs):
            answer = linprog(objective, **kwargs)
            cls.captured.update(kwargs, answer=answer)
            return answer

        with patch("mvt_t_profile_solve.linprog", side_effect=capture):
            cls.original, cls.primary = solve_profiles(cls.plan, **cls.options)
        cls.selected, cls.note = solve_profiles(
            cls.plan, selection="minimax_then_variation", **cls.options
        )

    def select(self, *, plan=None, deadline=None, max_edges=1024):
        captured = self.captured
        projection = height_projection(
            self.plan, len(self.plan["xy"]) + 3 * len(self.plan["nodes"]) + 1
        )
        return select_variation(
            self.plan if plan is None else plan,
            captured["A_ub"],
            captured["b_ub"],
            projection,
            captured["answer"].x,
            captured["answer"].fun,
            captured["bounds"],
            "highs-ds",
            time.perf_counter() + 5 if deadline is None else deadline,
            max_edges=max_edges,
        )

    def test_default_is_identical_and_secondary_preserves_primary_objective(self):
        explicit, note = solve_profiles(self.plan, selection="minimax", **self.options)
        np.testing.assert_array_equal(explicit, self.original)
        self.assertNotIn("selection_report", note)
        self.assertTrue(self.note["selection_report"]["selected"])
        self.assertEqual(self.note["objective"], self.primary["objective"])
        self.assertLessEqual(
            self.note["max_prior_change_m"], self.primary["objective"] + 1e-10
        )
        self.assertLess(self.note["maximum_constraint_residual"], 1e-7)

    def test_actual_consumed_mesh_is_smoother_with_unchanged_contacts_and_body_gates(
        self,
    ):
        measurements = []
        for heights in (self.original, self.selected):
            _body, data, _geometry = build(self.plan, heights)
            plan, field = consumed_plan(self.plan, data)
            proof = audit_profiles(
                plan, field, data, max_prior_change_m=3, gradient_model="axis_crossfall"
            )
            fixed = audit_rendered_contacts(plan, data, self.contacts)
            self.assertLess(max(fixed["height_errors_m"].values()), 1e-10)
            self.assertLess(proof["body"]["treads"]["maximum_float32_riser_m"], 0.18)
            self.assertGreater(
                proof["body"]["treads"]["minimum_support_disk_area_m2"], 0
            )
            measurements.append(path_creases(plan, field))
        for before, after in zip(*measurements, strict=True):
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertLess(
                after["maximum_dihedral_degrees"], before["maximum_dihedral_degrees"]
            )
            self.assertLess(
                after["integrated_gradient_jump_m"],
                before["integrated_gradient_jump_m"],
            )

    def test_budget_fallbacks_return_the_original_valid_solution(self):
        for options, status in (
            ({"max_edges": 0}, "edge_budget"),
            ({"deadline": 0}, "time_budget"),
        ):
            with (
                self.subTest(status=status),
                patch("stair_profile_variation.linprog") as solve,
            ):
                selected, note = self.select(**options)
                self.assertIs(selected, self.captured["answer"].x)
                self.assertEqual(note["status"], status)
                self.assertFalse(note["selected"])
                solve.assert_not_called()

    def test_secondary_keeps_every_primary_inequality_and_bound(self):
        primary = self.captured

        def inspect(objective, **kwargs):
            rows, columns = primary["A_ub"].shape
            self.assertEqual(
                (kwargs["A_ub"][:rows, :columns] != primary["A_ub"]).nnz, 0
            )
            np.testing.assert_array_equal(kwargs["b_ub"][:rows], primary["b_ub"])
            self.assertEqual(kwargs["bounds"][: columns - 1], primary["bounds"][:-1])
            self.assertEqual(kwargs["bounds"][columns - 1], (0, primary["answer"].fun))
            np.testing.assert_array_equal(objective[:columns], np.zeros(columns))
            return linprog(objective, **kwargs)

        with patch("stair_profile_variation.linprog", side_effect=inspect):
            _selected, note = self.select()
        self.assertTrue(note["selected"])

    def test_unsuccessful_or_nonfinite_secondary_preserves_original(self):
        invalid = SimpleNamespace(success=False, message="bounded secondary stopped")
        with patch("stair_profile_variation.linprog", return_value=invalid):
            selected, note = self.select()
        self.assertIs(selected, self.captured["answer"].x)
        self.assertFalse(note["selected"])
        invalid = SimpleNamespace(
            success=True,
            message="bad",
            x=np.full(self.note["selection_report"]["variables"], np.nan),
        )
        with patch("stair_profile_variation.linprog", return_value=invalid):
            selected, note = self.select()
        self.assertIs(selected, self.captured["answer"].x)
        self.assertEqual(note["status"], "nonfinite_candidate")

    def test_primary_failure_does_not_attempt_secondary(self):
        options = dict(self.options, max_prior_change_m=2)
        with patch("stair_profile_variation.linprog") as secondary:
            heights, note = solve_profiles(
                self.plan, selection="minimax_then_variation", **options
            )
        self.assertIsNone(heights)
        self.assertEqual(note["selection_report"]["status"], "primary_not_valid")
        secondary.assert_not_called()

    def test_nonmanifold_caps_are_rejected(self):
        plan = copy.deepcopy(self.plan)
        owner = next(
            i
            for i in range(len(plan["parts"]))
            if i not in {s[0] for s in plan["stairs"]}
        )
        face = plan["parts"][owner][0]
        plan["parts"][owner] = np.tile(face, (3, 1))
        with self.assertRaisesRegex(ValueError, "manifold"):
            self.select(plan=plan)

    def test_independent_metric_matches_analytic_two_facet_fold(self):
        plan = {
            "xy": np.array([[0.0, 0.0], [1.0, 0.0], [0.0, 1.0], [1.0, 1.0]]),
            "parts": [np.array([[0, 1, 2], [1, 3, 2]])],
            "rows": [{"source_id": "analytic"}],
            "stairs": [],
        }
        result = path_creases(plan, np.array([0.0, 0.0, 0.0, 1.0]))[0]
        self.assertAlmostEqual(result["integrated_gradient_jump_m"], 2)
        self.assertAlmostEqual(
            result["maximum_dihedral_degrees"], np.rad2deg(np.arccos(1 / np.sqrt(3)))
        )


if __name__ == "__main__":
    unittest.main()
