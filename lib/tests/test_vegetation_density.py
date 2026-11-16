import unittest

import numpy as np
from shapely.geometry import GeometryCollection, box

from outshine_ref.vegetation.density_tile import canopy_inputs, spruce_density


LIMIT = {'treelineBaseLatDeg': 47.4, 'treelineBaseM': 1900,
         'treelinePerDegM': -58.8, 'treelineBandM': 200, 'slopeBandDeg': 4}


def density(ground, wood=box(0, 0, 20, 20), blocked=GeometryCollection(), latitude=47.4):
    return spruce_density(wood, blocked, ground, (0, 0, 20, 20), latitude,
                          per_m2=.033, height_m=35, alpine_limit=LIMIT, slope_limit_deg=35)


class VegetationDensity(unittest.TestCase):
    def test_forest_stays_out_of_unmapped_ground_and_reserved_area(self):
        result = density(np.full((3, 3), 1700), wood=box(0, 0, 15, 20),
                         blocked=box(0, 0, 10, 10))
        np.testing.assert_allclose(result.density, [[0, .0165], [.033, .0165]])
        self.assertEqual(result.height_m[0, 0], 0)
        self.assertEqual(result.height_m[1, 0], 35)

    def test_altitude_latitude_and_steep_terrain_reduce_woody_growth(self):
        self.assertAlmostEqual(float(density(np.full((3, 3), 2000)).density[0, 0]), .0165)
        self.assertTrue(np.all(density(np.full((3, 3), 2100)).density == 0))
        self.assertTrue(np.all(density(np.full((3, 3), 1500), latitude=61).density == 0))
        xx = np.tile(np.array([0, 10, 20]), (3, 1))
        slope37 = density(1600 + xx * np.tan(np.radians(37)))
        np.testing.assert_allclose(slope37.density, .0165, rtol=1e-5)
        slope39 = density(1600 + xx * np.tan(np.radians(39)))
        np.testing.assert_allclose(slope39.density, 0, atol=1e-14)

    def test_canopy_adds_height_to_negative_terrain_only_where_forest_exists(self):
        ground = np.full((3, 3), -50.)
        result = canopy_inputs(density(ground, wood=box(0, 0, 10, 10)), ground,
                               crown_width_m=6, leaf_albedo_linear=(.04, .09, .025))
        self.assertEqual((ground + result['height_offset_m'])[0, 0], -15)
        self.assertEqual((ground + result['height_offset_m'])[-1, -1], -50)
        self.assertEqual(result['coverage'][-1, -1], 0)
        self.assertAlmostEqual(float(result['coverage'][0, 0]), 1 - np.exp(-.033 * np.pi * 9))
        np.testing.assert_allclose(result['leaf_albedo_linear'][0, 0], (.04, .09, .025))

    def test_partial_forest_coverage_and_reserved_path_remain_open(self):
        ground = np.full((3, 3), 1700.)
        field = density(ground, wood=box(0, 0, 15, 20))
        result = canopy_inputs(field, ground, crown_width_m=6,
                               leaf_albedo_linear=(.04, .09, .025))
        occupied = 1 - np.exp(-.033 * np.pi * 9)
        np.testing.assert_allclose(result['coverage'][:, 1], .5 * occupied, rtol=1e-6)
        self.assertTrue(np.all(result['height_offset_m'][:, -1] == 0))
        road = density(ground, blocked=box(9, -1, 11, 21))
        along_road = canopy_inputs(road, ground, crown_width_m=6,
                                   leaf_albedo_linear=(.04, .09, .025))
        np.testing.assert_allclose(along_road['coverage'], .9 * occupied, rtol=1e-6)
        self.assertTrue(np.all(along_road['height_offset_m'][:, 1] == 0))
