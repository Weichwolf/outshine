"""Same bounded operator on a second whole-source input and its real north interface."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from shared_bridge_geometry import corridor
from stair_blend_audit import continuous_cap
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, SOURCE, load, original_inputs, selected_sources
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_outer_geometry import outer_ends
from stair_terrain_layout import partition, selected_surfaces

NORTH = FIXTURE.with_name("stair-cycle-north-vienna.json")


class RecipeBounds(unittest.TestCase):
    def test_source_count_duplicates_and_absent_sources_are_rejected(self):
        available = {SOURCE: {"properties": {}}}
        for keys in ([], [SOURCE] * 7, [SOURCE, SOURCE], ["absent"]):
            with self.assertRaises(ValueError):
                selected_sources({"additional_source_ids": keys}, available)

    def test_indoor_or_separated_level_sources_need_another_recipe(self):
        for properties in (
            {"indoor": 1},
            {"layer": 2},
            {"level": 1},
            {"brunnel": "tunnel"},
        ):
            with self.assertRaisesRegex(ValueError, "ordinary exterior"):
                selected_sources(
                    {"source_id": SOURCE}, {SOURCE: {"properties": properties}}
                )

    def test_extended_input_cannot_silently_lose_protected_profiles(self):
        rows = [{"source_id": "a"}, {"source_id": "b"}]
        for declarations in (None, [], [["a", "b", 0]] * 2, [["a", "a", 0]]):
            recipe = {"additional_source_ids": ["a"]}
            if declarations is not None:
                recipe["protected_profiles"] = declarations
            with tempfile.TemporaryDirectory(
                prefix="outshine-tests.outshine_transport-"
            ) as folder:
                path = Path(folder) / "input.json"
                path.write_text(json.dumps(recipe))
                with self.assertRaises(ValueError):
                    bindings(path, rows)


class ActualNorth(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _case, cls.rows, cls.nodes, stairs, cls.context, sample = load(NORTH)
        cls.sample = staticmethod(sample)
        plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=stairs)
        heights, note = solve(plan)
        if heights is None:
            raise ValueError("actual north candidate failed: " + str(note))
        _body, cls.data, _geometry = build(plan, heights)

    def test_previous_input_and_all_nineteen_axes_priors_features_are_preserved(self):
        self.assertEqual(
            digest(FIXTURE),
            "b7e6681b8a5dc26a8508b981dca861d97af846c5355a243ec7a84d22d99c245e",
        )
        _case, old, _nodes, _stairs, _context, _sample = load()
        for before, after in zip(old, self.rows[:19], strict=True):
            np.testing.assert_array_equal(before["xy"], after["xy"])
            np.testing.assert_array_equal(before["prior"], after["prior"])
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(before["original"], after["original"])
        _case, _rows, _context, sources = original_inputs()
        actual = sources[self.rows[-1]["source_id"]]
        np.testing.assert_array_equal(self.rows[-1]["xy"], actual["xy_m"])

    def test_raw_shared_vertices_and_all_five_whole_protected_caps(self):
        nodes = [n for n in self.nodes if 15 in n["incident"] and 19 in n["incident"]]
        self.assertEqual(
            {tuple(n["mvt_xy"]) for n in nodes if n["mvt_xy"] is not None},
            {(2158, 1789)},
        )
        self.assertEqual(sum(n["mvt_xy"] is None for n in nodes), 1)
        _case, _rows, _context, sources = original_inputs()
        a = sources[self.rows[15]["source_id"]]["mvt_xy"]
        b = sources[self.rows[19]["source_id"]]["mvt_xy"]
        self.assertEqual({tuple(p) for p in a if p in b}, {(2677, 2591), (2158, 1789)})
        self.assertEqual(
            len(contacts(self.data, self.rows, bindings(NORTH, self.rows))), 5
        )
        corrupted = dict(self.data)
        corrupted["face_sources"] = self.data["face_sources"] & ~(1 << 19)
        with self.assertRaisesRegex(ValueError, "coowned neighbour cap missing"):
            contacts(corrupted, self.rows, bindings(NORTH, self.rows))

    def test_existing_underpasses_stairs_and_closed_links_remain_valid(self):
        pairs, _whole = interactions(self.rows, self.nodes)
        self.assertEqual(len(pairs), 6)
        audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        vertex_links(self.data["body_vertices"], self.data["body_faces"])

    def test_boundary_band_preserves_every_actual_ground_facet_in_both_precisions(self):
        ground, _upper = selected_surfaces(self.data, self.rows)
        legacy, _note = partition(ground, self.context, None, sample=self.sample)
        legacy_xyz = (
            np.c_[legacy["xy"], legacy["targets"]].astype(np.float32).astype(float)
        )
        with self.assertRaisesRegex(ValueError, "overlaps|inverted"):
            continuous_cap(Mesh(legacy_xyz, legacy["faces"]), legacy["faces"])
        planned, _note = partition(
            ground, self.context, None, sample=self.sample, interface="boundary_band"
        )
        actual = planned["xy"][planned["faces"][planned["ground"]]]
        np.testing.assert_array_equal(actual, ground.vertices[ground.triangles, :2])
        for quantized in (False, True):
            xyz = np.c_[planned["xy"], planned["targets"]]
            if quantized:
                xyz = xyz.astype(np.float32).astype(float)
            continuous_cap(Mesh(xyz, planned["faces"]), planned["faces"])

    def test_north_profile_is_bound_but_two_new_actual_ends_stay_open(self):
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(15, 0), (16, 0), (18, 0), (18, -1), (19, 0), (19, -1)],
        )


if __name__ == "__main__":
    unittest.main()
