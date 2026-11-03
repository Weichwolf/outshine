"""Native-page sampling and WGS84 reconstruction controls for #515."""

import unittest

import numpy as np

from terrain_clipmap_source import NativeTerrain


def page(zoom=0, x=0, y=0, height=None, postings=33):
    row = dict(zoom=zoom, x=x, y=y, side=33, postings=postings,
               virtual=postings == 33, source_zoom=17 if postings == 33 else -1,
               nodes=35 * 35)
    if height is None:
        height = np.zeros((35, 35), dtype="<f4")
    return row, np.asarray(height, dtype="<f4").tobytes()


def lon_lat(zoom, x, y):
    scale = 1 << zoom
    return np.column_stack((np.asarray(x) / scale * 360 - 180,
                             np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * np.asarray(y) / scale))))))


class NativeHeightSampling(unittest.TestCase):
    def test_triangle_interpolation_retains_the_native_diagonal(self):
        y, x = np.indices((35, 35), dtype=np.float32)
        source = NativeTerrain((0, 0), [page(height=(x - 1) * (y - 1))])
        u, v = 12.75 / 32, 9.25 / 32
        result = source.heights_lon_lat(lon_lat(0, [u], [v]))[0]
        expected = 0.25 * (12 * 9) + 0.5 * (13 * 9) + 0.25 * (13 * 10)
        self.assertAlmostEqual(result, expected, places=10)
        self.assertNotAlmostEqual(result, 12.75 * 9.25, places=3)

    def test_native_nonuniform_postings_remain_nonuniform(self):
        _, x = np.indices((35, 35), dtype=np.float32)
        source = NativeTerrain((0, 0), [page(height=x - 1, postings=64)])
        fraction = np.floor(13 * 63 / 32) / 63
        result = source.heights_lon_lat(lon_lat(0, [fraction], [0.5]))[0]
        self.assertAlmostEqual(result, 13, places=10)

    def test_missing_data_stays_unknown_and_known_parent_is_explicit(self):
        point = lon_lat(1, [0.5], [0.5])
        source = NativeTerrain((0, 0), [page(1, 1, 1)])
        self.assertTrue(np.isnan(source.heights_lon_lat(point)[0]))
        parent = NativeTerrain((0, 0), [page(), page(1, 1, 1)])
        self.assertEqual(parent.heights_lon_lat(point)[0], 0)

    def test_invalid_latitudes_do_not_wrap_into_a_valid_tile(self):
        source = NativeTerrain((0, 0), [page()])
        with np.errstate(invalid="raise"):
            result = source.heights_lon_lat([[0, 90], [np.inf, 0], [0, np.nan]])
        self.assertTrue(np.all(np.isnan(result)))

    def test_reconstructed_height_includes_earth_curvature(self):
        for altitude in [0.0, 100.0]:
            source = NativeTerrain((0, 0), [page(height=np.full((35, 35), altitude))])
            east = np.array([0, 1000, 10000], dtype=np.float64)
            radius = 6378137.0 + altitude
            expected = altitude - east * east / (radius + np.sqrt(radius * radius - east * east))
            np.testing.assert_allclose(source(np.column_stack((east, np.zeros_like(east)))),
                                       expected, rtol=0, atol=2e-8)

    def test_duplicate_pages_are_not_arbitrarily_selected(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            NativeTerrain((0, 0), [page(), page()])


if __name__ == "__main__":
    unittest.main()
