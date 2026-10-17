"""Whole geometry keeps actual asset ownership without a global 64-bit owner mask."""

import hashlib
import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from stair_local_assets import road_asset, road_window
from stair_local_checks import asset
from stair_reference_roads import reference_owner_indices, reference_roads
from stair_source_owners import source_selection


class WholeReferenceRoads(unittest.TestCase):
    def test_all_66_sources_keep_geometry_and_local_owners(self):
        assets = tuple(asset(i, x=0, sources=22) for i in range(3))
        road, ground, keys, rows, note = reference_roads(assets)
        self.assertNotIn("face_sources", road)
        self.assertEqual(len(keys), 66)
        self.assertEqual(rows, {})
        self.assertTrue(ground.all())
        np.testing.assert_array_equal(
            road["body_vertices"], np.vstack([a.data["body_vertices"] for a in assets])
        )
        for i, old in enumerate(assets):
            offset = i * len(old.data["body_vertices"])
            for name in ("body_faces", "top_faces", "bottom_faces"):
                n = len(old.data[name])
                np.testing.assert_array_equal(
                    road[name][i * n : (i + 1) * n], old.data[name] + offset
                )
            self.assertEqual(
                note["complete_assets"][i]["source_ids"], list(old.source_ids)
            )
            self.assertEqual(
                note["complete_assets"][i]["owner_sha256"],
                hashlib.sha256(old.data["owners"].tobytes()).hexdigest(),
            )
        road["body_vertices"][0, 0] = 99
        self.assertEqual(assets[0].data["body_vertices"][0, 0], 0)
        with self.assertRaisesRegex(ValueError, "source working budget"):
            road_window(assets, shapely.box(0, 0, 2, 2))

    def test_whole_geometry_has_unchanged_asset_and_face_limits(self):
        for assets in ([], [asset(0)] * 2, [asset(i) for i in range(17)]):
            with self.assertRaisesRegex(ValueError, "distinct complete"):
                reference_roads(assets)
        old = asset(0)
        data = dict(old.data, body_faces=np.tile(old.data["body_faces"], (2731, 1)))
        with self.assertRaisesRegex(ValueError, "body face budget"):
            reference_roads([replace(old, data=data)])

    def test_explicit_complete_control_preserves_more_than_64_assets_and_sources(self):
        assets = tuple(asset(i) for i in range(70))
        road, ground, keys, _rows, note = reference_roads(assets, max_assets=1024)
        self.assertEqual(len(keys), 70)
        self.assertTrue(ground.all())
        self.assertNotIn("face_sources", road)
        self.assertEqual(len(note["complete_assets"]), 70)
        self.assertEqual(note["asset_capacity"], 1024)
        for i, original in enumerate(assets):
            for name in ("body_faces", "top_faces", "bottom_faces"):
                count = len(original.data[name])
                np.testing.assert_array_equal(
                    road[name][i * count : (i + 1) * count],
                    original.data[name] + i * len(original.data["body_vertices"]),
                )
        indices = reference_owner_indices(assets)
        np.testing.assert_array_equal(indices[keys[69]], [138, 139])
        with self.assertRaisesRegex(ValueError, "asset/source working budget"):
            road_window(
                tuple(asset(i, x=0) for i in range(17)), shapely.box(0, 0, 2, 2)
            )

    def test_full_control_capacity_and_bytes_reject_before_geometry_copies(self):
        old = asset(0)
        for capacity in (False, 0, 1025, 1.5):
            with self.assertRaisesRegex(ValueError, "explicit complete-control"):
                reference_roads([old], max_assets=capacity)
        data = dict(
            old.data,
            body_vertices=np.broadcast_to(old.data["body_vertices"][:1], (400000, 3)),
        )
        with (
            patch(
                "stair_reference_roads.np.vstack", side_effect=AssertionError("copied")
            ),
            self.assertRaisesRegex(ValueError, "copy byte budget"),
        ):
            reference_roads([replace(old, data=data)], max_assets=1024)

    def test_source_65_selects_only_its_actual_asset_facets_without_a_mask(self):
        last = asset(2, sources=22)
        owners = np.array([[i < 11 for i in range(22)], [i >= 11 for i in range(22)]])
        last = road_asset(
            last.identity, last.source_ids, last.rows, dict(last.data, owners=owners)
        )
        assets = (asset(0, sources=22), asset(1, sources=22), last)
        road, _ground, keys, _rows, _note = reference_roads(assets)
        indices = reference_owner_indices(assets)
        state = {"road": road, "road_source_faces": indices}
        np.testing.assert_array_equal(source_selection(state, keys[65], keys), [5])
        self.assertEqual(set(indices), set(keys))
        for old in assets:
            local, _ground, local_keys, _rows, _note = road_window([old], old.shapes[0])
            for key in local_keys:
                actual = road["top_faces"][source_selection(state, key, keys)]
                offset = keys.index(key) // 22 * len(old.data["body_vertices"])
                expected = (
                    local["top_faces"][
                        source_selection({"road": local}, key, local_keys)
                    ]
                    + offset
                )
                np.testing.assert_array_equal(actual, expected)
        with self.assertRaises(ValueError):
            indices[keys[65]][0] = 0
        with self.assertRaises(ValueError):
            source_selection(state, "unknown", keys)


if __name__ == "__main__":
    unittest.main()
