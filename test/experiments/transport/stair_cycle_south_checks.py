"""Whole southern source, genuine open endpoints and a required fourth DEM corner."""

import json
import unittest

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.places import tile_geography
from shared_bridge_geometry import corridor
from stair_blend_audit import continuous_cap
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, load, sampler
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_outer_geometry import outer_ends
from stair_terrain_layout import partition, selected_surfaces

SOUTH = FIXTURE.with_name("stair-cycle-south-vienna.json")
NORTH = FIXTURE.with_name("stair-cycle-north-vienna.json")


class AdjacentWindows(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe = json.loads(SOUTH.read_text())
        _case, _rows, _nodes, _stairs, cls.context, _sample = load(SOUTH)

    def test_two_rows_and_columns_have_four_distinct_verified_files(self):
        records = self.recipe["elevation"]
        self.assertEqual(
            {tuple(r["tile"]) for r in records},
            {(14, 8937, 5678), (14, 8938, 5678), (14, 8937, 5679), (14, 8938, 5679)},
        )
        self.assertEqual(len({r["window_file"] for r in records}), 4)
        for row in records:
            self.assertEqual(
                digest(SOUTH.with_name(row["window_file"])), row["window_sha256"]
            )

    def test_terrain_corner_cannot_silently_fall_back_to_three_declared_tiles(self):
        lat, lon = tile_geography(
            [[4104, 4104]],
            self.context["vector"]["extent"],
            tuple(self.context["vector"]["tile"]),
        )
        lat0, lon0 = self.context["origin_lat_lon"]
        xy = np.c_[
            (lon - lon0) * 111320 * np.cos(np.deg2rad(lat0)), (lat - lat0) * 111132
        ]
        self.assertTrue(
            np.isfinite(sampler(SOUTH, self.recipe, self.context)(xy)).all()
        )
        missing = dict(self.recipe, elevation=self.recipe["elevation"][:3])
        with self.assertRaisesRegex(ValueError, "beyond declared DEM tiles"):
            sampler(SOUTH, missing, self.context)(xy)

    def test_duplicate_or_unrelated_tiles_are_rejected(self):
        first = self.recipe["elevation"][0]
        for records in ([first, first], [dict(first, tile=[14, 8939, 5679])]):
            with self.assertRaisesRegex(ValueError, "unique adjacent DEM windows"):
                sampler(SOUTH, dict(self.recipe, elevation=records), self.context)


class ActualSouth(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _case, cls.rows, cls.nodes, stairs, cls.context, sample = load(SOUTH)
        cls.sample = staticmethod(sample)
        plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=stairs)
        h, note = solve(plan)
        if h is None:
            raise ValueError("actual south candidate failed: " + str(note))
        _body, cls.data, _geometry = build(plan, h)

    def test_existing_inputs_and_twenty_axes_priors_and_sources_are_unchanged(self):
        self.assertEqual(
            digest(NORTH),
            "0160906a6aae33b8469d5ed8a3523311d9715f275b5346d5fbf42a566468f9fc",
        )
        self.assertEqual(
            digest(FIXTURE),
            "b7e6681b8a5dc26a8508b981dca861d97af846c5355a243ec7a84d22d99c245e",
        )
        _case, old, _nodes, _stairs, _context, _sample = load(NORTH)
        for before, after in zip(old, self.rows[:20], strict=True):
            np.testing.assert_array_equal(before["xy"], after["xy"])
            np.testing.assert_array_equal(before["prior"], after["prior"])
            self.assertEqual(before["original"], after["original"])

    def test_six_whole_caps_do_not_falsely_bind_the_new_actual_end(self):
        self.assertEqual(
            len(contacts(self.data, self.rows, bindings(SOUTH, self.rows))), 6
        )
        with self.assertRaisesRegex(ValueError, "exactly one actual shared component"):
            contacts(self.data, self.rows, ((20, 15, -1),))
        real = [
            n["mvt_xy"]
            for n in self.nodes
            if 15 in n["incident"] and 20 in n["incident"] and n["mvt_xy"] is not None
        ]
        self.assertEqual(real, [[3198, 3435]])

    def test_existing_stairs_underpasses_and_closed_links_still_pass(self):
        pairs, _whole = interactions(self.rows, self.nodes)
        self.assertEqual(len(pairs), 6)
        audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        vertex_links(self.data["body_vertices"], self.data["body_faces"])

    def test_seven_actual_outer_profiles_remain_explicit(self):
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(16, 0), (18, 0), (18, -1), (19, 0), (19, -1), (20, 0), (20, -1)],
        )

    def test_four_tile_boundary_partition_preserves_original_ground_facets(self):
        ground, _upper = selected_surfaces(self.data, self.rows)
        p, _note = partition(
            ground, self.context, None, sample=self.sample, interface="boundary_band"
        )
        np.testing.assert_array_equal(
            p["xy"][p["faces"][p["ground"]]], ground.vertices[ground.triangles, :2]
        )
        xyz = np.c_[p["xy"], p["targets"]].astype(np.float32).astype(float)
        continuous_cap(Mesh(xyz, p["faces"]), p["faces"])


if __name__ == "__main__":
    unittest.main()
