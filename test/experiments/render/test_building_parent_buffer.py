"""Expansion overlap, protected courtyards and unchanged small-group fallback."""

import unittest

import numpy as np
import shapely
from building_mass_inputs import MassSource
from building_parent_buffer import expand
from building_tree_products import band_mesh


def grid():
    return [
        shapely.box(1.5 * x, 1.5 * y, 1.5 * x + 1, 1.5 * y + 1)
        for x in range(16)
        for y in range(16)
    ]


class BuildingParentBuffer(unittest.TestCase):
    def test_many_overlapping_expansions_form_one_expected_solid(self):
        domain = shapely.MultiPolygon(grid())
        result = expand(domain, 0.5, "components")
        self.assertTrue(result.is_valid)
        self.assertTrue(result.equals(shapely.box(-0.5, -0.5, 24, 24)))
        self.assertTrue(result.covers(domain))

    def test_large_parent_preserves_courtyard_and_closed_f32_mesh_checks(self):
        ring = shapely.difference(shapely.box(40, 0, 50, 10), shapely.box(42, 2, 48, 8))
        group = [
            MassSource(i, p, (0, 0), (5, 5)) for i, p in enumerate(grid() + [ring])
        ]
        mesh, heights, work = band_mesh(group, 0.5, buffer_path="components")
        self.assertEqual(heights, [0, 5])
        self.assertEqual(work["protected_hole_area_m2"], 36)
        self.assertEqual(work["euler"], 2)
        self.assertTrue(np.isfinite(mesh["positions"]).all())

    def test_small_group_fallback_retains_exact_original_buffer_words(self):
        domain = shapely.MultiPolygon(
            [
                shapely.Polygon([(0.1, 0.2), (1.3, 0), (1, 1), (0, 1)]),
                shapely.box(2.5, 0, 3.5, 1),
            ]
        )
        expected = shapely.buffer(domain, 0.31, join_style="mitre", mitre_limit=2)
        self.assertEqual(expand(domain, 0.31).wkb, expected.wkb)
        self.assertEqual(expand(domain, 0.31, "components").wkb, expected.wkb)
        for radius in (0, -1, float("nan"), float("inf")):
            with self.assertRaisesRegex(ValueError, "positive"):
                expand(domain, radius, "components")


if __name__ == "__main__":
    unittest.main()
