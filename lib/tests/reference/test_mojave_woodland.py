import unittest
from datetime import UTC, datetime
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from outshine.vegetation.mojave_sites import mojave_woodland
from outshine.vegetation.muller_oak import MULLER_OAK_PROFILE, MULLER_OAK_TAXON
from outshine.vegetation.open_cover import MOJAVE_DESERT, RegionalOpenCover
from outshine.vegetation.pinyon import (
    BARK,
    FOLIAGE,
    PINYON_PROFILE,
    PINYON_TAXON,
    pinyon,
)
from shapely.geometry import box


def slope(xy):
    return np.asarray(xy) @ [0.09, 0.02]


def ground(left, right):
    xy = np.array(((left, -6), (right, -6), (right, 6), (left, 6)))
    return np.column_stack((xy, slope(xy))), np.array(((0, 1, 2), (0, 2, 3)))


class MojaveWoodlandTest(unittest.TestCase):
    def test_pinyon_lods_keep_the_bent_structural_limbs(self):
        low, _ = pinyon(0, False)
        high, _ = pinyon(0, True)
        for near, far in zip(high[BARK], low[BARK], strict=True):
            np.testing.assert_array_equal(near, far)
        bark = high[BARK][0]
        self.assertLess(np.min(bark[:, 2]), 0.0)
        self.assertGreater(np.ptp(bark[:, 0]), 3.0)
        self.assertGreater(np.ptp(bark[:, 2]), 3.0)
        for detailed, coarse in zip(high[FOLIAGE], low[FOLIAGE], strict=True):
            np.testing.assert_array_equal(detailed[: len(coarse)], coarse)
            self.assertGreater(len(detailed), len(coarse))

    def test_woodland_uses_the_mapped_association_and_solved_forest_floor(self):
        source = SimpleNamespace(
            lat=34.0,
            lon=-116.16,
            metres=np.array([92000.0, 111000.0]),
            features=[("landcover", box(-20, -6, 20, 6), {"class": "wood"})],
        )
        units = {1: "JOTR-2012:10161", 2: "JOTR-2012:29032", 3: "JOTR-2012:90200"}

        def observed_cells(lat, lon):
            east = (lon - source.lon) * source.metres[0]
            ids = np.select((east > 11, east > 8), (3, 2), default=1)
            return ids, np.where(ids == 3, 9, 5)

        atlas = SimpleNamespace(
            observation_indices=units,
            evidence_windows=lambda *_: [(-117, 33, -116, 35)],
            observed_cells=observed_cells,
            observation_mask=lambda ids, codes: np.isin(
                ids, [number for number, code in units.items() if code in codes]
            ),
            land_cells=lambda lat, lon: (
                np.full(len(lat), 20),
                np.full(len(lat), MOJAVE_DESERT),
            ),
            sample=lambda lat, lon: SimpleNamespace(
                native_candidates=(PINYON_TAXON, MULLER_OAK_TAXON)
            ),
        )
        cover = RegionalOpenCover(atlas, source, datetime(2026, 11, 14, tzinfo=UTC))
        sites = np.array([[-7.0, 1], [0, 0], [9, 0], [12, 0]])
        marks = np.zeros((len(sites), 3))
        surfaces = {"forest floor": ground(-19, 19), "unpaved path": ground(-1, 1)}
        with patch(
            "outshine.vegetation.mojave_sites.tile_sites",
            side_effect=lambda *args, **kwargs: iter([(sites, marks)]),
        ):
            batches = list(mojave_woodland(cover, surfaces, np.zeros(2), 2083))
        self.assertEqual(
            [profile for profile, *_ in batches], [PINYON_PROFILE, MULLER_OAK_PROFILE]
        )
        for (_, _, _, poses), buried in zip(batches, (0.0, 0.065), strict=True):
            np.testing.assert_array_equal(poses[:, :2], sites[:1])
            np.testing.assert_allclose(poses[:, 2], slope(sites[:1]) - buried)
