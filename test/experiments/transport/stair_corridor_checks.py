"""Whole real outer corridors, protected cross sections and retained source inputs."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from shared_bridge_geometry import corridor
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_outer_geometry import outer_ends
from stair_terrain_layout import partition, selected_surfaces

CORRIDORS = FIXTURE.with_name("stair-cycle-corridors-vienna.json")
SOUTH = FIXTURE.with_name("stair-cycle-south-vienna.json")


class ActualCorridors(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.rows, cls.nodes, stairs, cls.context, sample = load(CORRIDORS)
        cls.sample = staticmethod(sample)
        p = layout(cls.rows, "width", junctions=cls.nodes, stairs=stairs)
        heights, note = solve(p)
        if heights is None:
            raise ValueError("whole corridor profile failed: " + str(note))
        _, cls.data, _ = build(p, heights)

    def test_all_twenty_one_previous_axes_priors_and_originals_are_retained(self):
        _, previous, _, _, _, _ = load(SOUTH)
        self.assertEqual(len(self.rows), 23)
        for before, after in zip(previous, self.rows[:21], strict=True):
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(before["original"], after["original"])
            np.testing.assert_array_equal(before["xy"], after["xy"])
            np.testing.assert_array_equal(before["prior"], after["prior"])

    def test_ten_actual_whole_endpoint_profiles_and_seven_explicit_frontiers(self):
        self.assertEqual(
            len(contacts(self.data, self.rows, bindings(CORRIDORS, self.rows))), 10
        )
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(16, 0), (18, -1), (20, 0), (20, -1), (21, 0), (21, -1), (22, 0)],
        )

    def test_existing_stairs_underpasses_whole_caps_and_vertex_links_pass(self):
        pairs, _whole = interactions(self.rows, self.nodes)
        _, previous, nodes, _, _, _ = load(SOUTH)
        before, _ = interactions(previous, nodes)
        self.assertTrue({tuple(p[:2]) for p in before} <= {tuple(p[:2]) for p in pairs})
        proof = audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        self.assertEqual(len(proof["clearance"]), 10)
        self.assertEqual(sum(r["risers"] for r in proof["treads"].values()), 24)
        vertex_links(self.data["body_vertices"], self.data["body_faces"])

    def test_uniform_fine_terrain_is_rejected_at_the_existing_budget(self):
        ground, _upper = selected_surfaces(self.data, self.rows)
        with self.assertRaisesRegex(ValueError, "soil input face cap"):
            partition(
                ground,
                self.context,
                None,
                sample=self.sample,
                interface="boundary_band",
            )

    def test_actual_short_neighbor_does_not_cover_a_whole_end_profile(self):
        recipe = json.loads(CORRIDORS.read_text())
        key = recipe["excluded_source_trials"][0]["source_id"]
        owner = recipe["excluded_source_trials"][0]["neighbor_source_id"]
        recipe["additional_source_ids"].append(key)
        recipe["protected_profiles"].append([owner, key, -1])
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_transport-"
        ) as tmp:
            path = Path(tmp) / "candidate.json"
            path.write_text(json.dumps(recipe))
            for row in recipe["elevation"]:
                (path.parent / row["window_file"]).symlink_to(
                    CORRIDORS.with_name(row["window_file"])
                )
            _, rows, nodes, stairs, _, _ = load(path)
            plan = layout(rows, "width", junctions=nodes, stairs=stairs)
            heights, note = solve(plan)
            self.assertIsNotNone(heights, note)
            _, data, _ = build(plan, heights)
            with self.assertRaisesRegex(ValueError, "misses a whole endpoint profile"):
                contacts(data, rows, (bindings(path, rows)[-1],))


if __name__ == "__main__":
    unittest.main()
