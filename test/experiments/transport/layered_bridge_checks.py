"""Analytic and real counterexamples for interval grades, whole fields and source flow."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from cycle_bridge_profile import solve
from layered_bridge_audit import field_error
from layered_bridge_plan import candidate
from layered_bridge_problem import prepare
from layered_bridge_supports import air_intervals, layout
from layered_bridge_topology import topology
from outshine_ref.contracts import Mesh

FIXTURE = Path(__file__).with_name("fixtures") / "layered-bridges-vienna.json"


class LayeredBridge(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = json.loads(FIXTURE.read_text())
        cls.lower, cls.lower_note = candidate(cls.fixture, "path")
        cls.upper, cls.upper_note = candidate(cls.fixture, "motorway", lower=cls.lower)
        if cls.upper is None or cls.lower is None:
            raise AssertionError("actual source candidate absent")
        cls.chains, cls.travel = topology(cls.fixture, cls.upper[0]["axis"])

    def test_different_edge_derivatives_propagate_the_right_bounds(self):
        args = ([0, 1, 2], [0, 4, 0], [10, 10, 10], [[-0.1, 0.1], [-2, 2]])
        for method in ("chain", "lp"):
            h, n = solve(*args, method=method)
            self.assertTrue(n["valid"])
            np.testing.assert_allclose(h, [3.9, 4, 2], rtol=0, atol=1e-8)

    def test_seeded_variable_interval_chains_match_the_independent_lp(self):
        rng = np.random.default_rng(419)
        for _ in range(40):
            s = np.cumsum(rng.uniform(0.2, 3, 8))
            lo = rng.uniform(0, 2, 8)
            hi = np.full(8, 4.0)
            rate = np.c_[-rng.uniform(0.01, 0.4, 7), rng.uniform(0.01, 0.6, 7)]
            h, _ = solve(s, lo, hi, rate)
            other, _ = solve(s, lo, hi, rate, method="lp")
            np.testing.assert_allclose(h, other, rtol=0, atol=1e-8)

    def test_actual_two_profile_component_passes_whole_float32_fields_and_clearance(
        self,
    ):
        for row in (self.lower_note, self.upper_note):
            self.assertTrue(row["valid"])
            self.assertLessEqual(row["physical"]["float32_section_ratio"], 1 + 1e-7)
            self.assertLess(row["physical"]["float32_upper_field_error_m"], 0.001)
        self.assertGreaterEqual(
            self.upper_note["physical"]["float32_lower_traffic_clearance"][
                "minimum_clearance_m"
            ],
            4.5 - 1e-7,
        )

    def test_bridge_metadata_does_not_reset_runout_heights_to_ground(self):
        p, h, *_ = self.upper
        self.assertGreater(np.interp(0, p["stations"], h), 5)
        self.assertEqual(len(self.fixture["field_faces"]), 6)
        self.assertEqual(sum(r["role"] == "deck" for r in self.fixture["sources"]), 7)

    def test_unchosen_tunnel_context_gets_no_invented_height(self):
        row = next(
            r
            for r in self.fixture["context"]
            if r["properties"].get("brunnel") == "tunnel"
        )
        self.assertEqual(row["old_decision"]["status"], "not_used")
        self.assertFalse(
            any(r["source_id"] == row["source_id"] for r in self.fixture["sources"])
        )

    def test_whole_ground_cells_are_required(self):
        f = copy.deepcopy(self.fixture)
        xyz = np.array(f["field_vertices"])
        domain = self.upper[0]["domain"]
        index = next(
            i
            for i, face in enumerate(f["field_faces"])
            if shapely.Polygon(xyz[face, :2]).intersection(domain).area > 1e-6
        )
        f["field_faces"].pop(index)
        f["original_field_face_ids"].pop(index)
        with self.assertRaisesRegex(ValueError, "actual captured affine ground"):
            prepare(f, "motorway")

    def test_correct_vertices_do_not_prove_a_piecewise_ground_kink(self):
        field = Mesh(
            [[0, 0, 0], [4, 0, 0], [4, 4, 4], [0, 4, 0]], [[0, 1, 3], [1, 2, 3]]
        )
        cap = Mesh(field.vertices + [0, 0, 1], [[0, 1, 2], [0, 2, 3]])
        p = {
            "origin": np.zeros(2),
            "axis": np.array([1.0, 0.0]),
            "recipe": {"deck_thickness_m": 0.25},
        }
        with self.assertRaisesRegex(ValueError, "whole affine field"):
            field_error(cap, np.array([[0, 1], [4, 1]]), p, field)

    def test_air_interval_does_not_gain_ghost_supports_at_profile_knots(self):
        line = shapely.LineString([[0, 0], [100, 0]])
        p = {
            "origin": np.zeros(2),
            "axis": np.array([1.0, 0.0]),
            "recipe": {"deck_thickness_m": 0.5},
        }
        table = np.array([[0, 0.5], [20, 2], [40, 2.5], [60, 2], [100, 0.5]])
        self.assertEqual(air_intervals(line, p, table), [(0.0, 100.0)])

    def test_actual_fixed_piers_do_not_hide_omissions_as_abutments(self):
        _, fixed = layout(
            self.fixture, self.upper, self.chains, lower=self.lower, method="fixed"
        )
        self.assertFalse(fixed["valid"])
        self.assertGreater(fixed["maximum_air_span_m"], 36)
        _, shifted = layout(
            self.fixture, self.upper, self.chains, lower=self.lower, method="shifted"
        )
        self.assertTrue(shifted["valid"])
        self.assertLessEqual(shifted["maximum_air_span_m"], 36)

    def test_source_direction_is_not_inferred_from_physical_slab_contact(self):
        self.assertEqual(self.travel["logical_cross_direction_connections"], 0)
        self.assertEqual(self.travel["opposite_core_overlap_m2"], 0)
        self.assertEqual(len(self.chains), 2)
        f = copy.deepcopy(self.fixture)
        f["sources"][0]["properties"]["oneway"] = 0
        with self.assertRaisesRegex(ValueError, "one-way travel"):
            topology(f, self.upper[0]["axis"])

    def test_opposite_traffic_cores_cannot_overlap(self):
        f = copy.deepcopy(self.fixture)
        axis = self.upper[0]["axis"]
        normal = np.array([-axis[1], axis[0]])
        for row in f["sources"]:
            xy = np.array(row["coordinates"])
            if row["profile"] == "motorway" and (xy[-1] - xy[0]) @ axis < 0:
                row["coordinates"] = (xy - 12 * normal).tolist()
        with self.assertRaisesRegex(ValueError, "traffic cores overlap"):
            topology(f, axis)

    def test_conflicting_optional_profile_leaves_original_source_inputs_unchanged(self):
        f = copy.deepcopy(self.fixture)
        before = copy.deepcopy(f)
        p = prepare(
            f,
            "motorway",
            [
                {
                    "source_id": "chosen:impossible",
                    "kind": "maximum",
                    "value_m": 0,
                    "range_m": [100, 101],
                }
            ],
        )
        h, n = solve(*(p[k] for k in ("stations", "lower", "upper", "rate")))
        self.assertIsNone(h)
        self.assertFalse(n["valid"])
        self.assertEqual(f, before)


if __name__ == "__main__":
    unittest.main()
