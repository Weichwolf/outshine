"""A real second-stage contact whose flat profile has a varying side interface."""

import copy
import json
import unittest
from types import SimpleNamespace

import numpy as np
from camera_height_inputs import digest
from stair_cycle_contacts import profile
from stair_module_audit import audit
from stair_module_cache import load_cached, merge_road
from stair_module_geometry import build, old_surfaces
from stair_module_inputs import FIXTURE, inputs, read_arrays
from stair_module_sections import profile_levels


def cached_contact():
    path = FIXTURE.with_name("stair-module-chain-contact-vienna.npz")
    meta = json.loads(path.with_suffix(".json").read_text())
    if digest(path) != meta["data_sha256"]:
        raise ValueError("actual fixed parent input changed")
    arrays = read_arrays(path)
    module = {
        k.removeprefix("module_"): v
        for k, v in arrays.items()
        if k.startswith("module_")
    }
    empty = {
        "body_vertices": np.empty((0, 3)),
        "body_faces": np.empty((0, 3), np.uint32),
        "top_faces": np.empty((0, 3), int),
        "bottom_faces": np.empty((0, 3), int),
        "face_sources": np.empty(0, np.uint64),
    }
    road = merge_road(empty, module, 0)
    plan = {k: arrays["old_" + k] for k in ("xy", "faces", "ground", "targets", "bed")}
    state = {
        "road": road,
        "road_ground": np.ones(len(road["top_faces"]), bool),
        "plan": plan,
        "heights": arrays["old_heights"],
        "piers": [],
        "pier_contacts": [],
        "report": {
            "after": {
                "source_ids": meta["source_ids"],
                "proposed_outer_profiles": meta["outer_profiles"],
            },
            "candidate_source_ids": meta["source_ids"],
        },
    }
    return state, module


class RepeatedGroundModule(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe, cls.rows, _context, cls.sources, sample, _layer = inputs(
            FIXTURE.with_name("stair-west-module-chain-vienna.json")
        )
        cls.sample = staticmethod(sample)
        cls.state, cls.parent = cached_contact()
        cls.old, cls.line, cls.ground, cls.upper = old_surfaces(
            cls.state, cls.recipe, cls.sources
        )
        cls.data, cls.note, cls.planning = build(
            cls.rows,
            cls.sources,
            cls.old,
            cls.line,
            cls.ground,
            sample,
            contact_mode="piecewise",
        )
        cls.proof = audit(
            cls.state, cls.recipe, cls.data, cls.rows, cls.planning, cls.line, cls.upper
        )

    def test_real_whole_source_and_two_completed_profiles(self):
        self.assertEqual(self.data["surface_vertices"].shape, (14, 3))
        self.assertEqual(self.data["surface_faces"].shape, (16, 3))
        self.assertEqual(self.proof["profiles"]["complete_additional_profiles"], 2)
        self.assertEqual(self.proof["interfaces"]["segments"], 3)
        self.assertAlmostEqual(self.proof["interfaces"]["length_m"], 2.0538347512869746)
        for row in self.rows:
            np.testing.assert_array_equal(
                row["xy"], self.sources[row["source_id"]]["xy_m"]
            )
            np.testing.assert_array_equal(row["prior"], self.sample(row["xy"]))

    def test_old_source_varies_but_whole_root_profile_is_flat(self):
        owner = self.state["report"]["candidate_source_ids"].index(
            self.recipe["port"]["source_id"]
        )
        selected = self.parent["owners"][:, owner]
        levels = self.parent["surface_vertices"][
            self.parent["surface_faces"][selected], 2
        ]
        self.assertGreater(np.ptp(levels), 1)
        self.assertLess(np.ptp(self.line[:, 2]), 1e-7)
        values = self.data["fixed_heights"]
        self.assertGreater(np.ptp(values), 0.00011)
        self.assertLess(np.ptp(values), 0.00012)

    def test_same_actual_input_rejects_uniform_datum_without_relaxed_budget(self):
        rows = copy.deepcopy(self.rows)
        with self.assertRaisesRegex(ValueError, "nonflat or incompatible"):
            build(rows, self.sources, self.old, self.line, self.ground, self.sample)

    def test_changed_whole_contact_heights_cannot_pass(self):
        bad = {k: v.copy() for k, v in self.data.items()}
        bad["surface_vertices"][:, 2] += 0.0001
        bad["body_vertices"][:, 2] += 0.0001
        with self.assertRaisesRegex(ValueError, "uncovered region"):
            audit(
                self.state,
                self.recipe,
                bad,
                self.rows,
                self.planning,
                self.line,
                self.upper,
            )

    def test_float32_contact_envelope_has_measured_rounding_scope(self):
        for row in self.proof["interfaces"]["checks"]:
            if row["float32"]:
                self.assertGreater(
                    row["z_guard_m"], sum(row["local_vertex_z_rounding_m"])
                )
                self.assertLess(row["z_guard_m"], 15e-6)
            else:
                self.assertLess(row["z_guard_m"], 1.01e-7)
            self.assertTrue(all(s["uncovered_area_m2"] == 0 for s in row["sides"]))
            self.assertAlmostEqual(row["height_m"], 0.3, delta=4e-6)

    def test_profile_heights_use_line_cuts_instead_of_adjacent_face_extrema(self):
        values = profile_levels(
            self.data["surface_vertices"],
            self.data["surface_faces"],
            profile(self.rows[0], 0),
        )
        self.assertLess(np.ptp(values), 1e-7)
        self.assertGreater(
            np.ptp(self.data["surface_vertices"][self.data["fixed"], 2]), 1e-4
        )

    def test_cache_merge_keeps_actual_parent_geometry_and_owners(self):
        before = self.state["road"]
        after = merge_road(before, self.data, 4)
        for key in (
            "body_vertices",
            "body_faces",
            "top_faces",
            "bottom_faces",
            "face_sources",
        ):
            np.testing.assert_array_equal(after[key][: len(before[key])], before[key])
        self.assertTrue(
            np.all(after["face_sources"][len(before["face_sources"]) :] == 16)
        )
        with self.assertRaisesRegex(ValueError, "capacity"):
            merge_road(before, self.data, 64)

    def test_duplicate_or_excess_parent_stages_rejected_before_any_io(self):
        for parents in (
            [("/unused", FIXTURE)] * 2,
            [(f"/unused{i}", FIXTURE) for i in range(5)],
        ):
            with (
                self.subTest(parents=parents),
                self.assertRaisesRegex(ValueError, "four unique"),
            ):
                load_cached(SimpleNamespace(cached_module=parents), self.recipe)


if __name__ == "__main__":
    unittest.main()
