"""Joint source ownership, whole ports, conservative collision and partial output."""

import copy
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from indoor_floor_geometry import produce_floor_contacts
from indoor_floor_inputs import load_inputs
from indoor_floor_plan import FloorLimits
from indoor_stair_batch import compose_stairs
from indoor_stair_contacts import bind_stairs, group_stair_aliases
from indoor_stair_recipe import StairLimits
from indoor_stair_site import build_stair, check_stair_ports
from outshine_ref.contracts import LocalFrame
from test_indoor_floors import DATA, request
from test_indoor_stairs import chosen_site


def synthetic_group(binding, name):
    r = replace(binding["request"], identifier=name)
    r = replace(r, original=r.original | {"source_id": name, "properties": {}})
    b = binding | {
        "request": r,
        "source_id": name,
        "original": r.original,
        "frame": r.frame.identifier,
    }
    return {"primary": b, "members": [b]}


def two_stairs(shift_x=0.0, shift_y=3.0):
    first, floors, queries = chosen_site()
    length = first["request"].axis.length
    axis = shapely.LineString([(shift_x, shift_y), (length + shift_x, shift_y)])
    second = first | {"request": replace(first["request"], axis=axis)}
    extra = (
        request("second-low-way", [(shift_x - 3, shift_y), (shift_x, shift_y)]),
        request(
            "second-high-way",
            [(length + shift_x, shift_y), (length + shift_x + 3, shift_y)],
            1,
        ),
    )
    floors["choices"].extend(
        {"source_id": r.identifier, "floor_id": key, "status": "ready"}
        for r, key in zip(extra, ("low", "high"), strict=True)
    )
    return (
        (synthetic_group(first, "A"), synthetic_group(second, "B")),
        floors,
        (*queries, *extra),
    )


class JointStairControls(unittest.TestCase):
    def test_projection_overlap_stays_local_without_claiming_true_3d_collision(self):
        groups, floors, requests = two_stairs(shift_y=0.5)
        result = compose_stairs(groups, floors, requests)
        self.assertEqual(len(result["stairs"]), 1)
        self.assertEqual(
            result["source_ledger"][1]["reason"], "joint_stair_prism_overlap_unplanned"
        )
        self.assertEqual(len(result["source_ledger"]), 2)

    def test_real_opening_guard_can_remove_another_whole_port_and_falls_back(self):
        groups, floors, requests = two_stairs(shift_x=2, shift_y=1.5025)
        for group in groups:
            self.assertEqual(
                build_stair(group["primary"], floors, requests)["status"], "ready"
            )
        result = compose_stairs(groups, floors, requests)
        self.assertEqual(len(result["stairs"]), 1)
        self.assertEqual(result["stairs"][0]["source_id"], "A")
        self.assertEqual(
            result["source_ledger"][1]["reason"],
            "whole_stair_endpoint_removed_by_joint_opening",
        )
        self.assertGreater(result["work"]["fallback_candidates"], 0)
        self.assertEqual(check_stair_ports(result["stairs"], result["floors"]), 2)

    def test_budget_and_wrong_frame_do_not_destroy_an_independent_good_part(self):
        groups, floors, requests = two_stairs()
        limited = compose_stairs(groups, floors, requests, max_bodies=1)
        self.assertEqual(len(limited["stairs"]), 1)
        self.assertEqual(
            limited["source_ledger"][1]["reason"], "stair_batch_construction_budget"
        )
        bad = groups[1]["primary"] | {
            "request": replace(
                groups[1]["primary"]["request"], frame=LocalFrame("other")
            )
        }
        with patch("indoor_stair_site.stair_mesh") as mesh:
            rejected = build_stair(bad, floors, requests)
        self.assertEqual(rejected["reason"], "stair_floor_frame_mismatch")
        mesh.assert_not_called()
        mixed = compose_stairs(
            (groups[0], {"primary": bad, "members": [bad]}), floors, requests
        )
        self.assertEqual(len(mixed["stairs"]), 1)
        self.assertEqual(len(mixed["source_features"]), 1)
        self.assertEqual(mixed["source_ledger"][1]["frame"], "other")

    def test_inconsistent_chosen_height_is_rejected_before_geometry(self):
        groups, floors, requests = two_stairs()
        binding = copy.deepcopy(groups[0]["primary"])
        binding["ends"][1]["z_m"] += 0.5
        with patch("indoor_stair_site.stair_mesh") as mesh:
            rejected = build_stair(binding, floors, requests)
        self.assertEqual(
            rejected["reason"], "stair_endpoint_height_differs_from_chosen_floor"
        )
        mesh.assert_not_called()


class ActualJointStairs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inputs = load_inputs(DATA)
        original = {r.identifier: r for r in cls.inputs.requests}
        cls.requests = tuple(
            original.get(r.identifier, r)
            for r in cls.inputs.constraints
            if r.purpose == "flat_indoor_walkway"
        )
        cls.floors = produce_floor_contacts(
            cls.inputs.parts,
            cls.requests,
            cls.inputs.ground,
            method="adapted",
            constraints=cls.inputs.constraints,
            limits=FloorLimits(max_requests=128),
        )
        cls.bindings = bind_stairs(cls.inputs, cls.floors)
        cls.receipts = {r["sha256"]: r for r in cls.inputs.metadata["source_receipts"]}
        cls.groups = group_stair_aliases(cls.bindings, cls.receipts, StairLimits())

    def test_all_actual_stairs_share_two_final_plates_with_originals_and_ports(self):
        result = compose_stairs(self.groups, self.floors, self.requests)
        self.assertEqual(len(result["stairs"]), 6)
        self.assertEqual(len(result["source_ledger"]), 64)
        self.assertEqual(
            sum(r["status"] == "shared" for r in result["source_ledger"]), 1
        )
        self.assertEqual(len(result["source_features"]), 7)
        self.assertEqual(len(result["modified_floor_meshes"]), 2)
        self.assertEqual(result["work"]["footprint_pair_tests"], 15)
        self.assertEqual(check_stair_ports(result["stairs"], result["floors"]), 12)
        original = {b["source_id"]: b["original"] for b in self.bindings}
        for row in result["source_ledger"]:
            self.assertEqual(row["original"], original[row["source_id"]])
        final = {f.identifier: f for f in result["floors"]}
        choices = {r["source_id"]: r for r in self.floors["choices"]}
        for r in self.requests:
            c = choices[r.identifier]
            if c["status"] == "ready":
                self.assertTrue(final[c["floor_id"]].polygon.covers(r.corridor))
        for index, stair in enumerate(result["stairs"]):
            for previous in result["stairs"][:index]:
                self.assertEqual(
                    stair["footprint"].intersection(previous["footprint"]).area, 0
                )

    def test_eager_and_shared_emit_equal_geometry_and_source_order_independently(self):
        before = compose_stairs(self.groups, self.floors, self.requests, method="eager")
        after = compose_stairs(self.groups[::-1], self.floors, self.requests)
        self.assertEqual(before["source_ledger"], after["source_ledger"])
        for a, b in zip(before["stairs"], after["stairs"], strict=True):
            self.assertEqual(a["source_ids"], b["source_ids"])
            for name in ("vertices", "faces", "parts"):
                np.testing.assert_array_equal(a["mesh"][name], b["mesh"][name])
        for key, mesh in before["modified_floor_meshes"].items():
            for name in ("vertices", "faces", "parts"):
                np.testing.assert_array_equal(
                    mesh[name], after["modified_floor_meshes"][key][name]
                )
        self.assertEqual(before["work"]["individual_plate_meshes_qualified"], 6)
        self.assertEqual(after["work"]["individual_plate_meshes_qualified"], 0)
        self.assertEqual(after["work"]["joint_plate_meshes_qualified"], 2)
        reversed_groups = group_stair_aliases(
            self.bindings[::-1], self.receipts, StairLimits()
        )
        self.assertEqual(
            [[b["source_id"] for b in g["members"]] for g in self.groups],
            [[b["source_id"] for b in g["members"]] for g in reversed_groups],
        )

    def test_same_raw_path_with_different_attributes_is_not_shared(self):
        first = next(g["primary"] for g in self.groups if len(g["members"]) > 1)
        new_id = "zz-private"
        original = first["original"] | {
            "source_id": new_id,
            "properties": first["original"]["properties"] | {"access": "private"},
        }
        other = first | {
            "source_id": new_id,
            "original": original,
            "request": replace(first["request"], identifier=new_id, original=original),
        }
        groups = group_stair_aliases((first, other), self.receipts, StairLimits())
        self.assertEqual(len(groups), 2)
        result = compose_stairs(groups, self.floors, self.requests)
        self.assertEqual(len(result["stairs"]), 1)
        self.assertEqual(
            result["source_ledger"][1]["reason"], "joint_stair_prism_overlap_unplanned"
        )


if __name__ == "__main__":
    unittest.main()
