"""Analytic grouping, source-hole preservation and local closed-prism counterexamples."""

import unittest

import numpy as np
import shapely
from building_mass_groups import close_group, group_sources
from building_mass_inputs import MassSource
from building_mass_mesh import extrude


def source(identity, x, roof=(5, 5), bottom=(0, 0), *, raised=False):
    return MassSource(identity, shapely.box(x, 0, x + 1, 1), bottom, roof, raised)


class BuildingMassGroups(unittest.TestCase):
    def test_near_equal_height_sources_aggregate_and_order_is_stable(self):
        sources = [source(1, 0), source(2, 2), source(3, 4)]
        first, work = group_sources(sources)
        second, _ = group_sources(sources[::-1])
        self.assertEqual([[s.source for s in g] for g in first], [[1, 2, 3]])
        self.assertEqual(first, second)
        self.assertEqual(work["accepted_merges"], 2)

    def test_pairwise_height_chain_cannot_escape_whole_group_limit(self):
        sources = [source(i, i * 2, (5 + i * 2, 5 + i * 2)) for i in range(3)]
        groups, _ = group_sources(sources)
        self.assertEqual(sorted(map(len, groups)), [1, 2])

    def test_far_raised_high_and_different_bottom_sources_remain_separate(self):
        for other in (
            source(2, 5),
            source(2, 2, raised=True),
            source(2, 2, (13, 13)),
            source(2, 2, (9, 9), (4, 4)),
        ):
            with self.subTest(other=other):
                groups, _ = group_sources([source(1, 0), other])
                self.assertEqual(list(map(len, groups)), [1, 1])

    def test_closing_bridges_small_gap_but_retains_courtyard_and_occupied_island(self):
        courtyard = shapely.box(1, 1, 3, 3)
        outer = shapely.difference(shapely.box(0, 0, 4, 4), courtyard)
        island = shapely.box(1.75, 1.75, 2.25, 2.25)
        group = [
            MassSource(1, outer, (0, 0), (5, 5)),
            MassSource(2, island, (0, 0), (5, 5)),
        ]
        domain, _, report = close_group(group)
        empty = shapely.difference(courtyard, island)
        self.assertEqual(shapely.intersection(domain, empty).area, 0)
        self.assertTrue(domain.covers(island))
        self.assertEqual(report["protected_hole_area_m2"], empty.area)
        joined, _, change = close_group([source(3, 0), source(4, 2)])
        self.assertEqual(len(shapely.get_parts(joined)), 1)
        self.assertGreater(change["added_plan_area_m2"], 0)

    def test_closed_prism_volume_winding_and_courtyard_euler(self):
        for domain, volume, euler in (
            (shapely.box(0, 0, 2, 3), 24, 2),
            (
                shapely.difference(shapely.box(0, 0, 4, 4), shapely.box(1, 1, 3, 3)),
                48,
                0,
            ),
        ):
            with self.subTest(euler=euler):
                mesh, report = extrude(domain, (1, 5))
                self.assertEqual(report["packed_volume_m3"], volume)
                self.assertEqual(report["euler"], euler)
                self.assertEqual(mesh["positions"].dtype, np.float32)
                self.assertEqual(mesh["indices"].dtype, np.uint16)

    def test_float32_collapse_declines_instead_of_welding(self):
        with self.assertRaisesRegex(ValueError, "collapse in Float32"):
            extrude(shapely.box(100000, 0, 100000.001, 1), (0, 5))

    def test_duplicate_source_ids_and_nonpositive_height_decline(self):
        with self.assertRaisesRegex(ValueError, "unique source"):
            group_sources([source(1, 0), source(1, 2)])
        with self.assertRaisesRegex(ValueError, "positive prism height"):
            extrude(shapely.box(0, 0, 1, 1), (5, 5))

    def test_dense_proximity_input_declines_at_candidate_budget(self):
        with self.assertRaisesRegex(ValueError, "candidate budget"):
            group_sources([source(i, 0) for i in range(512)])


if __name__ == "__main__":
    unittest.main()
