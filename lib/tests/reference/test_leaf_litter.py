import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from outshine.materials.ground_detail import DETAIL_TINTS, SURFACE_DETAIL, ground_detail
from outshine.vegetation.understory import woodland_low_cover


class LeafLitterTest(unittest.TestCase):
    def test_atlas_has_raised_dry_leaves_and_exposed_moss(self):
        detail = SURFACE_DETAIL["forest floor"]
        tint, roughness, relief, moss = ground_detail(detail).transpose(2, 0, 1)
        leaves = (relief > 0.58) & (tint > 0.53) & (roughness < 0.84)
        moss_covered = (moss > 0.65) & (relief < 0.60)
        self.assertTrue(leaves.any())
        self.assertTrue(moss_covered.any())
        self.assertLess(float(moss[leaves].mean()), float(moss[moss_covered].mean()))
        self.assertEqual(len(DETAIL_TINTS[detail]), 3)
        self.assertTrue(np.isfinite(ground_detail(detail)).all())

    def test_low_cover_is_rooted_with_its_entire_footprint_off_the_walk(self):
        indices = np.array([[0, 1, 2], [0, 2, 3]])
        ground = np.array([[0, -4, 0], [4, -4, 0], [4, 4, 0], [0, 4, 0]])
        candidates = np.array([[0.02, 0], [0.27, 0], [1, 1]])
        marks = np.array([[0.9, 1.0, 0.05]] * len(candidates))
        habitat = shapely.box(0, -4, 4, 4)
        with (
            patch(
                "outshine.vegetation.understory.tile_sites",
                return_value=[(candidates, marks)],
            ),
            patch(
                "outshine.vegetation.understory.patch_density",
                return_value=np.array([1.0, 1.0]),
            ),
        ):
            outputs = list(
                woodland_low_cover(
                    (ground, indices),
                    SimpleNamespace(),
                    habitat,
                    np.array([0, 0, 1.7]),
                    7,
                )
            )
        [(name, vertices, _)] = outputs
        self.assertEqual(name, "woodland low leaves")
        self.assertGreaterEqual(vertices[:, 0].min(), 0)
        self.assertLess(vertices[:, 2].min(), 0)
        self.assertGreater(vertices[:, 2].max(), 0.1)
