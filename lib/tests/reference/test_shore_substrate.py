import unittest
from datetime import UTC, datetime
from types import SimpleNamespace

import numpy as np
from outshine.materials.aeolian_sand import SAND_PROFILES
from outshine.materials.palette import GROUND_BLENDS
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
    def test_wet_sand_varies_only_at_mapped_sandy_water_contacts(self):
        water = WaterBody(box(-35, -30, 15, 0), 0, water_class="pond")
        grade = shore_grade((water,), box(-90, -90, 90, 90), box(6, -3, 11, 3))
        mapped = box(-30, 0, 14, 8)
        profile = SAND_PROFILES[485]
        shore = mineral_shore(grade, {}, sand=profile, sand_area=mapped)
        source = SimpleNamespace(
            lon=-42.87, lat=-2.60, metres=np.array([111204, 111132]), features=[]
        )
        product = shore.appearance(source, np.array([0, 3, 1.7]), 2083)
        self.assertEqual(product.materials, {profile.sand})
        self.assertTrue(product.materials.isdisjoint(GROUND_BLENDS))
        self.assertEqual(product.wet[0], profile.wet)
        self.assertTrue(shore.bare.is_empty)
        self.assertGreater(weights_at(product, (-15, 0.1))[0], 220)
        self.assertGreater(weights_at(product, (-15, 1.2))[0], 35)
        self.assertEqual(weights_at(product, (-15, 3.8))[0], 0)
        np.testing.assert_array_equal(weights_at(product, (8, 1)), [0, 0, 0])
        self.assertFalse(product.pixels[..., 1:3].any())

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
        self.assertTrue(product.materials.isdisjoint(GROUND_BLENDS))
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
