"""Whole seed geometry keeps asset ownership without a global 64-bit owner mask."""

import hashlib
import unittest

import numpy as np
import shapely
from stair_local_assets import road_window
from stair_local_checks import asset
from stair_reference_roads import seed_reference_roads


class WholeSeedRoads(unittest.TestCase):
    def test_all_66_sources_keep_geometry_and_local_owners(self):
        assets = tuple(asset(i, x=0, sources=22) for i in range(3))
        road, ground, keys, rows, note = seed_reference_roads(assets)
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
                seed_reference_roads(assets)
        from dataclasses import replace

        old = asset(0)
        data = dict(old.data, body_faces=np.tile(old.data["body_faces"], (2731, 1)))
        with self.assertRaisesRegex(ValueError, "body face budget"):
            seed_reference_roads([replace(old, data=data)])


if __name__ == "__main__":
    unittest.main()
