"""Original pending-node bindings and whole level regions without phantom sources."""

import copy
import unittest

import numpy as np
from stair_closed_seed_checks import ring_example
from stair_module_node_heights import inspect_heights
from stair_seed_geometry import build, plans_for
from stair_seed_pending import bind_pending


def pending_example():
    sources, rows, recipe, state, empty = ring_example()
    sources["a"]["properties"] = {"class": "path", "subclass": "footway"}
    sources["b"] = {
        "mvt_xy": [[20, 20], [30, 30]],
        "xy_m": [[20, 20], [30, 30]],
        "properties": {"class": "minor"},
    }
    rows[0]["width_m"] = 1.8
    rows[0]["prior"] = 100.0 + rows[0]["xy"] @ [0.01, 0.02]
    recipe["pending_junctions"] = [
        {
            "source_id": "a",
            "vertex": 2,
            "neighbor_source_id": "b",
            "neighbor_vertex": 0,
        }
    ]
    profiles = {"footway": {"widthM": 1.8}, "unclassified": {"widthM": 5.5}}
    return sources, rows, recipe, state, empty, profiles


class PendingSeedChecks(unittest.TestCase):
    def test_full_level_region_is_checked_in_both_precisions_without_admitting_neighbor(
        self,
    ):
        sources, rows, recipe, _state, empty, profiles = pending_example()
        before = copy.deepcopy(sources)
        bind_pending(rows, recipe, sources, profiles)
        data, _network, planning = build(
            rows, sources, empty, lambda xy: 100.0 + xy @ [0.01, 0.02]
        )
        plans = plans_for(rows, planning[0])
        self.assertEqual(len(plans), 1)
        self.assertEqual(data["owners"].shape[1], 1)
        self.assertEqual(recipe["source_ids"], ["a"])
        self.assertEqual(sources, before)
        checks = inspect_heights(data, plans)
        self.assertEqual(len(checks), 2)
        self.assertTrue(
            all(c["maximum_height_error_m"] <= c["z_guard_m"] for c in checks)
        )
        damaged = dict(data, surface_vertices=data["surface_vertices"].copy())
        damaged["surface_vertices"][data["fixed"], 2] += 0.01
        with self.assertRaisesRegex(ValueError, "not level"):
            inspect_heights(damaged, plans)

    def test_default_rows_stay_unchanged_without_pending_declarations(self):
        sources, rows, recipe, *_rest, profiles = pending_example()
        recipe.pop("pending_junctions")
        before = copy.deepcopy(rows)
        bind_pending(rows, recipe, sources, profiles)
        self.assertEqual(rows[0].keys(), before[0].keys())
        self.assertNotIn("pending_junctions", rows[0])
        np.testing.assert_array_equal(rows[0]["prior"], before[0]["prior"])

    def test_wrong_original_node_frame_level_and_source_are_rejected(self):
        for change in ("node", "frame", "bridge", "unknown", "already_selected"):
            sources, rows, recipe, *_rest, profiles = pending_example()
            if change == "node":
                sources["b"]["mvt_xy"][0] = [21, 20]
            elif change == "frame":
                sources["b"]["xy_m"][0] = [20.001, 20]
            elif change == "bridge":
                sources["b"]["properties"]["brunnel"] = "bridge"
            elif change == "unknown":
                recipe["pending_junctions"][0]["neighbor_source_id"] = "missing"
            else:
                recipe["pending_junctions"][0]["neighbor_source_id"] = "a"
            with self.subTest(change=change), self.assertRaises(ValueError):
                bind_pending(rows, recipe, sources, profiles)

    def test_duplicate_overbudget_wrong_family_and_closure_alias_are_rejected(self):
        for change in ("duplicate", "capacity", "family", "alias", "noninteger"):
            sources, rows, recipe, *_rest, profiles = pending_example()
            if change == "duplicate":
                recipe["pending_junctions"] *= 2
            elif change == "capacity":
                recipe["pending_junctions"] *= 5
            elif change == "family":
                recipe["contact_mode"] = "profile"
            elif change == "alias":
                recipe["pending_junctions"][0]["vertex"] = 4
            else:
                recipe["pending_junctions"][0]["vertex"] = True
            with self.subTest(change=change), self.assertRaises(ValueError):
                bind_pending(rows, recipe, sources, profiles)


if __name__ == "__main__":
    unittest.main()
