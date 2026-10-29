"""Original source/frame guards and complete primary old-end/new-through profiles."""

import copy
import unittest

import numpy as np
import shapely
from stair_module_profiles import new_interior_checks
from stair_module_roots import bind_root_pad
from stair_module_source_contract import verify_source_contract
from stair_root_contact import new_through_root, profile_interiors


def original_contact():
    sources = {
        "old": {"mvt_xy": [[0, 0], [5, 0]], "xy_m": [[0, 0], [5, 0]]},
        "new": {
            "mvt_xy": [[0, -5], [0, 0], [0, 5]],
            "xy_m": [[0, -5], [0, 0], [0, 5]],
        },
    }
    recipe = {
        "contact_mode": "profile",
        "port": {"source_id": "old", "end": 0},
        "source_ids": ["new"],
    }
    row = {
        "source_id": "new",
        "xy": np.array(sources["new"]["xy_m"], float),
        "width_m": 2,
    }
    node = {"mvt_xy": [0, 0], "xy": [0, 0], "incident": [0]}
    line = np.array([[0, -1, -0.02], [0, 0, 0], [0, 1, 0.02]])
    return recipe, sources, row, node, line


class PrimaryThroughContact(unittest.TestCase):
    def test_role_comes_from_unique_original_occurrences_in_the_same_frame(self):
        recipe, sources, *_ = original_contact()
        self.assertTrue(new_through_root(recipe, sources))
        self.assertTrue(profile_interiors(recipe, sources))
        recipe["port"]["end"] = -1
        self.assertFalse(new_through_root(recipe, sources))
        self.assertFalse(profile_interiors(recipe, sources))

    def test_full_root_hull_is_reconstructed_from_original_sections(self):
        recipe, sources, row, node, line = original_contact()
        bind_root_pad(
            [row],
            node,
            line,
            include_interiors=True,
            allow_through=new_through_root(recipe, sources),
        )
        self.assertEqual(row["junction_pad_vertices"], [1])
        self.assertTrue(
            shapely.Polygon(row["junction_pads_xy_m"][0]).equals(
                shapely.Polygon([[-1, 0], [0, 1], [1, 0], [0, -1]])
            )
        )
        proof = verify_source_contract([row], sources, recipe, [], line, [node])
        self.assertEqual(proof["original_shared_nodes"], 1)
        self.assertEqual(proof["validated_source_pad_uses"], 1)
        row["junction_pads_xy_m"][0][0, 0] += 0.001
        with self.assertRaisesRegex(ValueError, "actual incident-profile hull"):
            verify_source_contract([row], sources, recipe, [], line, [node])

    def test_new_endpoint_and_old_interior_keep_the_original_roles(self):
        recipe, sources, row, node, line = original_contact()
        row["xy"] = row["xy"][1:]
        sources["new"]["mvt_xy"] = sources["new"]["mvt_xy"][1:]
        sources["new"]["xy_m"] = sources["new"]["xy_m"][1:]
        self.assertFalse(new_through_root(recipe, sources))
        bind_root_pad([row], node, line)
        self.assertNotIn("junction_pad_vertices", row)
        recipe["port"] = {"source_id": "old", "vertex": 1}
        self.assertFalse(new_through_root(recipe, sources))
        self.assertTrue(profile_interiors(recipe, sources))

    def test_old_interior_does_not_authorize_two_through_axes(self):
        recipe, sources, row, node, line = original_contact()
        recipe["port"] = {"source_id": "old", "vertex": 1}
        with self.assertRaisesRegex(ValueError, "no original endpoint"):
            bind_root_pad(
                [row],
                node,
                line,
                include_interiors=True,
                allow_through=new_through_root(recipe, sources),
            )
        with self.assertRaises(ValueError):
            bind_root_pad([row], node, line, allow_through=True)

    def test_an_equal_original_node_in_a_shifted_frame_is_not_a_contact(self):
        recipe, sources, *_ = original_contact()
        sources["new"]["xy_m"][1][0] = 1e-12
        with self.assertRaisesRegex(ValueError, "different source frame"):
            new_through_root(recipe, sources)

    def test_a_nearby_coordinate_without_the_original_node_cannot_bind(self):
        recipe, sources, *_ = original_contact()
        sources["new"]["mvt_xy"][1] = [1, 0]
        self.assertFalse(new_through_root(recipe, sources))

    def test_repeated_old_or_new_occurrences_are_ambiguous(self):
        for key in ("old", "new"):
            with self.subTest(key=key):
                recipe, sources, *_ = original_contact()
                sources[key]["mvt_xy"].insert(1, [0, 0])
                sources[key]["xy_m"].insert(1, [0, 0])
                with self.assertRaisesRegex(ValueError, "unique original"):
                    new_through_root(recipe, sources)

    def test_closed_old_source_has_no_outer_profile_and_cannot_be_new(self):
        recipe, sources, *_ = original_contact()
        sources["old"]["xy_m"][-1] = [0, 0]
        with self.assertRaises(ValueError):
            new_through_root(recipe, sources)
        recipe["port"]["source_id"] = "new"
        sources["new"]["mvt_xy"].insert(1, [0, -5])
        sources["new"]["xy_m"].insert(1, [0, -5])
        with self.assertRaises(ValueError):
            new_through_root(recipe, sources)

    def test_saved_root_membership_and_location_are_not_trusted(self):
        recipe, sources, row, node, line = original_contact()
        bind_root_pad([row], node, line, include_interiors=True, allow_through=True)
        for wrong in ({"incident": []}, {"xy": [0, 0.001]}, {"mvt_xy": [1, 0]}):
            with self.subTest(wrong=wrong), self.assertRaises(ValueError):
                verify_source_contract(
                    [row], sources, recipe, [], line, [dict(node, **wrong)]
                )
        wrong_sources = copy.deepcopy(sources)
        wrong_sources["new"]["mvt_xy"][1] = [1, 0]
        with self.assertRaises(ValueError):
            verify_source_contract([row], wrong_sources, recipe, [], line, [node])

    def test_flat_and_seed_modes_do_not_implicitly_gain_a_through_root(self):
        recipe, sources, *_ = original_contact()
        recipe["contact_mode"] = "flat"
        self.assertFalse(new_through_root(recipe, sources))
        self.assertFalse(profile_interiors(recipe, sources))
        recipe["contact_mode"] = "seed"
        self.assertFalse(new_through_root(recipe, sources))
        self.assertTrue(profile_interiors(recipe, sources))

    def test_another_branch_cannot_replace_a_missing_through_side(self):
        _recipe, _sources, row, node, _line = original_contact()
        other = {"source_id": "branch", "xy": np.array([[0, 0], [5, 0]]), "width_m": 2}
        node["incident"] = [0, 1]
        vertices = np.array([[-1, 0, 0], [1, 0, 0], [1, 5, 0], [-1, 5, 0]], float)
        data = {
            "surface_vertices": vertices,
            "surface_faces": np.array([[0, 1, 2], [0, 2, 3]]),
            "owners": np.ones((2, 2), bool),
        }
        shape = shapely.box(-1, -5, 1, 5)
        with self.assertRaisesRegex(ValueError, "positive-area facets on both sides"):
            new_interior_checks(
                data,
                [row, other],
                [node],
                [shape],
                [shape, shape],
                {},
                through_nodes=[0],
            )


if __name__ == "__main__":
    unittest.main()
