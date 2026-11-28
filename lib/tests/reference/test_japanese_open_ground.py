import unittest
from datetime import UTC, datetime
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.vegetation.japanese_open_ground import (
    BOG,
    SILVERGRASS,
    open_ground_colour,
)
from outshine.vegetation.open_cover import (
    BOG_GRASS,
    JAPANESE_SILVERGRASS,
    RegionalOpenCover,
)

ORIGIN = np.array([138.16, 36.12])
METRES = np.array([89900, 111132])


def fixture_cover(offset=(0, 0)):
    bog = next(unit for unit in BOG if unit.endswith("470100"))
    grass = next(iter(SILVERGRASS))
    codes = {bog: 1, grass: 2}
    lon, lat = ORIGIN + offset / METRES
    atlas = SimpleNamespace(
        observation_indices=codes,
        observed_cells=lambda lat, lon: (
            np.where(
                lon < ORIGIN[0], 1, np.where(lon < ORIGIN[0] + 85 / METRES[0], 2, 0)
            ),
            np.zeros_like(lon),
        ),
        observation_mask=lambda indices, units: np.isin(
            indices, [codes[unit] for unit in units if unit in codes]
        ),
        land_cells=lambda lat, lon: (np.full_like(lon, 10), np.full_like(lon, 683)),
    )
    source = SimpleNamespace(
        lon=lon,
        lat=lat,
        metres=METRES,
        features=[
            ("landcover", shapely.box(-200, -200, 200, 200), {"class": "wood"}),
            ("water", shapely.box(-125, -15, -63, 15), {}),
            ("building", shapely.box(8, -15, 25, 15), {}),
        ],
    )
    return RegionalOpenCover(atlas, source, datetime(2026, 8, 17, tzinfo=UTC))


class JapaneseOpenGroundTest(unittest.TestCase):
    def test_mapped_open_ground_overrides_wood_without_planting_water_or_buildings(
        self,
    ):
        cover = fixture_cover()
        points = np.array([[-42, 0], [40, 0], [105, 0], [-75, 0], [15, 0]])
        np.testing.assert_array_equal(
            cover.classify(points), [BOG_GRASS, JAPANESE_SILVERGRASS, 0, 0, 0]
        )
        np.testing.assert_array_equal(
            shapely.contains_xy(cover.japanese_open_ground, *points[:3].T),
            [True, True, False],
        )

    def test_moss_and_peat_colour_varies_and_keeps_the_same_world_position(self):
        shift = np.array([26, -57])
        xy = np.array([[-8, -30], [-16, 0], [-30, 10], [45, 4]])
        bog = np.array([True, True, True, False])
        first = open_ground_colour(fixture_cover().source, xy, bog, 2083)
        second = open_ground_colour(fixture_cover(shift).source, xy - shift, bog, 2083)
        np.testing.assert_allclose(first, second, atol=1e-10, rtol=0)
        self.assertGreater(np.ptp(first[:3, 1]), 0.015)
