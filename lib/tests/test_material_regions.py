"""Rank ambiguity, source retention and independent material coverage controls."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame
from outshine_ref.materials import MaterialRegion, coalesce_regions


class MaterialRegionTests(unittest.TestCase):
    def region(self, key, geometry, material=3, rank=10, frame="material-test"):
        return MaterialRegion(LocalFrame(frame), material, rank, geometry, (key,))

    def test_union_preserves_overlaps_holes_disconnected_parts_and_source_ids(self):
        hole = shapely.Polygon(
            [(0, 0), (8, 0), (8, 8), (0, 8)], [[(2, 2), (2, 6), (6, 6), (6, 2)]]
        )
        before = (
            self.region("outer", hole),
            self.region("fill", shapely.box(4, 0, 10, 8)),
            self.region("island", shapely.box(20, 20, 22, 22)),
        )
        result = coalesce_regions(before)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].source_ids, ("fill", "island", "outer"))
        self.assertEqual(result[0].frame, before[0].frame)
        points = shapely.points(np.random.default_rng(173).uniform(-1, 23, (2048, 2)))
        reference = np.zeros(len(points), dtype=bool)
        for row in before:
            reference |= shapely.covers(row.geometry, points)
        np.testing.assert_array_equal(
            shapely.covers(result[0].geometry, points), reference
        )
        self.assertFalse(shapely.covers(result[0].geometry, shapely.Point(3, 3)))
        self.assertTrue(shapely.covers(result[0].geometry, shapely.Point(5, 3)))
        self.assertTrue(shapely.covers(result[0].geometry, shapely.Point(21, 21)))
        self.assertEqual(before[0].geometry, hole)

    def test_ambiguous_ranks_preserve_individual_records_and_order(self):
        rows = (
            self.region("a", shapely.box(0, 0, 2, 2)),
            self.region("b", shapely.box(1, 1, 3, 3), material=4),
            self.region("c", shapely.box(2, 2, 4, 4)),
        )
        result = coalesce_regions(rows)
        self.assertEqual(len(result), len(rows))
        for actual, original in zip(result, rows, strict=True):
            self.assertIs(actual, original)

    def test_different_ranks_remain_separate_and_empty_input_is_valid(self):
        a = self.region("a", shapely.box(0, 0, 2, 2))
        b = self.region("b", shapely.box(0, 0, 2, 2), rank=11)
        self.assertEqual(coalesce_regions((a, b)), (a, b))
        self.assertEqual(coalesce_regions(()), ())

    def test_frame_source_and_geometry_contracts_are_checked(self):
        a = self.region("a", shapely.box(0, 0, 2, 2))
        b = self.region("b", shapely.box(0, 0, 2, 2), frame="other")
        with self.assertRaises(ValueError):
            coalesce_regions((a, b))
        with self.assertRaises(ValueError):
            coalesce_regions([a])
        with self.assertRaises(ValueError):
            self.region("", shapely.box(0, 0, 1, 1))
        with self.assertRaises(ValueError):
            self.region("a", shapely.LineString([(0, 0), (1, 1)]))
        with self.assertRaises(ValueError):
            self.region("a", shapely.box(0, 0, 1, 1), rank=-1)
        with self.assertRaises(TypeError):
            MaterialRegion(None, 0, 0, shapely.box(0, 0, 1, 1), ("a",))
