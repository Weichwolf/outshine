"""Real cached-interface, whole-wall, pier and bounded-field counterexamples."""

import copy
import json
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_blend_solve import candidate, pins_for, validate_candidate
from stair_module_audit import audit
from stair_module_contacts import interfaces
from stair_module_geometry import build, old_surfaces
from stair_module_inputs import FIXTURE, inputs, read_arrays
from stair_module_soil import fit, prepare


def context_state():
    path = FIXTURE.with_name("stair-module-contact-vienna.npz")
    meta = json.loads(path.with_suffix(".json").read_text())
    if digest(path) != meta["data_sha256"]:
        raise ValueError("fixed old contact input changed")
    data = read_arrays(path)
    plan = {k: data["old_" + k] for k in ("xy", "faces", "targets", "bed", "ground")}
    n = len(data["root_top_faces"])
    road = {
        "body_vertices": data["root_vertices"],
        "body_faces": data["root_body_faces"],
        "top_faces": data["root_top_faces"],
        "bottom_faces": data["root_bottom_faces"],
        "face_sources": np.ones(n, dtype=np.int64),
    }
    piers, contacts = [], []
    for i in range(len(meta["pier_source_indices"])):
        piers.append(Mesh(data[f"pier_{i}_vertices"], data[f"pier_{i}_faces"]))
        contacts.append(
            tuple(
                Mesh(data[f"pier_{i}_{name}_vertices"], data[f"pier_{i}_contact_faces"])
                for name in ("low", "high")
            )
        )
    state = {
        "road": road,
        "plan": plan,
        "heights": data["old_heights"],
        "piers": piers,
        "pier_contacts": contacts,
        "report": {
            "after": {
                "source_ids": [meta["frozen_profile"]["source_id"]],
                "proposed_outer_profiles": [meta["frozen_profile"]],
            }
        },
    }
    return state, Mesh(data["upper_vertices"], data["upper_faces"])


class RealGroundModule(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe, cls.rows, cls.context, cls.sources, sample, _layer = inputs()
        cls.sample = staticmethod(sample)
        cls.state, cls.upper = context_state()
        old, cls.line, cls.ground, _unused = old_surfaces(
            cls.state, cls.recipe, cls.sources
        )
        cls.data, cls.note, cls.planning = build(
            cls.rows, cls.sources, old, cls.line, cls.ground, sample
        )
        cls.proof = audit(
            cls.state, cls.recipe, cls.data, cls.rows, cls.planning, cls.line, cls.upper
        )
        cls.local, cls.selected, cls.terrain = prepare(
            cls.state, cls.data, cls.ground, cls.recipe, sample
        )
        cls.footprint = shapely.union_all(
            [cls.planning[4], shapely.union_all(cls.planning[2])]
        )
        cls.heights, cls.roles, cls.solve = fit(cls.local, cls.upper, cls.footprint)

    def test_whole_original_sources_and_six_profiles_pass_actual_export(self):
        self.assertEqual(len(self.rows), 4)
        for row in self.rows:
            source = self.sources[row["source_id"]]
            np.testing.assert_array_equal(row["xy"], source["xy_m"])
            np.testing.assert_array_equal(row["prior"], self.sample(row["xy"]))
            self.assertEqual(
                row["original"]["source"]["feature"], source["original_feature"]
            )
        self.assertEqual(self.proof["profiles"]["complete_additional_profiles"], 6)
        self.assertEqual(len(self.proof["profiles"]["outer_profiles"]), 3)
        self.assertEqual(self.proof["body"]["surface_faces"], 203)
        self.assertEqual(self.proof["body"]["body_faces"], 576)

    def test_every_actual_wall_rectangle_is_covered_with_opposite_orientation(self):
        proof = self.proof["interfaces"]
        self.assertAlmostEqual(proof["length_m"], 8.59392684549, places=8)
        self.assertEqual(proof["segments"], 5)
        for row in proof["checks"]:
            self.assertLess(row["xy_guard_m"], 1e-5)
            self.assertEqual(
                row["sides"][0]["orientation"], -row["sides"][1]["orientation"]
            )
            self.assertEqual(sum(q["uncovered_area_m2"] for q in row["sides"]), 0)

    def test_missing_cached_wall_cannot_be_replaced_by_roof_samples(self):
        old = dict(self.state["road"])
        old["body_faces"] = old["body_faces"][: 2 * len(old["top_faces"])]
        with self.assertRaisesRegex(ValueError, "wall missing"):
            interfaces(
                old,
                self.data,
                self.planning[4],
                self.planning[2],
                float(self.line[1, 2]),
            )

    def test_shifted_or_inverted_body_fails_whole_contact(self):
        for shift in ([0.001, 0, 0], [0, 0, 0.001]):
            bad = dict(self.data, body_vertices=self.data["body_vertices"] + shift)
            with self.subTest(shift=shift), self.assertRaises(ValueError):
                audit(
                    self.state,
                    self.recipe,
                    bad,
                    self.rows,
                    self.planning,
                    self.line,
                    self.upper,
                )
        bad = dict(self.state["road"])
        bad["body_faces"] = bad["body_faces"][:, ::-1]
        with self.assertRaisesRegex(ValueError, "oppositely oriented"):
            interfaces(
                bad,
                self.data,
                self.planning[4],
                self.planning[2],
                float(self.line[1, 2]),
            )

    def test_nonflat_old_source_is_not_a_frozen_flat_interface(self):
        state = copy.deepcopy(self.state)
        index = state["road"]["top_faces"][0, 0]
        state["road"]["body_vertices"][index, 2] += 0.001
        with self.assertRaises(ValueError):
            old_surfaces(state, self.recipe, self.sources)

    def test_missing_source_ownership_cannot_pass_geometric_body_checks(self):
        bad = dict(self.data, owners=self.data["owners"].copy())
        bad["owners"][:, 0] = False
        with self.assertRaises(ValueError):
            audit(
                self.state,
                self.recipe,
                bad,
                self.rows,
                self.planning,
                self.line,
                self.upper,
            )

    def test_old_whole_pier_bodies_are_outside_new_foreign_traffic(self):
        self.assertEqual(len(self.state["piers"]), 5)
        for check in self.proof["traffic"]:
            self.assertGreater(check["minimum_whole_pier_xy_distance_m"], 3.6)
            self.assertEqual(check["bridge_overlap_area_m2"], 0)

    def test_old_boundary_heights_are_literal_and_all_ground_pins_are_finite(self):
        self.assertEqual(self.terrain["old_selected_faces"], 520)
        self.assertEqual(self.terrain["protected_parent_faces"], 10)
        self.assertLess(len(self.local["xy"]), 1000)
        boundary = self.local["boundary"]
        lookup = {
            tuple(p): h
            for p, h in zip(
                self.state["plan"]["xy"], self.state["heights"], strict=True
            )
        }
        expected = [lookup[tuple(p)] for p in self.local["xy"][boundary]]
        np.testing.assert_array_equal(self.heights[boundary], expected)
        self.assertTrue(
            np.isfinite(
                self.local["bed"][np.unique(self.local["faces"][self.local["ground"]])]
            ).all()
        )
        self.assertTrue(self.solve["accepted_field"])

    def test_old_extra_reserve_is_a_real_infeasible_countercase(self):
        strict = dict(self.local, bridge_reserve_m=np.array(0.1))
        heights, model, note = candidate(strict, self.upper, self.footprint)
        validate_candidate(strict, heights, model, note)
        self.assertFalse(note["accepted_field"])
        self.assertIn("Infeasible", note["solver_status"])

    def test_missing_explicit_boundary_pin_is_rejected(self):
        bad = dict(self.local, bed=self.local["bed"].copy())
        bad["bed"][np.flatnonzero(bad["boundary"])[0]] = np.nan
        with self.assertRaisesRegex(ValueError, "finite cached heights"):
            pins_for(bad)

    def test_provider_rejects_original_bridge_tags_as_ground(self):
        layer = copy.deepcopy(self.context)
        # Reuse actual provider output, changing only the original brunnel tag.
        from stair_junction_layout import INPUT
        from stair_module_inputs import decode_vector

        native = json.loads(INPUT.read_text())
        receipt = dict(
            layer["vector"], path=str(INPUT.with_name(native["origin"]["payload_file"]))
        )
        decoded = decode_vector(receipt)
        feature = next(
            f for f in decoded["transportation"]["features"] if f["id"] == 138663000
        )
        feature["properties"]["brunnel"] = "bridge"
        with (
            patch("stair_module_inputs.decode_vector", return_value=decoded),
            self.assertRaisesRegex(ValueError, "exterior ground"),
        ):
            inputs()


if __name__ == "__main__":
    unittest.main()
