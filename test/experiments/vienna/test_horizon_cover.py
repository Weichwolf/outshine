import unittest

import numpy as np
import shapely
from horizon_cover import rasterize_woodland


class HorizonWoodlandTest(unittest.TestCase):
    def test_coverage_uses_world_axis_order_and_keeps_a_clearance(self):
        woodland = shapely.box(-5, 2, 16.37, 15).difference(shapely.box(8, 7, 11, 12))
        footprints = [woodland, shapely.box(17, 11, 23, 19), shapely.box(9, 8, 11, 10)]
        cover = rasterize_woodland(footprints, (0, 0, 20, 20), size=128)

        np.testing.assert_allclose(cover.transform, (0, 0, 0.05, 0.05))
        self.assertEqual(cover.pixels.dtype, np.uint8)
        self.assertEqual(cover.pixels[25, 64], 255)
        self.assertEqual(cover.pixels[68, 57], 0)
        self.assertEqual(cover.pixels[60, 60], 255)
        self.assertEqual(cover.pixels[108, 120], 255)
        self.assertEqual(cover.pixels[120, 64], 0)
        self.assertTrue(np.any((cover.pixels > 0) & (cover.pixels < 255)))
        calculated_area = cover.pixels.mean() * 400 / 255
        domain = shapely.box(0, 0, 20, 20)
        expected = shapely.union_all(footprints).intersection(domain)
        supersample_diagonal = np.sqrt(2) * 20 / (2 * len(cover.pixels))
        boundary_band = expected.boundary.buffer(supersample_diagonal)
        self.assertAlmostEqual(
            calculated_area,
            expected.area,
            delta=boundary_band.intersection(domain).area,
        )
