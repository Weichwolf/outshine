"""Analytic/actual source and physical bridge profile counterexamples."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from cycle_bridge_audit import audit, reference, whole_field
from cycle_bridge_geometry import build
from cycle_bridge_problem import problem
from cycle_bridge_profile import solve
from outshine_ref.contracts import Mesh
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed

FIXTURE = Path(__file__).with_name("fixtures") / "cycle-bridge-vienna.json"


class CycleBridge(unittest.TestCase):
    def test_real_uncushioned_profile_fails_the_actual_float32_grade(self):
        fixture = json.loads(FIXTURE.read_text())
        fixture["recipe"].update(construction_grade_margin=0, clearance_margin_m=0)
        p = problem(fixture)
        h, _ = solve(*(p[k] for k in ("stations", "lower", "upper", "rate")))
        ribbon, note = build(p, h)
        with self.assertRaisesRegex(ValueError, "Float32 whole bridge grade"):
            audit(fixture, ribbon, np.array(note["table"]), p["domain"])

    def test_real_eight_percent_variant_is_rejected_with_unchanged_inputs(self):
        fixture = json.loads(FIXTURE.read_text())
        fixture["recipe"]["maximum_grade"] = 0.08
        p = problem(fixture)
        arrays = [p[k] for k in ("stations", "lower", "upper", "rate")]
        old = [a.copy() for a in arrays]
        for method in ("chain", "lp"):
            result, note = solve(*arrays, method=method)
            self.assertIsNone(result)
            self.assertFalse(note["valid"])
            for a, b in zip(arrays, old, strict=True):
                np.testing.assert_array_equal(a, b)

    def test_asymmetric_derivatives_match_the_identical_linear_program(self):
        rng = np.random.default_rng(404)
        for _ in range(40):
            stations = np.cumsum(rng.uniform(0.1, 5, 12))
            lower = rng.uniform(0, 3, 12)
            upper = np.full(12, 8.0)
            rate = np.sort(rng.uniform(-0.2, 0.3, 2))
            args = stations, lower, upper, rate
            a, _ = solve(*args)
            b, _ = solve(*args, method="lp")
            self.assertEqual(a is None, b is None)
            if a is not None:
                np.testing.assert_allclose(a, b, rtol=0, atol=1e-8)

    def test_positive_required_rise_propagates_without_a_zero_slope_assumption(self):
        h, proof = solve([0, 2, 5], [0, 0, 0], [5, 5, 5], [0.1, 0.2])
        np.testing.assert_allclose(h, [0, 0.2, 0.5], atol=1e-12)
        self.assertTrue(proof["valid"])

    def test_incompatible_contact_and_underpass_does_not_produce_a_profile(self):
        for method in ("chain", "lp"):
            h, proof = solve(
                [0, 1, 2], [0, 2.85, 0], [0, 4, 0], [-0.12, 0.12], method=method
            )
            self.assertIsNone(h)
            self.assertFalse(proof["valid"])

    def test_conflicting_bounds_and_zero_budget_reject_both_paths(self):
        for method in ("chain", "lp"):
            for bounds, seconds in [([0, 0], 2), ([4, 4], 0)]:
                h, proof = solve(
                    [0, 1], [1, 1], bounds, [-0.1, 0.1], method=method, seconds=seconds
                )
                self.assertIsNone(h)
                self.assertFalse(proof["valid"])

    def test_real_ribbon_matches_lp_with_whole_face_grade_and_closed_positive_volume(
        self,
    ):
        p = problem(json.loads(FIXTURE.read_text()))
        args = [p[k] for k in ("stations", "lower", "upper", "rate")]
        h, _ = solve(*args)
        other, _ = solve(*args, method="lp")
        np.testing.assert_allclose(h, other, rtol=0, atol=1e-8)
        result, note = build(p, h)
        self.assertGreater(audit_closed(result.body), 0)
        grad = normal_gradients(
            result.upper.vertices[:, :2],
            result.upper.triangles,
            result.upper.vertices[:, 2],
        )
        self.assertLessEqual(
            np.linalg.norm(grad, axis=1).max(), p["recipe"]["maximum_grade"] + 1e-8
        )
        self.assertLess(note["coverage_error_m2"], 1e-7)
        xyz = result.body.vertices.astype(np.float32).astype(float)[
            result.body.triangles
        ]
        self.assertTrue(
            np.all(
                np.linalg.norm(
                    np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0]), axis=1
                )
                > 0
            )
        )

    def test_correct_vertex_heights_do_not_prove_a_whole_profile_kink(self):
        fixture = json.loads(FIXTURE.read_text())
        p = problem(fixture)
        h, _ = solve(*(p[k] for k in ("stations", "lower", "upper", "rate")))
        _ribbon, note = build(p, h)
        s = np.array([0.0, 100.0, 220.0])
        normal = np.array([-p["axis"][1], p["axis"][0]])
        xy = (
            p["origin"]
            + s[:, None] * p["axis"]
            + np.array([0.0, 0.5, 0.0])[:, None] * normal
        )
        z = (
            p["reference"][2]
            + (xy - p["reference"][:2]) @ p["gradient"]
            + np.interp(s, p["stations"], h)
        )
        with self.assertRaisesRegex(ValueError, "whole bridge cap field"):
            whole_field(
                Mesh(np.c_[xy, z], [[0, 2, 1]]),
                np.array(note["table"]),
                reference(fixture),
            )

    def test_private_bridge_cannot_be_promoted_to_a_public_cycle_route(self):
        fixture = json.loads(FIXTURE.read_text())
        fixture["sources"][0]["properties"]["access"] = "private"
        fixture["sources"][0]["properties"].pop("foot", None)
        with self.assertRaisesRegex(ValueError, "public cycle"):
            problem(fixture)

    def test_crossing_role_and_field_coverage_are_not_trusted_fixture_assertions(self):
        fixture = json.loads(FIXTURE.read_text())
        changed = copy.deepcopy(fixture)
        next(r for r in changed["sources"] if r["role"] == "underpass")["role"] = (
            "ground_joint"
        )
        with self.assertRaisesRegex(ValueError, "topology/level"):
            problem(changed)
        fixture["field_vertices"][0][0] += 3000
        with self.assertRaisesRegex(ValueError, "cover"):
            problem(fixture)

    def test_active_body_does_not_cover_the_flat_unchanged_end_sections(self):
        p = problem(json.loads(FIXTURE.read_text()))
        h, _ = solve(*(p[k] for k in ("stations", "lower", "upper", "rate")))
        result, _ = build(p, h)
        self.assertGreater(p["domain"].difference(result.shape).area, 100)
        self.assertTrue(shapely.covered_by(result.shape, p["domain"].buffer(1e-8)))


if __name__ == "__main__":
    unittest.main()
