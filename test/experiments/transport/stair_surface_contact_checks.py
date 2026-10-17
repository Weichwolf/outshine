"""Contact provenance, supplied access, opposite-side traces and bounded pads."""

import copy
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from shared_bridge_geometry import corridor
from stair_junction_layout import landings
from stair_local_assets import road_asset
from stair_module_source_contract import verify_source_contract
from stair_shared_boxes import add_boxes, common_pad, verify_masks
from stair_surface_access import contact_access
from stair_surface_contacts import bind_contacts, contact_regions
from stair_surface_portals import portal


def strip(x0, x1, width=2, height=0):
    vertices = np.array(
        [
            [x0, -width / 2, height],
            [x1, -width / 2, height],
            [x1, width / 2, height],
            [x0, width / 2, height],
        ],
        float,
    )
    return vertices, np.array([[0, 1, 2], [0, 2, 3]])


def case():
    vertices, faces = strip(-4, 0)
    row = {"source_id": "old", "xy": [[-4, 0], [0, 0]], "width_m": 1.8}
    data = {
        "body_vertices": np.vstack([vertices, vertices - [0, 0, 0.3]]),
        "body_faces": faces,
        "top_faces": faces,
        "bottom_faces": faces + 4,
        "ground": np.ones(2, bool),
        "owners": np.ones((2, 1), bool),
    }
    asset = road_asset("actual-asset", ["old"], [row], data)
    state = {
        "road_assets": [asset],
        "road": dict(data, face_sources=np.ones(2, np.uint64)),
        "cached_module_rows": {"old": row},
        "road_ground": data["ground"],
        "report": {"candidate_source_ids": ["old"]},
    }
    sources = {
        k: {"properties": {"class": "path", "subclass": "footway"}}
        for k in ("new", "old")
    }
    chosen = {
        "source_id": "new",
        "cached_source_id": "old",
        "kind": "footway_overlap",
        "modes": ["walk"],
        "access_assumption": "procedural_walk_default",
    }
    recipe = {
        "source_ids": ["new"],
        "contact_mode": "profile",
        "junction_mode": "shared_box",
        "surface_contacts": [chosen],
    }
    return state, sources, recipe


class SurfaceContactControls(unittest.TestCase):
    def test_actual_owner_binding_mask_and_private_access_are_separate(self):
        state, sources, recipe = case()
        contacts = bind_contacts(state, recipe, sources)
        self.assertEqual(contacts[0]["binding"]["asset_owner_column"], 0)
        self.assertEqual(contacts[0]["binding"]["asset_top_face_indices"], [0, 1])
        bound = contact_regions(contacts, [shapely.box(-1, -0.9, 2, 0.9)])
        self.assertAlmostEqual(bound[0]["mask"].area, 1.8)
        sources["old"]["properties"]["access"] = "private"
        with self.assertRaisesRegex(ValueError, "public walk"):
            bind_contacts(state, recipe, sources)

    def test_specific_foot_permission_and_motor_oneway_are_preserved(self):
        state, sources, recipe = case()
        sources["old"]["properties"] = {
            "class": "minor",
            "oneway": 1,
            "access": "no",
            "foot": "yes",
        }
        row = dict(
            state["cached_module_rows"]["old"],
            source_recipe="minor_ground_carriageway",
            profile_kind="unclassified",
            traffic_profile={"width_m": 1.8},
        )
        chosen = dict(recipe["surface_contacts"][0], kind="minor_pedestrian_crossing")
        before = copy.deepcopy(sources)
        proof = contact_access(chosen, sources["new"], sources["old"], row)
        self.assertEqual(proof["source_public_mode_bits"][1], [4, 4])
        sources["old"]["properties"].pop("access")
        sources["old"]["properties"].pop("foot")
        proof = contact_access(chosen, sources["new"], sources["old"], row)
        self.assertEqual(proof["source_public_mode_bits"][1], [7, 4])
        self.assertEqual(proof["motor_cycle_contact_arcs"], 0)
        self.assertEqual(
            before["old"]["properties"]["oneway"],
            sources["old"]["properties"]["oneway"],
        )

    def test_restricted_unknown_foot_oneway_bridge_and_missing_assumption_refused(self):
        state, sources, recipe = case()
        chosen, row = recipe["surface_contacts"][0], state["cached_module_rows"]["old"]
        for props in (
            {"foot": "private"},
            {"foot": "unknown"},
            {"oneway:foot": 1},
            {"brunnel": "bridge"},
            {"level": 1},
            {"indoor": True},
        ):
            altered = copy.deepcopy(sources["old"])
            altered["properties"].update(props)
            with self.subTest(props=props), self.assertRaises(ValueError):
                contact_access(chosen, sources["new"], altered, row)
        with self.assertRaises(ValueError):
            contact_access(
                dict(chosen, access_assumption=None),
                sources["new"],
                sources["old"],
                row,
            )

    def test_wrong_working_owner_duplicate_and_contact_budget_refused(self):
        state, sources, recipe = case()
        for altered in (
            dict(recipe, surface_contacts=recipe["surface_contacts"] * 2),
            dict(recipe, surface_contacts=recipe["surface_contacts"] * 5),
            dict(recipe, junction_mode="level_box"),
        ):
            with self.assertRaises(ValueError):
                bind_contacts(state, altered, sources)
        state["road"]["body_vertices"] = state["road"]["body_vertices"] + [0, 0, 0.001]
        with self.assertRaisesRegex(ValueError, "asset-local owner"):
            bind_contacts(state, recipe, sources)

    def test_disjoint_contact_is_not_an_access_decision(self):
        state, sources, recipe = case()
        contacts = bind_contacts(state, recipe, sources)
        with self.assertRaisesRegex(ValueError, "positive overlap"):
            contact_regions(contacts, [shapely.box(1, -1, 2, 1)])

    def test_old_bridge_steep_cap_and_wrong_depth_are_refused(self):
        for failure in ("ground", "steep", "depth"):
            state, sources, recipe = case()
            asset = state["road_assets"][0]
            data = {k: v.copy() for k, v in asset.data.items()}
            if failure == "ground":
                data["ground"][:] = False
            elif failure == "steep":
                data["body_vertices"][:, 2] += data["body_vertices"][:, 0] * 0.3
            else:
                data["body_vertices"][4:, 2] -= 0.01
            state["road_assets"] = [
                road_asset(asset.identity, asset.source_ids, asset.rows, data)
            ]
            with (
                self.subTest(failure=failure),
                self.assertRaisesRegex(ValueError, "ground/depth/gradient"),
            ):
                bind_contacts(state, recipe, sources)

    def test_unknown_conditional_access_is_not_a_public_default(self):
        state, sources, recipe = case()
        sources["old"]["properties"]["foot:conditional"] = "no @ (sunset-sunrise)"
        with self.assertRaises(ValueError):
            bind_contacts(state, recipe, sources)

    def test_portal_compares_full_distinct_opposite_side_surfaces(self):
        old, of = strip(-4, 0)
        new, nf = strip(0, 4, 1.8)
        result = portal(old, of, new, nf, 1.8)
        self.assertGreaterEqual(result["length_m"], 1.8 - 2e-7)
        self.assertEqual(len(result["checks"]), 2)
        for bad in (new + [0, 0, 0.001], new + [0.001, 0, 0], strip(0, 4, 1.7)[0], old):
            with self.subTest(bad=bad.tolist()), self.assertRaises(ValueError):
                portal(old, of, bad, nf, 1.8)

    def test_common_pad_and_saved_support_masks_must_be_complete(self):
        rows = [
            {"source_id": "new", "xy": np.array([[0, 0], [4, 0]], float), "width_m": 2}
        ]
        nodes = [{"mvt_xy": [0, 0], "xy": [0, 0], "incident": [0]}]
        old, of = strip(-4, 0, 4)
        new, nf = strip(0, 4, 4)
        data = {
            "surface_vertices": new,
            "surface_faces": nf,
            "owners": np.ones((2, 1), bool),
        }
        old_line = np.array([[-2, 0], [0, 0], [2, 0]], float)
        common_pad(
            data, nodes, rows, 0, [Mesh(old, of)], [old_line], include_interiors=False
        )
        with self.assertRaisesRegex(ValueError, "pad is incomplete"):
            common_pad(
                dict(data, owners=np.zeros((2, 1), bool)),
                nodes,
                rows,
                0,
                [Mesh(old, of)],
                [old_line],
                include_interiors=False,
            )
        line = np.c_[old_line, np.zeros(3)]
        original = [corridor(rows[0])]
        _, _, masks = landings(rows, original, "width", 0.75, nodes)
        from stair_module_roots import profile_masks

        masks = profile_masks(nodes, masks, line, [])
        verify_masks(rows, nodes, original, masks, line, [])
        with self.assertRaisesRegex(ValueError, "saved shared-box masks"):
            verify_masks(rows, nodes, original, [masks[0].buffer(0.001)], line, [])

    def test_double_positive_portal_side_cannot_disappear_in_float32(self):
        old, of = strip(1024 - 1e-5, 1024, 1.8)
        new, nf = strip(1024, 1025, 1.8)
        with self.assertRaisesRegex(ValueError, "no complete opposite-side"):
            portal(old, of, new, nf, 1.8)

    def test_saved_original_node_boxes_cannot_be_omitted_or_added_elsewhere(self):
        rows = [
            {"source_id": "new", "xy": np.array([[0, 0], [10, 0]], float), "width_m": 2}
        ]
        sources = {
            "new": {"mvt_xy": [[0, 0], [10, 0]]},
            "old": {"mvt_xy": [[-10, 0], [0, 0]]},
        }
        recipe = {
            "contact_mode": "profile",
            "junction_mode": "shared_box",
            "port": {"source_id": "old", "end": -1},
        }
        nodes = [{"mvt_xy": [0, 0], "xy": [0, 0], "incident": [0]}]
        line = np.array([[0, -2, 0], [0, 0, 0], [0, 2, 0]], float)
        add_boxes(rows, nodes, line, [], include_interiors=False)
        verify_source_contract(rows, sources, recipe, [], line, nodes)
        saved = rows[0].pop("junction_pads_xy_m")
        with self.assertRaisesRegex(ValueError, "box missing"):
            verify_source_contract(rows, sources, recipe, [], line, nodes)
        rows[0]["junction_pads_xy_m"] = [*saved, [[8, 8], [9, 8], [9, 9], [8, 9]]]
        with self.assertRaisesRegex(ValueError, "incident-profile hull"):
            verify_source_contract(rows, sources, recipe, [], line, nodes)


if __name__ == "__main__":
    unittest.main()
