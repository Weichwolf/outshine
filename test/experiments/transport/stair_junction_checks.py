"""Real four-way controls and analytic stepped-side closure counterexamples."""

import copy
import unittest

import numpy as np
from outshine_ref.contracts import Mesh
from stair_junction_audit import audit, norm
from stair_junction_body import build
from stair_junction_layout import inputs, layout
from stair_junction_solve import solve
from surface_solid_checks import audit_closed


class StairJunction(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.rows = inputs()
        cls.plan = layout(cls.rows, "width")
        cls.heights, cls.note = solve(cls.plan)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)

    def test_actual_four_way_geometry_and_quantized_clearance(self):
        self.assertTrue(self.note["valid"])
        proof = audit(self.data, self.plan["domains"])
        self.assertGreater(proof["clearance"]["float32"]["minimum_clearance_m"], 2.5)
        self.assertEqual(self.geometry["risers"], 12)
        self.assertLessEqual(self.geometry["rise_m"], 0.18)

    def test_fixed_core_does_not_become_feasible_by_releasing_priors(self):
        p = layout(self.rows, "fixed")
        heights, note = solve(p)
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])

    def test_source_priors_are_not_invented_hard_outer_contacts(self):
        before = copy.deepcopy(self.case)
        heights, note = solve(self.plan, fixed_port_heights=True)
        self.assertIsNone(heights)
        self.assertFalse(note["valid"])
        self.assertEqual(inputs()[0], before)

    def test_independent_interior_point_method_meets_the_same_body_conditions(self):
        heights, note = solve(self.plan, solver="highs-ipm")
        self.assertTrue(note["valid"])
        self.assertAlmostEqual(note["objective"], self.note["objective"], places=5)
        _body, data, _geometry = build(self.plan, heights)
        audit(data, self.plan["domains"])

    def test_metric_axes_match_the_supplied_native_reference_geometry(self):
        original = self.rows[3]["original"]["source"]["local_metres_geometry"][
            "coordinates"
        ]
        np.testing.assert_allclose(self.rows[3]["xy"], original, rtol=0, atol=1e-10)

    def test_real_double_valid_triangle_can_fail_float32_grade(self):
        xyz = np.array(
            [
                [3.7361469878548124, -8.070476569919213, 167.28159894353007],
                [3.343962489733223, -7.7705796763025035, 167.28159894353007],
                [3.342099115766997, -7.771892780503329, 167.28147025020698],
            ]
        )
        self.assertLess(norm(Mesh(xyz, [[0, 1, 2]])), 0.06)
        self.assertGreater(
            norm(Mesh(xyz.astype(np.float32).astype(float), [[0, 1, 2]])), 0.06
        )

    def test_actual_sidewall_vertical_splits_close_two_analytic_treads(self):
        plan = {
            "xy": np.array([[0.0, 0.0], [2.0, 0.0], [2.0, 1.5], [0.0, 1.5]]),
            "parts": [np.array([[0, 1, 2], [0, 2, 3]])],
            "nodes": [[0, 0], [2, 0]],
        }
        body, data, geometry = build(plan, np.array([0.0, 0.34, 0.34, 0.0]))
        self.assertEqual(geometry["risers"], 2)
        self.assertAlmostEqual(audit_closed(body), 1.155)
        self.assertGreater(int(data["wall_riser"].sum()), 0)

    def test_source_footprint_loss_is_rejected_from_saved_cap(self):
        data = dict(self.data)
        data["face_sources"] = data["face_sources"].copy()
        data["face_sources"][np.flatnonzero(data["face_sources"] & 2)[0]] &= ~2
        with self.assertRaisesRegex(ValueError, "whole original footprint"):
            audit(data, self.plan["domains"])


if __name__ == "__main__":
    unittest.main()
