import unittest
from datetime import UTC, datetime
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from outshine.vegetation.open_cover import RegionalOpenCover
from outshine.vegetation.regional_woody import savanna_woody
from outshine.vegetation.umbrella_thorn import BARK, LEAVES, TAXON_ID, umbrella_thorn
from shapely.geometry import box


class SavannaWoodyTest(unittest.TestCase):
    def test_atlas_candidates_root_on_the_solved_grade_and_clear_the_route(self):
        source = SimpleNamespace(
            lon=35.0,
            lat=-2.0,
            metres=np.array([111000, 111000]),
            features=[("building", box(19, -2, 25, 2), {})],
        )
        atlas = SimpleNamespace(
            land_cells=lambda lat, lon: (np.full(len(lat), 30), np.full(len(lat), 57)),
            sample=lambda lat, lon: SimpleNamespace(
                native_candidates=(TAXON_ID,) if lon < 35 + 34 / 111000 else ()
            ),
        )
        cover = RegionalOpenCover(atlas, source, datetime(2026, 8, 12, tzinfo=UTC))
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        ground = np.array([[-50, -20, -5], [50, -20, 5], [50, 20, 5], [-50, 20, -5]])
        road = np.array([[-1, -15, 0], [1, -15, 0], [1, 15, 0], [-1, 15, 0]])
        points = np.array([[-12, 0], [-9, 0], [4, 0], [11, 0], [22, 0], [36, 0]])
        marks = np.column_stack(
            ([0.13, 0.21, 0.33, 0.28, 0.57, 0.81], np.full(6, 0.6), np.zeros(6))
        )
        surfaces = {"ground": (ground, faces), "road": (road, faces)}
        with patch(
            "outshine.vegetation.regional_woody.tile_sites",
            return_value=[(points, marks)],
        ):
            origins = [
                np.concatenate(
                    [
                        entries
                        for _, _, _, entries in savanna_woody(
                            cover, surfaces, np.array(eye), 2083
                        )
                    ]
                )
                for eye in [(0, 0), (-34, 3)]
            ]
        for offset in origins:
            xyz = offset[offset[:, 0].argsort(), :3]
            np.testing.assert_allclose(xyz, [[-12, 0, -1.2], [11, 0, 1.1]])

    def test_detail_changes_leaf_sampling_without_changing_the_stem(self):
        low = umbrella_thorn(0, True, False)
        high = umbrella_thorn(0, True, True)
        np.testing.assert_array_equal(low[BARK][0], high[BARK][0])
        np.testing.assert_array_equal(low[BARK][1], high[BARK][1])
        for leaf in LEAVES:
            self.assertLess(len(low[leaf][1]) * 4, len(high[leaf][1]))
        vertices = np.concatenate([high[name][0] for name in LEAVES])
        self.assertGreater(max(np.ptp(vertices[:, :2], axis=0)), vertices[:, 2].max())
