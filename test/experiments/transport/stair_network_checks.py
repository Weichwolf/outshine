"""Real three-stair network, source-preserving detour and physical countercases."""

import unittest

import numpy as np
from outshine_ref.contracts import Mesh
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_network_inputs import load
from surface_solid_checks import audit_closed


class StairNetwork(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.rows, cls.nodes, cls.stairs, cls.context, cls.recipe = load()
        cls.plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=cls.stairs)
        cls.heights, cls.note = solve(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.note)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)
        cls.crossings, cls.interactions = interactions(cls.rows, cls.nodes)

    def test_three_actual_stairs_and_every_selected_whole_band_are_valid(self):
        self.assertEqual([s[0] for s in self.stairs], [0, 7, 13])
        self.assertEqual(len(self.rows), 15)
        proof = audit(
            self.data,
            self.plan["domains"],
            limits=[r["gradient"] for r in self.rows],
            crossings=self.crossings,
        )
        self.assertEqual([p["risers"] for p in proof["treads"].values()], [12, 7, 5])
        self.assertEqual(len(proof["clearance"]), 4)
        for p in proof["clearance"].values():
            self.assertGreater(p["float32"]["minimum_clearance_m"], 2.5)
        vertex_links(self.body.vertices, self.body.triangles)

    def test_original_complete_fifteen_source_geometry_cannot_hold_required_gap(self):
        _case, rows, nodes, stairs, _context, _recipe = load(offset=0)
        h, note = solve(layout(rows, "width", junctions=nodes, stairs=stairs))
        self.assertIsNone(h)
        self.assertIn("infeasible", note["status"].lower())

    def test_original_finite_capacity_has_a_checked_numerical_dual_bound(self):
        _case, rows, nodes, stairs, _context, _recipe = load(offset=0)
        _h, note = solve(
            layout(rows, "width", junctions=nodes, stairs=stairs), gap_m=None
        )
        self.assertTrue(note["valid"])
        self.assertLess(note["capacity"]["numerical_dual_upper_gap_m"], 1.68)
        self.assertLess(abs(note["capacity"]["duality_gap"]), 1e-7)

    def test_detour_retains_every_shared_source_node_and_other_fourteen_axes(self):
        sources = {r["source_id"]: r for r in self.context["sources"]}
        for i, row in enumerate(self.rows):
            original = np.array(sources[row["source_id"]]["xy_m"])
            if i != 6:
                np.testing.assert_allclose(row["xy"], original, rtol=0, atol=1e-8)
            else:
                np.testing.assert_array_equal(row["original_xy_m"], original)
                np.testing.assert_array_equal(row["xy"][:2], original[:2])
                np.testing.assert_array_equal(row["xy"][-1], original[-1])
                self.assertGreater(
                    np.linalg.norm(np.diff(row["xy"], axis=0), axis=1).sum(), 50
                )

    def test_short_footway_is_an_explicit_whole_width_contact(self):
        nodes = [n for n in self.nodes if "chosen_overlap_wkt" not in n]
        plan = layout(self.rows, "width", junctions=nodes, stairs=self.stairs)
        with self.assertRaisesRegex(ValueError, "non-contact overlap"):
            solve(plan)

    def test_all_noncontact_overlap_components_are_actually_audited(self):
        self.assertEqual(
            set(self.interactions["noncontact_overlap_areas_m2"]),
            {"1:2", "6:2", "5:9", "11:9"},
        )
        self.assertEqual(len(self.crossings), 4)

    def test_a_stair_cannot_consume_another_stairs_level_range(self):
        data = dict(self.data)
        data["stair_owners"] = np.array([0, 13, 7])
        with self.assertRaises(ValueError):
            audit(
                data,
                self.plan["domains"],
                limits=[r["gradient"] for r in self.rows],
                crossings=self.crossings,
            )

    def test_independent_ipm_meets_same_whole_geometry_and_objective(self):
        h, note = solve(self.plan, solver="highs-ipm")
        self.assertIsNotNone(h, note)
        self.assertAlmostEqual(note["objective"], self.note["objective"], places=5)
        body, data, _geometry = build(self.plan, h)
        audit(
            data,
            self.plan["domains"],
            limits=[r["gradient"] for r in self.rows],
            crossings=self.crossings,
        )
        vertex_links(body.vertices, body.triangles)

    def test_two_edge_closed_solids_touching_only_at_a_vertex_are_rejected(self):
        a = np.array(
            [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
        )
        f = np.array([[0, 2, 1], [0, 1, 3], [0, 3, 2], [1, 2, 3]])
        v = np.vstack([a, -a[1:]])
        second = np.array([0, 4, 5, 6])[f[:, ::-1]]
        body = Mesh(v, np.vstack([f, second]))
        self.assertGreater(audit_closed(body), 0)
        with self.assertRaisesRegex(ValueError, "disconnected vertex link"):
            vertex_links(body.vertices, body.triangles)


if __name__ == "__main__":
    unittest.main()
