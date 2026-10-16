"""Actual whole-neighbour contacts, unsafe approaches and full-payload inventory."""

import unittest

import numpy as np
import shapely
from stair_context_experiment import crossings
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_outer_context import catalog
from stair_outer_geometry import bound_contacts, outer_ends
from stair_outer_inputs import load


class StairOuter(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.rows, cls.nodes, cls.stairs, cls.context, cls.recipe = load()
        cls.plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=cls.stairs)
        cls.heights, cls.note = solve(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.note)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)
        cls.pairs, cls.whole = interactions(cls.rows, cls.nodes)

    def test_actual_whole_eighteen_source_body_and_all_six_float32_underpasses(self):
        proof = audit(
            self.data,
            self.plan["domains"],
            limits=[r["gradient"] for r in self.rows],
            crossings=self.pairs,
        )
        self.assertEqual(len(self.rows), 18)
        self.assertEqual(len(self.pairs), 6)
        self.assertEqual(self.geometry["risers"], 24)
        for pair in proof["clearance"].values():
            self.assertGreater(pair["float32"]["minimum_clearance_m"], 2.5)
        vertex_links(self.body.vertices, self.body.triangles)

    def test_both_former_outer_profiles_are_whole_actual_shared_caps(self):
        contacts = bound_contacts(self.data, self.rows)
        self.assertEqual(len(contacts), 2)
        for contact in contacts:
            self.assertTrue(
                all(
                    q["uncovered_length_m"] == 0
                    for q in contact["whole_profile_coverage"]
                )
            )

    def test_a_missing_neighbour_owner_cannot_fake_a_bound_profile(self):
        data = dict(self.data)
        data["face_sources"] = data["face_sources"].copy()
        data["face_sources"] &= ~(1 << 15)
        with self.assertRaisesRegex(ValueError, "lacks an actual shared"):
            bound_contacts(data, self.rows)

    def test_previous_two_knot_approach_fails_with_actual_whole_neighbours(self):
        _case, rows, nodes, stairs, _context, _recipe = load(folded=False)
        h, note = solve(layout(rows, "width", junctions=nodes, stairs=stairs))
        self.assertIsNone(h)
        self.assertIn("infeasible", note["status"].lower())

    def test_early_return_sweeps_the_whole_band_into_the_bridge_clearance(self):
        _case, rows, nodes, stairs, _context, _recipe = load(return_fraction=0.5)
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        h, _note = solve(plan)
        self.assertIsNone(h)
        _h, capacity = solve(plan, gap_m=None)
        self.assertLess(capacity["capacity"]["numerical_dual_upper_gap_m"], 2.0)

    def test_only_the_cycle_approach_axis_changes_and_all_original_nodes_remain(self):
        sources = {q["source_id"]: q for q in self.context["sources"]}
        for i, row in enumerate(self.rows):
            original = np.array(sources[row["source_id"]]["xy_m"])
            if i == 6:
                np.testing.assert_array_equal(row["original_xy_m"], original)
                np.testing.assert_array_equal(row["xy"][:2], original[:2])
                np.testing.assert_array_equal(row["xy"][-1], original[-1])
            else:
                np.testing.assert_allclose(row["xy"], original, rtol=0, atol=1e-8)

    def test_new_frontier_does_not_retain_the_two_bound_original_ports(self):
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(15, 0), (15, -1), (16, 0), (17, 0), (17, -1)],
        )

    def test_declared_whole_contact_regions_are_not_only_axis_node_points(self):
        point = shapely.LineString(self.rows[6]["xy"]).intersection(
            shapely.LineString(self.rows[14]["xy"])
        )
        self.assertEqual(point.geom_type, "Point")
        distance = min(
            shapely.Point(n["xy"]).distance(point)
            for n in self.nodes
            if 6 in n["incident"] and 14 in n["incident"]
        )
        self.assertGreater(distance, 0.1)
        self.assertEqual(len(crossings(self.rows, self.nodes)), 6)
        shared = (self.data["face_sources"] & ((1 << 6) | (1 << 14))) == (
            (1 << 6) | (1 << 14)
        )
        cover = shapely.union_all(
            shapely.polygons(
                self.data["body_vertices"][self.data["top_faces"][shared], :2]
            )
        )
        self.assertLess(cover.distance(point), 1e-7)

    def test_full_original_payload_parts_and_areas_have_explicit_nonadmission(self):
        result = catalog(self.case, self.rows, self.context)
        self.assertEqual(result["original_features"], 572)
        self.assertEqual(result["original_line_parts"], 1657)
        self.assertEqual(len(result["unadmitted_sources"]), 1639)
        self.assertEqual(len(result["unadmitted_non_line_features"]), 71)
        self.assertTrue(all(r["reason"] for r in result["unadmitted_sources"]))
        self.assertTrue(
            any(
                r["source_id"].endswith(":4935280960:0") and r["candidate_overlaps"]
                for r in result["unadmitted_sources"]
            )
        )


if __name__ == "__main__":
    unittest.main()
