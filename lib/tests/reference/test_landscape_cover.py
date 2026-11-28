import unittest
from datetime import UTC, datetime
from types import SimpleNamespace

import numpy as np
from outshine.vegetation.landscape_cover import landscape_cover
from outshine.vegetation.open_cover import RegionalOpenCover
from shapely.geometry import LineString, box

LATITUDE = -50.98
LONGITUDE = -72.8
METRES = np.array([70000, 111132])


def fixture_cover(offset=(0, 0)):
    shift = np.array(offset)
    lon, lat = np.array([LONGITUDE, LATITUDE]) + shift / METRES
    source = SimpleNamespace(
        lon=lon,
        lat=lat,
        metres=METRES,
        features=[
            (
                "transportation",
                LineString(np.array([[-300, 225], [300, 225]]) - shift),
                {"class": "service", "surface": "unpaved"},
            ),
            (
                "water",
                box(*(np.array([50, 250, 130, 320]) - np.tile(shift, 2))),
                {},
            ),
        ],
    )
    atlas = SimpleNamespace(
        community_indices={"CHL-2017:P70": 1},
        community_windows=lambda bounds: [(-72.807, -50.987, -72.793, -50.973)],
        land_cells=lambda latitude, longitude: (
            np.where(longitude < LONGITUDE, 60, 30),
            np.full(len(latitude), 561),
        ),
        community_mask=lambda latitude, longitude, codes: np.ones(
            len(latitude), dtype=bool
        ),
    )
    return RegionalOpenCover(atlas, source, datetime(2026, 12, 29, tzinfo=UTC))


def sample_pixel(product, xy):
    east, north, sx, sy = product.transform
    height, width = product.pixels.shape[:2]
    x, y = np.floor((np.array(xy) - [east, north]) * [sx * width, sy * height]).astype(
        int
    )
    return product.pixels[y, x]


class LandscapeCoverTest(unittest.TestCase):
    def test_landscape_fill_keeps_near_geometry_and_reserved_surfaces_clear(self):
        tile = landscape_cover(fixture_cover(), np.array([0, 0]), 2083)
        for position in ((0, 0), (80, 225), (90, 280)):
            np.testing.assert_array_equal(sample_pixel(tile, position), np.zeros(4))
        self.assertFalse(tile.pixels[[0, -1], :, :].any())
        self.assertFalse(tile.pixels[:, [0, -1], :].any())
        sparse = sample_pixel(tile, (-200, -200))
        grass = sample_pixel(tile, (200, -200))
        self.assertEqual(sparse[3], 255)
        self.assertEqual(grass[3], 255)
        self.assertGreater(sparse[0] / sparse[1], grass[0] / grass[1])

    def test_overlapping_cameras_share_sampled_albedo_at_same_coordinates(self):
        shift = np.array([41, -59])
        first = landscape_cover(fixture_cover(), np.array([0, 0]), 2083)
        second = landscape_cover(fixture_cover(shift), np.array([0, 0]), 2083)
        for point in np.array([[-206, -288], [240, -184], [-313, 176], [138, -198]]):
            np.testing.assert_array_equal(
                sample_pixel(first, point), sample_pixel(second, point - shift)
            )
