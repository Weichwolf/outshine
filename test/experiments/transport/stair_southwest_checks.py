"""Real minor-road component, varying profiles and explicit cached-interior loop."""

import copy
import json
import unittest

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_module_audit import audit
from stair_module_cache import merge_road
from stair_module_geometry import build, old_surfaces
from stair_module_inputs import FIXTURE, inputs, read_arrays
from stair_module_materials import body_slots
from stair_module_roads import inspect_frames
from stair_module_soil import fit, prepare
from stair_module_source_contract import verify_source_contract
from stair_module_trace import agree, compare
from stair_module_world import source_ground


def cached_reference():
    path = FIXTURE.with_name("stair-southwest-contact-vienna.npz")
    meta = json.loads(path.with_suffix(".json").read_text())
    if digest(path) != meta["data_sha256"]:
        raise ValueError("fixed original parent crop changed")
    values = read_arrays(path)
    module = {
        k.removeprefix("module_"): v
        for k, v in values.items()
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
    plan = {k: values["old_" + k] for k in ("xy", "faces", "ground", "targets", "bed")}
    return {
        "road": road,
        "road_ground": np.ones(len(road["top_faces"]), bool),
        "plan": plan,
        "heights": values["old_heights"],
        "piers": [],
        "pier_contacts": [],
        "cached_module_rows": meta["cached_module_rows"],
        "report": {
            "after": {
                "source_ids": meta["source_ids"],
                "proposed_outer_profiles": meta["outer_profiles"],
            },
            "candidate_source_ids": meta["source_ids"],
        },
    }


class SouthwestStreets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe, cls.rows, _context, cls.sources, sample, _layer = inputs(
            FIXTURE.with_name("stair-southwest-streets-vienna.json")
        )
        cls.sample = staticmethod(sample)
        cls.state = cached_reference()
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
            contact_mode="profile",
        )
        cls.proof = audit(
            cls.state, cls.recipe, cls.data, cls.rows, cls.planning, cls.line, cls.upper
        )

    def test_six_original_whole_sources_and_real_nonflat_contact(self):
        self.assertEqual(len(self.rows), 6)
        self.assertGreater(np.ptp(self.line[:, 2]), 0.024)
        for row in self.rows:
            source = self.sources[row["source_id"]]
            np.testing.assert_array_equal(row["xy"], source["xy_m"])
            np.testing.assert_array_equal(row["prior"], self.sample(row["xy"]))
            self.assertEqual(
                row["original"]["source"]["feature"], source["original_feature"]
            )
        self.assertEqual(self.data["surface_vertices"].shape, (156, 3))
        self.assertEqual(self.data["surface_faces"].shape, (244, 3))
        self.assertEqual(self.proof["profiles"]["complete_additional_profiles"], 10)
        self.assertEqual(self.proof["profiles"]["complete_cached_interior_profiles"], 1)
        self.assertEqual(len(self.proof["profiles"]["outer_profiles"]), 3)

    def test_road_assumptions_do_not_reclassify_original_provider_properties(self):
        roads = [r for r in self.rows if r.get("profile_kind") == "unclassified"]
        self.assertEqual(len(roads), 4)
        for row in roads:
            props = self.sources[row["source_id"]]["properties"]
            self.assertEqual(props["class"], "minor")
            self.assertNotIn("subclass", props)
            traffic = row["traffic_profile"]
            self.assertEqual(traffic["width_m"], 5.5)
            self.assertEqual(traffic["lane_width_m"], 2.75)
            self.assertEqual(traffic["source_oneway"], props.get("oneway"))
            self.assertEqual(
                traffic["forward_lanes"], 2 if props.get("oneway") == 1 else 1
            )
            self.assertEqual(
                traffic["reverse_lanes"], 0 if props.get("oneway") == 1 else 1
            )

    def test_actual_longitudinal_and_crossfall_bounds_pass_both_representations(self):
        for cast in (False, True):
            proof = inspect_frames(self.data, self.rows, cast)
            self.assertLessEqual(proof["maximum_normalized_profile_grade"], 1)
            for road in proof["roads"]:
                self.assertLess(road["maximum_longitudinal_grade"], 0.1)
                self.assertLess(road["maximum_crossfall"], 0.03)
        bad = dict(self.data, grade_frames=self.data["grade_frames"] * 0.5)
        with self.assertRaisesRegex(ValueError, "frames changed"):
            inspect_frames(bad, self.rows, False)

    def test_loop_uses_the_actual_original_interior_node(self):
        contacts = self.state["module_contacts"]
        self.assertEqual(len(contacts), 1)
        self.assertEqual(contacts[0]["mvt_xy"], [2446, 3763])
        recipe = copy.deepcopy(self.recipe)
        recipe["interior_contacts"][0]["end"] = -1
        with self.assertRaisesRegex(ValueError, "actual original interior source node"):
            old_surfaces(copy.deepcopy(self.state), recipe, self.sources)

    def test_undeclared_old_cycleway_reuse_is_rejected(self):
        recipe = dict(self.recipe, interior_contacts=[])
        state, rows = cached_reference(), copy.deepcopy(self.rows)
        for row in rows:
            row.pop("junction_pads_xy_m", None)
        old, line, ground, upper = old_surfaces(state, recipe, self.sources)
        data, _note, planning = build(
            rows, self.sources, old, line, ground, self.sample, contact_mode="profile"
        )
        with self.assertRaisesRegex(ValueError, "undeclared cached asset"):
            audit(state, recipe, data, rows, planning, line, upper)

    def test_whole_walls_reject_added_height_gap(self):
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

    def test_source_nodes_and_saved_pad_hulls_are_verified_without_planning(self):
        proof = verify_source_contract(
            self.rows,
            self.sources,
            self.recipe,
            self.state["module_contacts"],
            self.line,
            self.planning[0],
        )
        self.assertEqual(proof["original_shared_nodes"], 4)
        self.assertGreater(proof["validated_source_pad_uses"], 0)
        rows = copy.deepcopy(self.rows)
        rows[0]["junction_pads_xy_m"][0][0, 0] += 0.1
        with self.assertRaisesRegex(ValueError, "actual incident-profile hull"):
            verify_source_contract(
                rows,
                self.sources,
                self.recipe,
                self.state["module_contacts"],
                self.line,
                self.planning[0],
            )
        nodes = copy.deepcopy(self.planning[0])
        nodes[0]["xy"][0] += 0.01
        with self.assertRaisesRegex(ValueError, "membership or location"):
            verify_source_contract(
                self.rows,
                self.sources,
                self.recipe,
                self.state["module_contacts"],
                self.line,
                nodes,
            )

    def test_local_soil_reuses_exact_boundary_and_fixed_old_beds(self):
        local, _chosen, terrain = prepare(
            self.state, self.data, self.ground, self.recipe, self.sample
        )
        combined = source_ground(self.ground, self.data)
        footprint = shapely.union_all(
            shapely.polygons(combined.vertices[combined.triangles, :2])
        )
        heights, _roles, note = fit(local, self.upper, footprint)
        self.assertTrue(note["accepted_field"])
        np.testing.assert_array_equal(
            heights[local["boundary"]], local["bed"][local["boundary"]]
        )
        self.assertEqual(terrain["old_selected_faces"], 841)
        self.assertEqual(terrain["affected_old_piers"], 0)
        self.assertEqual(len(heights), 909)

    def test_new_road_caps_have_source_owned_asphalt_and_neutral_sides(self):
        slots = body_slots(self.data)
        m = len(self.data["surface_faces"])
        self.assertTrue(np.any(slots[:m] == 8))
        self.assertTrue(np.any(slots[:m] == 3))
        self.assertTrue(np.all(slots[2 * m :] == 6))
        bad = dict(self.data, surface_materials=np.full(m, 3))
        with self.assertRaisesRegex(ValueError, "material lost original source"):
            inspect_frames(bad, self.rows, False)


class WholeTraceCounterexamples(unittest.TestCase):
    def test_three_equal_probes_miss_an_actual_piecewise_profile_kink(self):
        x = [0, 0.5, 1, 2]
        vertices = np.array([[a, y, 0] for a in x for y in (-1, 1)], float)
        faces = np.array(
            [[2 * i, 2 * i + 2, 2 * i + 1] for i in range(3)]
            + [[2 * i + 1, 2 * i + 2, 2 * i + 3] for i in range(3)]
        )
        raised = vertices.copy()
        raised[raised[:, 0] == 0.5, 2] = 0.001
        line = np.array([[0, 0], [1, 0], [2, 0]], float)
        sampled = SurfaceSampler(Mesh(raised, faces)).sample(
            line, max_queries=3, max_candidates=64
        )
        np.testing.assert_array_equal(sampled.points_enu_m[:, 2], np.zeros(3))
        for cast in (False, True):
            with (
                self.subTest(cast=cast),
                self.assertRaisesRegex(ValueError, "profile heights disagree"),
            ):
                compare(
                    [[(vertices, faces, None)], [(raised, faces, None)]], line, cast
                )

    def test_intermediate_gap_and_work_exhaustion_remain_explicit(self):
        whole = np.array([[0, 2, 0, 0]], float)
        gap = np.array([[0, 0.9, 0, 0], [1.1, 2, 0, 0]], float)
        with self.assertRaisesRegex(ValueError, "uncovered interval"):
            agree(whole, gap, 2, xy_guard=1e-7, z_guard=1e-7)
        with self.assertRaisesRegex(ValueError, "budget exhausted"):
            agree(whole, gap, 2, xy_guard=1e-7, z_guard=1e-7, max_tests=1)


if __name__ == "__main__":
    unittest.main()
