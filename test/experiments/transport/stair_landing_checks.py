"""Whole chosen endpoint contacts, immutable profile primitive and local budgets."""

import copy
import unittest

import numpy as np
from shared_bridge_geometry import corridor
from stair_corner_checks import CORNER, toy
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_landing_contacts import node_profiles
from stair_landing_pads import chosen_kinds, complete_landings
from stair_network_geometry import interactions, vertex_links

LANDINGS = FIXTURE.with_name("stair-landings-vienna.json")


class LandingPolicy(unittest.TestCase):
    def test_unchosen_nodes_and_budget_end_leave_originals_available(self):
        rows, nodes, _ = toy()
        original = copy.deepcopy(rows)
        for cap, original_node in ((0, True), (16, False)):
            rows = copy.deepcopy(original)
            nodes[0]["mvt_xy"] = [1, 2] if original_node else None
            pads, ledger = complete_landings(rows, nodes, [], max_pads=cap)
            self.assertEqual(pads, [])
            self.assertEqual(len(ledger), 1)
            self.assertEqual(
                ledger[0]["reason"],
                "chosen_landing_pad_budget"
                if original_node
                else "no_original_coincident_endpoint_node",
            )
            for before, after in zip(original, rows, strict=True):
                np.testing.assert_array_equal(before["xy"], after["xy"])
                self.assertEqual(before["original"], after["original"])
                self.assertNotIn("junction_pads_xy_m", after)

    def test_real_selected_bridge_level_and_explicit_stair_landing_are_distinct(self):
        _, rows, nodes, stairs, _, _ = load(CORNER)
        for index, node in enumerate(nodes):
            if 9 in node["incident"]:
                self.assertEqual(chosen_kinds(rows, [(9, 0)], index, stairs), "")
                changed = copy.deepcopy(rows)
                changed[9]["original"]["bridge"] = False
                self.assertEqual(
                    chosen_kinds(changed, [(9, 0)], index, stairs),
                    "unsupported_source_kind_or_level",
                )
                break
        self.assertEqual(
            chosen_kinds(rows, [(7, 0)], -1, stairs),
            "stair_endpoint_not_a_chosen_landing",
        )
        for props in ({"indoor": 1}, {"brunnel": "tunnel"}):
            changed = copy.deepcopy(rows)
            changed[9]["original"]["source"]["feature"]["properties"].update(props)
            self.assertEqual(
                chosen_kinds(changed, [(9, 0)], index, stairs),
                "unsupported_source_kind_or_level",
            )

    def test_already_complete_edge_join_gets_no_extra_pad(self):
        rows, nodes, _ = toy()
        rows[0]["width_m"] = rows[1]["width_m"] = 2
        rows[1]["xy"] = np.array([[0, 0], [10, 0]], float)
        pads, ledger = complete_landings(rows, nodes, [])
        self.assertEqual(pads, [])
        self.assertEqual(
            ledger[0]["reason"], "whole_profiles_already_supported_by_neighbours"
        )

    def test_real_box_recipe_is_rejected_for_two_unplanned_band_overlaps(self):
        _, rows, nodes, stairs, _, _ = load(CORNER)
        pads, _ = complete_landings(rows, nodes, stairs, method="profile_box")
        self.assertEqual(len(pads), 9)
        with self.assertRaisesRegex(ValueError, "non-axis band overlap"):
            interactions(rows, nodes)

    def test_whole_shared_edge_requires_matching_actual_owned_surface_heights(self):
        rows, nodes, _ = toy()
        rows[0]["xy"] = np.array([[-2, 0], [0, 0]], float)
        rows[1]["xy"] = np.array([[0, 0], [2, 0]], float)
        rows[0]["width_m"] = rows[1]["width_m"] = 2
        xy = np.array([[-2, -1], [-2, 1], [0, -1], [0, 1], [2, -1], [2, 1]], float)
        data = {
            "body_vertices": np.c_[xy, np.full(6, 165)],
            "top_faces": np.array([[0, 2, 3], [0, 3, 1], [2, 4, 5], [2, 5, 3]]),
            "face_sources": np.array([1, 1, 2, 2]),
        }
        proof = node_profiles(data, rows, nodes)
        self.assertEqual(proof["complete_internal_profiles"], 2)
        self.assertTrue(proof["all_chosen_internal_profiles_whole"])
        changed = dict(data)
        shifted = data["body_vertices"].copy()
        shifted[:, 2] += 0.01
        changed["body_vertices"] = np.vstack([data["body_vertices"], shifted])
        changed["top_faces"] = data["top_faces"].copy()
        changed["top_faces"][2:] += 6
        self.assertFalse(
            node_profiles(changed, rows, nodes)["all_chosen_internal_profiles_whole"]
        )


class ActualLandings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.rows, cls.nodes, stairs, cls.context, _ = load(LANDINGS)
        p = layout(cls.rows, "width", junctions=cls.nodes, stairs=stairs)
        heights, note = solve(p)
        if heights is None:
            raise ValueError("whole chosen landing fit failed: " + str(note))
        _, cls.data, _ = build(p, heights)

    def test_all_previous_axes_priors_and_derived_source_bands_are_preserved(self):
        _, previous, _, _, _, _ = load(CORNER)
        for before, after in zip(previous, self.rows, strict=True):
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(before["original"], after["original"])
            np.testing.assert_array_equal(before["xy"], after["xy"])
            np.testing.assert_array_equal(before["prior"], after["prior"])
            self.assertLess(corridor(before).difference(corridor(after)).area, 1e-8)
        self.assertEqual(len(self.context["landing_pads"]), 9)

    def test_all_forty_one_internal_end_profiles_are_whole_in_both_precisions(self):
        proof = node_profiles(self.data, self.rows, self.nodes)
        self.assertTrue(proof["all_chosen_internal_profiles_whole"])
        self.assertEqual(proof["complete_internal_profiles"], 41)
        self.assertEqual(len(proof["outer_profiles"]), 7)
        self.assertEqual(proof["unresolved_internal_profiles"], [])
        self.assertEqual(
            len(contacts(self.data, self.rows, bindings(LANDINGS, self.rows))), 12
        )

    def test_missing_source_ownership_or_incorrect_actual_contact_height_fails(self):
        changed = dict(self.data)
        changed["face_sources"] = self.data["face_sources"] & ~(1 << 23)
        self.assertFalse(
            node_profiles(changed, self.rows, self.nodes)[
                "all_chosen_internal_profiles_whole"
            ]
        )
        changed = dict(self.data)
        changed["body_vertices"] = self.data["body_vertices"].copy()
        point = self.rows[23]["xy"][0]
        vertex = np.argmin(
            np.linalg.norm(changed["body_vertices"][:, :2] - point, axis=1)
        )
        changed["body_vertices"][vertex, 2] += 0.01
        self.assertFalse(
            node_profiles(changed, self.rows, self.nodes)[
                "all_chosen_internal_profiles_whole"
            ]
        )

    def test_original_underpasses_stairs_and_closed_vertex_links_remain(self):
        pairs, _ = interactions(self.rows, self.nodes)
        proof = audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        self.assertEqual(len(proof["clearance"]), 10)
        self.assertEqual(sum(r["risers"] for r in proof["treads"].values()), 24)
        vertex_links(self.data["body_vertices"], self.data["body_faces"])


if __name__ == "__main__":
    unittest.main()
