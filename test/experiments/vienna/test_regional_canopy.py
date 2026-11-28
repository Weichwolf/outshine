import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from regional_canopy import CROWN_HEIGHT_M, crown_batches, rooted_crowns


class RegionalCanopyTest(unittest.TestCase):
    def test_crowns_follow_solved_ground_and_respect_mapped_buildings(self):
        features = (
            ("building", shapely.box(15, 7, 17, 9), {}),
            (
                "landcover",
                shapely.box(27, 0, 44, 20),
                {"class": "wood", "leaf_type": "needleleaved"},
            ),
        )
        source = SimpleNamespace(
            lat=48, lon=16, datum=300, metric_features=lambda *_: features
        )
        sites = np.array([[8, 7], [16, 8], [25, 8], [32, 7], [12, 12]])
        marks = np.tile((0.4, 0.0, 0.5, 0.3), (len(sites), 1))
        marks[-1, 0] = 0.62
        xy = np.array(
            [[0, 0], [18, 0], [18, 20], [0, 20], [28, 0], [42, 0], [42, 20], [28, 20]]
        )
        z = 0.2 * xy[:, 0] - 0.12 * xy[:, 1] + 2
        forest = [(np.c_[xy, z], np.array([[0, 1, 2, 3], [4, 5, 6, 7]]))]

        with patch("regional_canopy.tile_crown_sites", return_value=(sites, marks)):
            products = list(
                rooted_crowns(source, {"tile": (14, 8900, 5700)}, {}, forest, 700, 12)
            )

        self.assertEqual(
            [species for species, _ in products], ["oak", "beech", "spruce"]
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
