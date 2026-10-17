"""Original-node end/through contacts, immutable caps and whole two-sided traces."""

import copy
import unittest

import numpy as np
import shapely
from stair_frontier_endpoints import endpoint_bindings
from stair_module_cache import remaining_outer_profiles
from stair_module_loops import frozen_contacts, interior_profile, prepare_contacts
from stair_module_through import through_sides
from stair_module_trace import compare


def original_end():
    xy = np.array([[0, -1], [10, -1], [10, 1], [0, 1]], float)
    xyz = np.c_[xy, xy @ [0.01, 0.02]]
    sources = {
        "old": {"mvt_xy": [[0, 0], [5, 0], [10, 0]], "xy_m": [[0, 0], [5, 0], [10, 0]]},
        "new": {
            "mvt_xy": [[0, -10], [0, 0], [0, 10]],
            "xy_m": [[0, -10], [0, 0], [0, 10]],
        },
    }
    state = {
        "cached_module_rows": {"old": {"xy": sources["old"]["xy_m"], "width_m": 2}},
        "road": {
            "body_vertices": xyz,
            "top_faces": np.array([[0, 1, 2], [0, 2, 3]]),
            "face_sources": np.ones(2, np.uint32),
        },
        "road_ground": np.ones(2, bool),
        "report": {"candidate_source_ids": ["old"]},
    }
    recipe = {
        "contact_mode": "profile",
        "source_ids": ["new"],
        "endpoint_contacts": endpoint_bindings(
            state, sources, {"node": [9, 9]}, ["new"]
        ),
    }
    return state, sources, recipe


class EndThroughContacts(unittest.TestCase):
    def test_exact_old_end_new_interior_and_complete_pad_without_old_mutation(self):
        state, sources, recipe = original_end()
        before = state["road"]["body_vertices"].copy()
        contacts = frozen_contacts(state, recipe, sources)
        self.assertEqual(contacts[0]["vertex"], 1)
        self.assertFalse(contacts[0]["cached_interior"])
        np.testing.assert_allclose(contacts[0]["profile"][:, 2], [-0.02, 0, 0.02])
        rows = [
            {"source_id": "new", "xy": np.array(sources["new"]["xy_m"]), "width_m": 2}
        ]
        nodes = []
        prepare_contacts(rows, nodes, contacts)
        self.assertEqual(rows[0]["junction_pad_vertices"], [1])
        pad = shapely.Polygon(rows[0]["junction_pads_xy_m"][0])
        self.assertTrue(pad.covers(shapely.LineString([[-1, 0], [1, 0]])))
        self.assertTrue(pad.covers(shapely.LineString([[0, -1], [0, 1]])))
        np.testing.assert_array_equal(state["road"]["body_vertices"], before)

    def test_nearby_coordinate_does_not_create_original_node_connectivity(self):
        state, sources, recipe = original_end()
        sources["new"]["mvt_xy"][1] = [0, 1]
        self.assertEqual(
            endpoint_bindings(state, sources, {"node": [9, 9]}, ["new"]), []
        )
        with self.assertRaisesRegex(ValueError, "exact shared original node"):
            frozen_contacts(state, recipe, sources)

    def test_endpoint_kind_ground_frame_and_unique_node_are_required(self):
        original = original_end()
        for change in ("kind", "ground", "frame", "repeat"):
            state, sources, recipe = copy.deepcopy(original)
            if change == "kind":
                recipe["endpoint_contacts"][0]["cached_vertex"] = 1
            elif change == "ground":
                state["road_ground"][:] = False
            elif change == "frame":
                sources["new"]["xy_m"][1][0] = 0.001
            else:
                sources["new"]["mvt_xy"][2] = [0, 0]
            with self.subTest(change=change), self.assertRaises(ValueError):
                frozen_contacts(state, recipe, sources)
        with self.assertRaises(ValueError):
            interior_profile(original[0], original[1], "old", 0)

    def test_secondary_contacts_keep_one_combined_four_contact_limit(self):
        state, sources, recipe = original_end()
        recipe["endpoint_contacts"] *= 5
        with self.assertRaisesRegex(ValueError, "bounded explicit"):
            frozen_contacts(state, recipe, sources)

    def test_consumed_endpoint_leaves_other_old_end_and_all_unrelated_ports(self):
        _state, _sources, recipe = original_end()
        recipe["port"] = {"source_id": "root", "vertex": 1}
        profiles = [
            {"source_id": key, "end": end}
            for key in ("old", "other")
            for end in (0, -1)
        ]
        remaining = remaining_outer_profiles(profiles, recipe)
        self.assertEqual(remaining, profiles[1:])
        self.assertEqual(len(profiles), 4)


class WholeThroughTraces(unittest.TestCase):
    @staticmethod
    def caps(*, gap=False, step=False, one_side=False):
        groups = []
        for side in [1] if one_side else [-1, 1]:
            near = 0.001 * side if gap else 0
            xy = np.array([[-1, near], [1, near], [1, side], [-1, side]], float)
            xyz = np.c_[xy, xy[:, 0] * 0.02 + (0.01 if step and side > 0 else 0)]
            faces = np.array([[0, 1, 2], [0, 2, 3]])
            groups.append((xyz, faces, None))
        return groups

    def test_both_actual_sides_cover_and_agree_over_every_trace_interval(self):
        line = np.array([[-1, 0], [0, 0], [1, 0]], float)
        groups = through_sides(self.caps(), line)
        for cast in (False, True):
            proof = compare(groups, line, cast)
            self.assertGreater(proof["intervals"], 0)
            self.assertLessEqual(
                proof["maximum_height_disagreement_m"], proof["z_guard_m"]
            )

    def test_gap_step_and_line_only_contact_cannot_prove_through_continuity(self):
        line = np.array([[-1, 0], [0, 0], [1, 0]], float)
        for name in ("gap", "step", "one_side"):
            for cast in (False, True):
                with self.subTest(case=name, cast=cast), self.assertRaises(ValueError):
                    compare(through_sides(self.caps(**{name: True}), line), line, cast)


if __name__ == "__main__":
    unittest.main()
