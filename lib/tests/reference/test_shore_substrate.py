import unittest
from datetime import UTC, datetime
from types import SimpleNamespace

import numpy as np
from outshine.vegetation.open_cover import RegionalOpenCover
from outshine.water.shore_grade import shore_grade
from outshine.water.shore_substrate import BARE_WIDTH_M, mineral_shore
from outshine.water.water_surface import WaterBody
from shapely.geometry import Point, box


def weights_at(appearance, xy):
    east, north, sx, sy = appearance.transform
    height, width = appearance.pixels.shape[:2]
    x, y = np.floor((np.array(xy) - [east, north]) * [sx * width, sy * height]).astype(
        int
    )
    return appearance.pixels[y, x, :3]


class ShoreSubstrateTest(unittest.TestCase):
    def test_mapped_lake_supplies_material_weights_and_a_local_bare_margin(self):
        minerals = WaterBody(box(-35, -30, 15, 0), 0, water_class="lake", source_id="a")
        fresh = WaterBody(box(30, -20, 45, -5), 0, water_class="lake", source_id="b")
        grade = shore_grade((minerals, fresh), box(-90, -90, 90, 90), box(6, -3, 11, 3))
        self.assertIsNone(mineral_shore(grade, {("lake", "other"): "carbonate-gravel"}))
        margin = mineral_shore(grade, {("lake", "a"): "carbonate-gravel"})
        self.assertTrue(margin.bare.contains(Point(0, BARE_WIDTH_M - 0.1)))
        self.assertFalse(margin.bare.contains(Point(0, BARE_WIDTH_M + 0.1)))
        self.assertFalse(margin.bare.intersects(fresh.footprint))

        source = SimpleNamespace(
            lon=-72.8,
            lat=-50.97,
            metres=np.array([70000, 111132]),
            features=[],
        )
        product = margin.appearance(source, np.array([0, 5, 1.7]), 2083)
        wet, crust, gravel = weights_at(product, (-15, 0.1))
        self.assertGreater(wet, 240)
        self.assertEqual(crust, 0)
        self.assertGreater(gravel, 210)
        wet, crust, gravel = weights_at(product, (-15, 3.5))
        self.assertEqual(wet, 0)
        self.assertGreater(crust, 15)
        self.assertGreater(gravel, 210)
        np.testing.assert_array_equal(weights_at(product, (8, 1)), [0, 0, 0])
        np.testing.assert_array_equal(weights_at(product, (0, 8.9)), [0, 0, 0])

        atlas = SimpleNamespace(
            land_cells=lambda lat, lon: (np.full(len(lat), 30), np.full(len(lat), 57))
        )
        vegetation = RegionalOpenCover(
            atlas, source, datetime(2026, 12, 29, tzinfo=UTC), exclusion=margin.bare
        )
        np.testing.assert_array_equal(
            vegetation.replaces(np.array([[0, 2], [0, 8], [30, -4]])),
            [False, True, True],
        )
