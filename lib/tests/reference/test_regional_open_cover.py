import unittest
from datetime import UTC, datetime
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from outshine.vegetation.open_cover import PATAGONIAN, RegionalOpenCover
from outshine.vegetation.plant_sites import scatter_mesh
from shapely.geometry import box


def slope(lower, upper):
    xy = np.asarray(((lower, -8), (upper, -8), (upper, 8), (lower, 8)))
    return np.column_stack((xy, xy @ [0.12, 0.04])), np.array(((0, 1, 2), (0, 2, 3)))


class RegionalOpenCoverTest(unittest.TestCase):
    def test_steppe_requires_mapped_potential_community_and_open_observed_cover(self):
        source = SimpleNamespace(
            lon=-72.8,
            lat=-50.98,
            metres=np.array([70000, 111000]),
            features=[("water", box(1, -1, 3, 1), {})],
        )
        atlas = SimpleNamespace(
            community_indices={"CHL-2017:P70": 1},
            land_cells=lambda lat, lon: (
                np.array([60, 60, 60, 10]),
                np.full(len(lat), 561),
            ),
            community_mask=lambda lat, lon, codes: lon > -72.8001,
        )
        cover = RegionalOpenCover(atlas, source, datetime(2026, 12, 29, tzinfo=UTC))
        xy = np.array([[-14.0, 0.0], [2.0, 0.0], [4.0, 0.0], [6.0, 0.0]])
        np.testing.assert_array_equal(cover.classify(xy), [0, 0, PATAGONIAN, 0])

    def test_tufts_root_on_the_grade_and_respect_reserved_and_mapped_areas(self):
        source = SimpleNamespace(
            lon=35.0,
            lat=-2.0,
            metres=np.array([111000, 111000]),
            features=[("building", box(5, -2, 7, 2), {})],
        )
        atlas = SimpleNamespace(
            land_cells=lambda lat, lon: (np.full(len(lat), 30), np.full(len(lat), 57))
        )
        cover = RegionalOpenCover(atlas, source, datetime(2026, 8, 12, tzinfo=UTC))
        ground = slope(-8, 0.7)
        grass = slope(0.7, 8)
        road = slope(-1, 1)
        points = np.array([[-4, 0], [0, 0], [4, 0], [6, 0]], dtype=float)
        marks = np.zeros((len(points), 3))
        with (
            patch(
                "outshine.vegetation.open_cover.tile_sites",
                return_value=iter([(points, marks)]),
            ),
            patch(
                "outshine.vegetation.open_cover.scatter_mesh", wraps=scatter_mesh
            ) as scatter,
        ):
            batches = list(
                cover.batches(
                    {"ground": ground, "grass": grass, "road": road},
                    np.array([0, 0, 1.7]),
                    2083,
                )
            )
        self.assertEqual([name for name, *_ in batches], ["savanna straw blades"])
        np.testing.assert_allclose(
            scatter.call_args.args[1], [[-4, 0, -0.48], [4, 0, 0.48]]
        )
        footprint = box(-8, -8, -1, 8).union(box(1, -8, 8, 8))
        self.assertTrue(
            shapely.covers(footprint, shapely.points(batches[0][1][:, :2])).all()
        )
