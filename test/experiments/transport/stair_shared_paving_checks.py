"""Actual borrowed facets, whole road frames and original-node walking semantics."""

import copy
import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_module_clearance import source_clearances
from stair_module_inputs import inputs
from stair_shared_paving import cap_frames, paving_node
from stair_surface_access import contact_access
from stair_surface_portals import portal

FIXTURES = Path(__file__).with_name("fixtures")


def square(x0, x1, width=2, grade=(0, 0), height=0):
    xy = np.array(
        [[x0, -width / 2], [x1, -width / 2], [x1, width / 2], [x0, width / 2]], float
    )
    return Mesh(np.c_[xy, height + xy @ grade], np.array([[0, 1, 2], [0, 2, 3]]))


class SharedPavingControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe, cls.rows, _, cls.sources, *_ = inputs(
            FIXTURES / "stair-cached-grid-paving-vienna.json"
        )
        path = FIXTURES / "stair-shared-paving-caps-vienna.npz"
        cls.meta = json.loads(path.with_suffix(".json").read_text())
        if hashlib.sha256(path.read_bytes()).hexdigest() != cls.meta["data_sha256"]:
            raise ValueError("actual old paving fixture changed")
        with np.load(path, allow_pickle=False) as archive:
            cls.caps = [
                Mesh(archive[f"cap{i}_vertices"], archive[f"cap{i}_faces"])
                for i in range(len(cls.meta["caps"]))
            ]

    def context(self):
        sources = copy.deepcopy(self.sources)
        chosen = copy.deepcopy(self.recipe["surface_contacts"][0])
        old = chosen["cached_source_id"]
        state = {
            "cached_module_rows": {old: {"xy": sources[old]["xy_m"], "width_m": 1.8}}
        }
        return state, sources, chosen

    def test_actual_old_facets_are_byte_preserved_and_meet_both_new_road_frames(self):
        np.testing.assert_array_equal(self.rows[0]["xy"], self.meta["source_axis_xy_m"])
        shape = shapely.from_wkt(self.meta["original_source_mask_wkt"])
        for cap, record in zip(self.caps, self.meta["caps"], strict=True):
            self.assertEqual(
                hashlib.sha256(cap.vertices[cap.triangles].tobytes()).hexdigest(),
                record["top_xyz_sha256"],
            )
            result, count = cap_frames(cap, self.rows[0], shape)
            self.assertGreater(count, 0)
            self.assertEqual(result, record["checks"])
            self.assertTrue(all(c["maximum_normalized_grade"] <= 1 for c in result))

    def test_shared_pavement_keeps_oneway_and_never_grants_motor_access_to_footway(
        self,
    ):
        state, sources, chosen = self.context()
        before = copy.deepcopy(sources)
        old = chosen["cached_source_id"]
        proof = contact_access(
            chosen,
            sources[chosen["source_id"]],
            sources[old],
            state["cached_module_rows"][old],
        )
        self.assertEqual(proof["source_public_mode_bits"], [[7, 4], [4, 4]])
        self.assertEqual(proof["motor_cycle_contact_arcs"], 0)
        self.assertEqual(
            paving_node(state, self.recipe, sources, chosen),
            {"original_shared_node_mvt": [1438, 1915], "walk_width_m": 1.8},
        )
        self.assertEqual(sources, before)

    def test_wrong_node_frame_root_curve_and_repeated_node_are_refused(self):
        state, sources, chosen = self.context()
        key = chosen["cached_source_id"]
        for change in ("node", "frame", "root", "curve", "repeat"):
            selected, recipe = copy.deepcopy(sources), copy.deepcopy(self.recipe)
            if change == "node":
                selected[key]["mvt_xy"] = [
                    [a + 1, b] for a, b in selected[key]["mvt_xy"]
                ]
            elif change == "frame":
                selected[key]["xy_m"] = (
                    np.array(selected[key]["xy_m"]) + [0.001, 0]
                ).tolist()
            elif change == "root":
                recipe["port"]["source_id"] = key
            elif change == "curve":
                recipe["road_axis"] = {}
            else:
                selected[key]["mvt_xy"].append([1438, 1915])
                selected[key]["xy_m"].append(selected[key]["xy_m"][-1])
            with self.subTest(change=change), self.assertRaises(ValueError):
                paving_node(state, recipe, selected, chosen)

    def test_private_unknown_and_oneway_walking_do_not_become_public_crossings(self):
        state, sources, chosen = self.context()
        key, new = chosen["cached_source_id"], chosen["source_id"]
        for properties in (
            {"access": "private"},
            {"foot": "unknown"},
            {"oneway:foot": 1},
        ):
            target = copy.deepcopy(sources[key])
            target["properties"].update(properties)
            with self.subTest(properties=properties), self.assertRaises(ValueError):
                contact_access(
                    chosen, sources[new], target, state["cached_module_rows"][key]
                )

    def test_isotropic_foot_gradient_does_not_prove_carriageway_crossfall(self):
        row = {
            "xy": np.array([[0, 0], [10, 0]], float),
            "gradient": 0.1,
            "crossfall_limit": 0.03,
        }
        mask = shapely.box(0, -1, 2, 1)
        self.assertLess(np.linalg.norm([0.02, 0.04]), 0.1)
        with self.assertRaisesRegex(ValueError, "road profile frame"):
            cap_frames(square(0, 2, grade=(0.02, 0.04)), row, mask)
        good, count = cap_frames(square(0, 2, grade=(0.08, 0.005)), row, mask)
        self.assertEqual(count, 2)
        self.assertTrue(all(p["maximum_normalized_grade"] < 1 for p in good))

    def test_walking_portal_uses_whole_foot_width_and_still_rejects_height_step(self):
        old, new = square(-4, 0, 1.8), square(0, 4, 5.5)
        proof = portal(old.vertices, old.triangles, new.vertices, new.triangles, 1.8)
        self.assertGreaterEqual(proof["length_m"], 1.8 - 2e-7)
        with self.assertRaises(ValueError):
            portal(old.vertices, old.triangles, new.vertices, new.triangles, 5.5)
        with self.assertRaises(ValueError):
            portal(
                old.vertices,
                old.triangles,
                new.vertices + [0, 0, 0.001],
                new.triangles,
                1.8,
            )

    def test_old_footway_clearance_does_not_replace_new_vehicle_clearance(self):
        top, ceiling = square(-2, 2), square(-2, 2, height=4)
        data = {
            "surface_vertices": top.vertices,
            "surface_faces": top.triangles,
            "owners": np.ones((2, 1), bool),
        }
        shape = shapely.box(-2, -1, 2, 1)
        road = {"source_id": "road", "traffic_profile": {"clearance_m": 4.5}}
        with self.assertRaisesRegex(ValueError, "headroom below"):
            source_clearances(data, [road], [shape], [], ceiling)
        foot = {"source_id": "foot", "traffic_profile": {"clearance_m": 2.5}}
        proof = source_clearances(data, [foot], [shape], [], ceiling)
        self.assertTrue(all(p["sources"][0]["valid"] for p in proof))


if __name__ == "__main__":
    unittest.main()
