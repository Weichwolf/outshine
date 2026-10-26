"""Fine lakes, dry-child supersession and no overlapping coarse water."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from place_scene_checks import ConstantGround, camera, feature
from place_water import source_regions, water


class WaterCoverageChecks(unittest.TestCase):
    def test_fine_lake_is_retained_and_coarse_copy_is_superseded(self):
        coarse = {"kind": "vector", "tile": [8, 128, 128], "sha256": "coarse"}
        fine = {"kind": "vector", "tile": [14, 8192, 8192], "sha256": "fine"}

        def decoded(r):
            size = 64 if r is coarse else 4096
            ring = [[0, 0], [size, 0], [size, size], [0, size], [0, 0]]
            return {
                "water": {
                    "extent": 4096,
                    "features": [
                        feature({"type": "Polygon", "coordinates": [ring]}, "lake")
                    ],
                }
            }

        with patch("place_water.decode_vector", side_effect=decoded):
            mesh, report = water(camera(), ConstantGround(), [coarse, fine])
        self.assertGreater(len(mesh.triangles), 0)
        self.assertEqual(len(report["sources"]), 1)
        self.assertEqual(report["sources"][0]["source_sha256"], "fine")
        self.assertEqual(len(report["rejected"]), 1)
        self.assertIn("superseded", report["rejected"][0]["reason"])

    def test_tile_priority_is_exact_disjoint_and_receipt_order_independent(self):
        receipts = [
            {"kind": "vector", "tile": tile}
            for tile in ([8, 128, 128], [14, 8192, 8192], [14, 8193, 8192])
        ]
        regions = source_regions(receipts)
        backwards = source_regions(receipts[::-1])
        values = list(regions.values())
        for key, region in regions.items():
            self.assertTrue(region.equals(backwards[key]))
        for i, a in enumerate(values):
            for b in values[i + 1 :]:
                self.assertEqual(a.intersection(b).area, 0)
        self.assertEqual(shapely.union_all(values).area, (1 / 256) ** 2)

    def test_fine_dry_tile_also_suppresses_obsolete_coarse_lake(self):
        coarse = {"kind": "vector", "tile": [8, 128, 128], "sha256": "coarse"}
        fine = {"kind": "vector", "tile": [14, 8192, 8192], "sha256": "fine"}
        ring = [[0, 0], [64, 0], [64, 64], [0, 64], [0, 0]]

        def decoded(r):
            return {
                "water": {
                    "extent": 4096,
                    "features": [
                        feature({"type": "Polygon", "coordinates": [ring]}, "lake")
                    ]
                    if r is coarse
                    else [],
                }
            }

        with patch("place_water.decode_vector", side_effect=decoded):
            mesh, report = water(camera(), ConstantGround(), [coarse, fine])
        np.testing.assert_array_equal(mesh.vertices.shape, (0, 3))
        self.assertEqual(report["sources"], [])
        self.assertEqual(len(report["rejected"]), 1)


if __name__ == "__main__":
    unittest.main()
