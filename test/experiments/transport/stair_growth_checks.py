"""Bounded asset-profile discovery and distinct-root admission scheduling controls."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from stair_growth_sources import asset_state, roots_first, select
from stair_local_assets import road_asset
from stair_local_checks import asset, terrain
from stair_module_roots import root_profile


class ResidentGrowth(unittest.TestCase):
    def test_resident_endpoint_uses_actual_whole_cap_without_outer_report(self):
        source = {"xy_m": np.array([[0, 1], [2, 1]]), "mvt_xy": [[0, 1], [2, 1]]}
        original = asset(0)
        row = {"source_id": "0:0", "xy": source["xy_m"], "width_m": 2.0}
        resident = road_asset("test", ["0:0"], [row], original.data)
        state = asset_state(resident)
        for end in (0, -1):
            _row, line = root_profile(
                state,
                {"port": {"source_id": "0:0", "end": end}, "contact_mode": "profile"},
                {"0:0": source},
            )
            self.assertEqual(line.shape, (3, 3))
            np.testing.assert_array_equal(line[:, 2], np.ones(3))
        row["width_m"] = 3.0
        with self.assertRaisesRegex(ValueError, "missing intervals"):
            root_profile(
                state,
                {"port": {"source_id": "0:0", "end": 0}, "contact_mode": "profile"},
                {"0:0": source},
            )

    def test_profile_state_shares_frozen_bodies_and_uses_local_owners(self):
        original = asset(9, sources=32)
        state = asset_state(original)
        self.assertIs(state["road"]["body_vertices"], original.data["body_vertices"])
        self.assertIs(state["road"]["top_faces"], original.data["top_faces"])
        self.assertIs(state["road_ground"], original.data["ground"])
        np.testing.assert_array_equal(
            state["road"]["face_sources"], np.full(2, 2**32 - 1, np.uint64)
        )
        state["road"]["face_sources"][:] = 0
        self.assertTrue(original.data["owners"].all())

    def test_distinct_roots_precede_smaller_prefixes_without_losing_any(self):
        recipes = [
            {"port": {"source_id": key, "vertex": vertex}, "source_ids": list(range(n))}
            for key, vertex, n in (
                ("a", 1, 8),
                ("a", 1, 4),
                ("a", 1, 2),
                ("a", 2, 3),
                ("b", 1, 1),
            )
        ]
        ordered = roots_first(recipes)
        self.assertEqual([len(r["source_ids"]) for r in ordered], [8, 3, 1, 4, 2])
        self.assertEqual({id(r) for r in ordered}, {id(r) for r in recipes})
        self.assertEqual(roots_first([]), [])

    def test_70_asset_sources_need_no_global_mask_and_share_64_probe_budget(self):
        assets, sources, old_keys = [], {}, []
        for i in range(70):
            old, new = "old-" + str(i), "new-" + str(i)
            x = 3 * i + 5
            nodes = [[x, 10], [x, 20], [x, 30]]
            for key, values in ((old, nodes), (new, [nodes[1], [x + 1, 20]])):
                xy = np.array(values, float)
                sources[key] = {
                    "xy_m": xy,
                    "mvt_xy": values,
                    "shape": shapely.LineString(xy),
                    "properties": {"class": "path", "subclass": "footway"},
                }
            data = asset(i).data
            assets.append(
                road_asset(
                    str(i), [old], [{"source_id": old, "xy": np.array(nodes)}], data
                )
            )
            old_keys.append(old)
        calls = []

        def profile(state, recipe, _sources):
            self.assertEqual(len(state["report"]["candidate_source_ids"]), 1)
            calls.append(recipe["port"])

        with patch("stair_frontier_sources.root_profile", side_effect=profile):
            result = select(terrain(), assets, {"source_ids": old_keys}, sources, {})
        self.assertEqual(len(calls), 64)
        self.assertEqual(
            sum(r["actual_profile_probes"] for r in result["profile_work"]), 64
        )
        self.assertEqual(len(result["roots"]), 64)
        self.assertEqual(len(result["deferred_profiles"]), 6)
        self.assertTrue(all(r["source_columns"] == 1 for r in result["profile_work"]))
        self.assertTrue(
            all(
                k.startswith("new-") for r in result["recipes"] for k in r["source_ids"]
            )
        )
        with self.assertRaisesRegex(ValueError, "schedule"):
            select(terrain(), [], {"source_ids": []}, {}, {}, schedule="unknown")


if __name__ == "__main__":
    unittest.main()
