import unittest
from datetime import UTC, datetime
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from outshine.materials.palette import GROUND_BLENDS
from outshine.terrain.ground_surface import MINERAL_TRAIL_SHOULDER_M
from outshine.vegetation.blackbrush import BLACKBRUSH_PROFILE, BLACKBRUSH_TAXON
from outshine.vegetation.mojave_sites import mojave_blackbrush, mojave_yuccas
from outshine.vegetation.open_cover import MOJAVE_DESERT, RegionalOpenCover
from outshine.vegetation.yuccas import (
    JOSHUA_BARK,
    JOSHUA_PROFILE,
    JOSHUA_TAXON,
    yucca_geometry,
)
from outshine.world.mesh import Meshes
from outshine.world.scene import ReferenceScene
from shapely.geometry import LineString, box


def grade(xy):
    return np.asarray(xy) @ [0.11, 0.04]


def surface(x0, x1):
    xy = np.array(((x0, -10), (x1, -10), (x1, 10), (x0, 10)))
    return np.column_stack((xy, grade(xy))), np.array(((0, 1, 2), (0, 2, 3)))


class MojaveYuccasTest(unittest.TestCase):
    def test_lod_keeps_the_same_woody_axes_and_reduces_leaf_meshes(self):
        coarse, _ = yucca_geometry(JOSHUA_TAXON, 2, True, False)
        detailed, _ = yucca_geometry(JOSHUA_TAXON, 2, True, True)
        for actual, expected in zip(
            coarse[JOSHUA_BARK], detailed[JOSHUA_BARK], strict=True
        ):
            np.testing.assert_array_equal(actual, expected)
        leaves = "joshua tree leaves"
        self.assertLess(2 * len(coarse[leaves][1]), len(detailed[leaves][1]))
        mature = np.asarray(detailed[JOSHUA_BARK][0])
        young = np.asarray(
            yucca_geometry(JOSHUA_TAXON, 2, False, True)[0][JOSHUA_BARK][0]
        )
        self.assertLess(mature[:, 2].min(), 0)
        self.assertGreater(mature[:, 2].max(), young[:, 2].max() + 0.8)

    def test_native_filter_and_setbacks_keep_yuccas_on_the_solved_ground(self):
        atlas = SimpleNamespace(
            land_cells=lambda lat, lon: (
                np.full(len(lat), 20),
                np.full(len(lat), MOJAVE_DESERT),
            ),
            sample=lambda lat, lon: SimpleNamespace(native_candidates=(JOSHUA_TAXON,)),
        )
        source = SimpleNamespace(
            lon=-116.1,
            lat=34.0,
            metres=np.array([92000.0, 111000.0]),
            height=grade,
            features=[("water", box(10, -2, 14, 2), {})],
        )
        cover = RegionalOpenCover(atlas, source, datetime(2026, 11, 14, tzinfo=UTC))
        xy = np.array([[-8.0, 2], [0.0, 0], [1.15, 0], [7.0, 0], [12.0, 0]])
        marks = np.zeros((len(xy), 3))
        with patch(
            "outshine.vegetation.mojave_sites.tile_sites",
            side_effect=lambda *args, **kwargs: iter([(xy, marks)]),
        ):
            batches = list(
                mojave_yuccas(
                    cover,
                    {"ground": surface(-16, 16), "unpaved path": surface(-1, 1)},
                    np.zeros(2),
                    2083,
                )
            )
        self.assertEqual([batch[0] for batch in batches], [JOSHUA_PROFILE])
        positions = batches[0][3]
        np.testing.assert_array_equal(positions[:, :2], xy[[0, 3]])
        np.testing.assert_allclose(positions[:, 2], grade(xy[[0, 3]]))

    def test_compaction_fades_from_the_footpath_to_matching_gravelly_sand(self):
        scene = object.__new__(ReferenceScene)
        scene.local = Meshes()
        scene.local.mineral_trail_edges = LineString([(0, -2), (0, 2)])
        width = MINERAL_TRAIL_SHOULDER_M
        vertices = np.array([[0, 0, 0], [0.5 * width, 0, 0], [width, 0, 0]])
        scene.local["unpaved path"] = vertices, []
        np.testing.assert_allclose(
            scene.ground_blend("mojave footpath", vertices), [1, 0.5, 0]
        )
        self.assertEqual(GROUND_BLENDS["mojave footpath"], "mojave gravelly sand")

    def test_blackbrush_requires_an_observed_association_and_root_clearance(self):
        source = SimpleNamespace(
            lon=-116.1,
            lat=34.0,
            metres=np.array([92000.0, 111000.0]),
            height=grade,
            features=[],
        )
        code = {1: "JOTR-2012:29032", 2: "JOTR-2012:90200", 3: "JOTR-2012:10161"}

        def observed_cells(lat, lon):
            east = (lon - source.lon) * source.metres[0]
            indices = np.select((east > 9, east > 7), (2, 3), default=1)
            return indices, np.where(indices == 2, 9, 5)

        atlas = SimpleNamespace(
            observation_indices=code,
            evidence_windows=lambda *_: [(-117, 33, -116, 35)],
            observed_cells=observed_cells,
            observation_mask=lambda indices, units: np.isin(
                indices, [key for key, unit in code.items() if unit in units]
            ),
            land_cells=lambda lat, lon: (
                np.full(len(lat), 20),
                np.full(len(lat), MOJAVE_DESERT),
            ),
            sample=lambda lat, lon: SimpleNamespace(
                native_candidates=(BLACKBRUSH_TAXON,)
            ),
        )
        cover = RegionalOpenCover(atlas, source, datetime(2026, 11, 14, tzinfo=UTC))
        xy = np.array([[-8.0, 2], [0.0, 0], [1.05, 0], [5.0, 0], [8.0, 0], [12.0, 0]])
        marks = np.zeros((len(xy), 3))
        with patch(
            "outshine.vegetation.mojave_sites.tile_sites",
            return_value=iter([(xy, marks)]),
        ):
            batches = list(
                mojave_blackbrush(
                    cover,
                    {"ground": surface(-16, 16), "unpaved path": surface(-1, 1)},
                    np.zeros(2),
                    2083,
                )
            )
        self.assertEqual([batch[0] for batch in batches], [BLACKBRUSH_PROFILE])
        positions = batches[0][3]
        np.testing.assert_array_equal(positions[:, :2], xy[[0, 3]])
        np.testing.assert_allclose(positions[:, 2], grade(xy[[0, 3]]) - 0.03)
