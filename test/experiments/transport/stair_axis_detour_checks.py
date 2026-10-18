"""Actual pier/source counterexample and explicit corridor-replacement contracts."""

import copy
import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from shared_bridge_geometry import corridor
from stair_axis_detours import apply, bind, checked_axis, proposal
from stair_module_inputs import inputs

FIXTURES = Path(__file__).with_name("fixtures")


class PierDetours(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe, cls.rows, _, cls.sources, *_ = inputs(
            FIXTURES / "stair-pier-detour-vienna.json"
        )
        cls.key = cls.recipe["source_ids"][0]
        cls.declaration = cls.recipe["source_axis_detours"][cls.key]
        cls.foot = shapely.from_wkb(cls.declaration["pier_footprint_wkb"])
        p = FIXTURES / "stair-pier-detour-obstacle-vienna.npz"
        meta = json.loads(p.with_suffix(".json").read_text())
        if hashlib.sha256(p.read_bytes()).hexdigest() != meta["data_sha256"]:
            raise ValueError("actual whole pier fixture changed")
        with np.load(p, allow_pickle=False) as d:
            cls.pier = Mesh(d["vertices"], d["faces"])

    def raw(self):
        row = copy.deepcopy(self.rows[0])
        row.pop("surface_axis_xy_m")
        row.pop("source_axis_detour")
        return row

    def test_actual_whole_corridor_excludes_pier_and_preserves_original_nodes(self):
        row = self.rows[0]
        original = np.asarray(self.sources[self.key]["xy_m"])
        np.testing.assert_array_equal(row["xy"], original)
        axis = checked_axis(row)
        segment = self.declaration["segment"]
        indices = np.r_[np.arange(segment + 1), np.arange(segment + 5, len(axis))]
        np.testing.assert_array_equal(axis[indices], original)
        old, new = corridor(self.raw()), corridor(row)
        self.assertEqual(old.distance(self.foot), 0.0)
        self.assertGreaterEqual(new.distance(self.foot), 0.2)
        self.assertGreater(old.difference(new).intersection(self.foot).area, 0)
        rounded = shapely.transform(new, lambda xy: xy.astype("f4").astype(float))
        self.assertTrue(rounded.is_valid)
        self.assertGreaterEqual(rounded.distance(self.foot), 0.2)
        self.assertEqual(row["source_axis_detour"]["added_knots"], 4)

    def test_original_preserving_curve_family_still_retains_original_band(self):
        row = copy.deepcopy(self.rows[0])
        row.pop("source_axis_detour")
        original = corridor(self.raw())
        self.assertLess(original.difference(corridor(row)).area, 1e-7)
        self.assertEqual(corridor(row).distance(self.foot), 0.0)

    def test_moved_original_knot_removed_guard_or_excessive_offset_are_refused(self):
        for change in ("source", "knot", "guard", "offset"):
            row = copy.deepcopy(self.rows[0])
            segment = self.declaration["segment"]
            if change == "source":
                row["xy"][0, 0] += 0.001
            elif change == "knot":
                row["surface_axis_xy_m"][0, 0] += 0.001
            elif change == "guard":
                row["surface_axis_xy_m"][segment + 1, 0] += 0.001
            else:
                row["source_axis_detour"]["offset_m"] = 4.0
            with self.subTest(change=change), self.assertRaises(ValueError):
                corridor(row)

    def test_source_ends_or_unblocked_obstacle_cannot_request_this_detour(self):
        for segment in (0, len(self.rows[0]["xy"]) - 2):
            with self.subTest(segment=segment), self.assertRaises(ValueError):
                proposal(self.raw(), dict(self.declaration, segment=segment))
        near = shapely.transform(self.foot, lambda xy: xy + [2.0, 0.0])
        with self.assertRaises(ValueError):
            proposal(
                self.raw(), dict(self.declaration, pier_footprint_wkb=near.wkb_hex)
            )

    def test_only_one_declared_foot_cycle_interpretation_is_allowed(self):
        for change in ("motor", "seed", "other_axis", "unknown_source"):
            row, recipe = self.raw(), copy.deepcopy(self.recipe)
            if change == "motor":
                row["original"]["source"]["feature"]["properties"]["subclass"] = (
                    "motorway"
                )
            elif change == "seed":
                recipe["contact_mode"] = "seed"
            elif change == "other_axis":
                row["surface_axis_xy_m"] = row["xy"].copy()
            else:
                recipe["source_axis_detours"]["unknown"] = self.declaration
            with self.subTest(change=change), self.assertRaises(ValueError):
                apply([row], recipe)

    def test_actual_whole_pier_binding_refuses_changed_geometry_or_footprint(self):
        recipe = copy.deepcopy(self.recipe)
        recipe["source_axis_detours"][self.key]["pier_index"] = 0
        proof = bind([self.pier], recipe)
        self.assertEqual(
            proof[0]["actual_pier_xyz_sha256"], self.declaration["pier_xyz_sha256"]
        )
        moved = Mesh(self.pier.vertices + [0.01, 0, 0], self.pier.triangles)
        with self.assertRaises(ValueError):
            bind([moved], recipe)
        recipe["source_axis_detours"][self.key]["pier_footprint_wkb"] = (
            self.foot.buffer(0.01).wkb_hex
        )
        with self.assertRaises(ValueError):
            bind([self.pier], recipe)
