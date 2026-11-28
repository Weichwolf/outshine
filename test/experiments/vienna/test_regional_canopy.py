import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from regional_canopy import CROWN_HEIGHT_M, crown_batches, rooted_crowns
from woodland import habitat
from woodland_sites import mature_stand, woodland_sites


class RegionalCanopyTest(unittest.TestCase):
    def test_woodland_distances_share_mature_placements_and_respect_buildings(self):
        features = (
            ("landcover", shapely.box(0, 0, 44, 20), {"class": "wood"}),
            ("building", shapely.box(15, 7, 17, 9), {}),
            (
                "landcover",
                shapely.box(27, 0, 44, 20),
                {"class": "wood", "leaf_type": "needleleaved"},
            ),
        )
        source = SimpleNamespace(
            lat=48,
            lon=16,
            datum=300,
            metres=np.array([74_000, 111_000]),
            features=features,
            metric_features=lambda *_: features,
        )
        sites = np.array([[8, 7], [16, 8], [25, 8], [32, 7], [12, 12]])
        marks = np.tile((0.4, 0.17, 0.5), (len(sites), 1))
        marks[-1, 0] = 0.62
        xy = np.array(
            [[0, 0], [18, 0], [18, 20], [0, 20], [28, 0], [42, 0], [42, 20], [28, 20]]
        )
        z = 0.2 * xy[:, 0] - 0.12 * xy[:, 1] + 2
        forest = [(np.c_[xy, z], np.array([[0, 1, 2, 3], [4, 5, 6, 7]]))]

        domain = habitat(source)
        with patch("woodland_sites.tile_sites", side_effect=([(sites, marks)], [])):
            stand = mature_stand(source, domain, 100, 12, [])
            near, near_marks = woodland_sites(
                source, domain, np.zeros(2), 18, 12, mature=stand
            )
        receipt = {"tile": (14, 8900, 5700)}
        products = [
            *rooted_crowns(source, receipt, {}, forest, 700, 12, stand, (0, 0, 18, 20)),
            *rooted_crowns(
                source, receipt, {}, forest, 700, 12, stand, (18, 0, 44, 20)
            ),
        ]

        np.testing.assert_array_equal(near, sites[[0, 4]])
        self.assertEqual(
            [kind for kind, _ in products], [("oak", 1), ("beech", 0), ("spruce", 1)]
        )
        np.testing.assert_allclose(
            np.vstack([p for _, p in products[:2]])[:, 3:],
            near_marks[:, 1:] * (2 * np.pi, 1),
        )
        for expected_root, (_, placement) in zip(
            sites[[0, 4, 3]], products, strict=True
        ):
            x, y = expected_root
            np.testing.assert_allclose(
                placement[0, :3], (x, y, 0.2 * x - 0.12 * y + 1.9)
            )
        for _, meshes, _ in crown_batches(products):
            vertices, _ = meshes["shrub wood"]
            self.assertEqual(vertices[:, 2].min(), 0)
            self.assertGreater(vertices[:, 2].max(), 6.9)
            canopy, faces = next(
                mesh for name, mesh in meshes.items() if name.endswith("crown")
            )
            self.assertGreater(canopy[:, 2].max(), vertices[:, 2].max())
            self.assertLessEqual(canopy[:, 2].max(), CROWN_HEIGHT_M)
            self.assertLessEqual(len(faces), 48)
