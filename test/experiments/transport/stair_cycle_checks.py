"""Actual eastern loop, source retention and the missing-band-contact countercase."""

import json
import unittest

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_cycle_contacts import contacts
from stair_cycle_inputs import load, original_inputs
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_network_inputs import sample_xy
from stair_outer_geometry import outer_ends
from stair_outer_inputs import FIXTURE as OUTER
from stair_outer_inputs import load as outer_inputs


class ActualCycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _case, cls.rows, cls.nodes, stairs, cls.context, sample = load()
        cls.sample = staticmethod(sample)
        cls.plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=stairs)
        heights, note = solve(cls.plan)
        if heights is None:
            raise ValueError("actual nineteen-source candidate failed: " + str(note))
        _body, cls.data, _geometry = build(cls.plan, heights)

    def test_original_axes_priors_features_and_ids_are_retained(self):
        _case, old, _nodes, _stairs, _context, _recipe = outer_inputs()
        for before, after in zip(old, self.rows[:18], strict=True):
            for key in ("xy", "prior"):
                np.testing.assert_array_equal(before[key], after[key])
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(before["original"], after["original"])
        _case, _rows, _ctx, sources = original_inputs()
        actual = sources[self.rows[-1]["source_id"]]
        np.testing.assert_array_equal(self.rows[-1]["xy"], actual["xy_m"])
        self.assertEqual(
            self.rows[-1]["original"]["source"]["feature"], actual["original_feature"]
        )

    def test_two_tile_windows_preserve_the_original_dem_without_extrapolation(self):
        recipe = json.loads(OUTER.read_text())
        raster = dict(np.load(OUTER.with_name(recipe["elevation"]["window_file"])))
        points = np.vstack([row["xy"] for row in self.rows[:18]])
        np.testing.assert_array_equal(
            self.sample(points), sample_xy(points, self.context, raster)
        )
        with self.assertRaisesRegex(ValueError, "declared DEM tiles"):
            self.sample(np.array([[-10000.0, 0]]))

    def test_node_only_recipe_cannot_admit_the_actual_near_axis_contact(self):
        _case, rows, nodes, _stairs, _context, _sample = load(band_join=False)
        self.assertEqual(len(self.nodes) - len(nodes), 1)
        with self.assertRaisesRegex(ValueError, "non-axis band overlap"):
            interactions(rows, nodes)
        near = [n for n in self.nodes if 18 in n["incident"] and n["mvt_xy"] is None]
        self.assertEqual(len(near), 1)
        distance = shapely.LineString(self.rows[-1]["xy"]).distance(
            shapely.Point(self.rows[17]["xy"][-1])
        )
        self.assertGreater(distance, 0.1)
        self.assertLess(distance, 0.2)

    def test_four_original_and_new_profiles_have_whole_shared_caps(self):
        self.assertEqual(len(contacts(self.data, self.rows)), 4)
        corrupted = dict(self.data)
        corrupted["face_sources"] = self.data["face_sources"] & ~(1 << 18)
        with self.assertRaisesRegex(ValueError, "coowned neighbour cap missing"):
            contacts(corrupted, self.rows)

    def test_outer_ports_move_to_real_sources_without_hiding_remaining_five(self):
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(15, 0), (15, -1), (16, 0), (18, 0), (18, -1)],
        )

    def test_six_underpasses_stairs_and_closed_links_still_pass(self):
        pairs, _whole = interactions(self.rows, self.nodes)
        self.assertEqual(len(pairs), 6)
        audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        vertex_links(self.data["body_vertices"], self.data["body_faces"])


if __name__ == "__main__":
    unittest.main()
