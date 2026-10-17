"""Translated positive slivers, disconnected integrals and exact empty-band budgets."""

import math
import unittest
from fractions import Fraction as F

import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from stair_occupancy_geometry import AffineOccupancy, OccupancyError
from stair_occupancy_handoff import OccupancyQuery, adjusted
from stair_occupancy_precision import exact_band_area


def body(domain, origin=(0, 0), lower=(100, 1, 0), upper=(101, 1, 0)):
    return AffineOccupancy("body", domain, "traffic", origin, lower, upper)


class ExactEmptyBands(unittest.TestCase):
    def test_unrepresentable_input_area_cannot_bypass_guard_through_full_overlap(self):
        source = shapely.box(0, 0, 1e-170, 1e-170)
        self.assertTrue(source.is_valid)
        self.assertEqual(source.area, 0)
        with self.assertRaisesRegex(OccupancyError, "domain area is not representable"):
            body(source, lower=(100, 0, 0), upper=(101, 0, 0))

    def test_positive_sliver_is_not_false_free_after_translation(self):
        heights = HeightInterval(0, math.nextafter(100.0, 101.0))
        wanted = F(heights.upper_m) - 100
        for x in (0, 1000, 500000):
            row = body(shapely.box(x, 0, x + 1, 1), (x, 0))
            with self.subTest(x=x):
                self.assertEqual(exact_band_area(row, heights), wanted)
                if x == 0:
                    self.assertEqual(row.conflict_geometry(heights).area, float(wanted))
                else:
                    with self.assertRaisesRegex(
                        OccupancyError, "positive affine conflict"
                    ):
                        row.conflict_geometry(heights)

    def test_resolution_failure_retains_source_and_reason_and_continues_elsewhere(self):
        row = body(shapely.box(1000, 0, 1001, 1), (1000, 0))
        h = HeightInterval(0, math.nextafter(100.0, 101.0))
        original = BuildingFootprint(
            "original", row.geometry, h, attributes={"source": "kept"}
        )
        free = BuildingFootprint("other", shapely.box(2000, 0, 2001, 1), h)
        changes, notes = adjusted(OccupancyQuery((row,)), (original, free))
        self.assertEqual(len(changes), 1)
        self.assertIs(changes[0].original, original)
        self.assertEqual(changes[0].adjusted, ())
        self.assertIn("positive affine conflict", notes[0]["deferred"])
        self.assertNotIn("deferred", notes[1])

    def test_noncollapsed_rectangle_can_still_lose_positive_domain_strip(self):
        p = shapely.Polygon([(1001, 1000), (1000, 1001), (1001, 1001)])
        row = body(p, (1000, 1000), lower=(100, 1, 1), upper=(101, 1, 1))
        h = HeightInterval(0, math.nextafter(101.0, 102.0))
        self.assertGreater(exact_band_area(row, h), 0)
        with self.assertRaisesRegex(OccupancyError, "positive affine conflict"):
            row.conflict_geometry(h)

    def test_disconnected_clipped_u_shape_preserves_both_components(self):
        domain = shapely.Polygon(
            [(0, 0), (4, 0), (4, 4), (3, 4), (3, 1), (1, 1), (1, 4), (0, 4)]
        )
        row = body(domain, lower=(100, 0, -1), upper=(101, 0, -1))
        heights = HeightInterval(0, 98)
        self.assertEqual(exact_band_area(row, heights), 4)
        self.assertEqual(row.conflict_geometry(heights).geom_type, "MultiPolygon")

    def test_holes_and_multipolygon_integrals_are_orientation_independent(self):
        polygon = shapely.Polygon(
            shapely.box(0, 0, 4, 4).exterior.coords,
            [shapely.box(1, 1, 3, 3).exterior.coords],
        )
        for domain in (polygon, shapely.reverse(polygon)):
            row = body(shapely.MultiPolygon([domain, shapely.box(5, 0, 6, 1)]))
            self.assertEqual(exact_band_area(row, HeightInterval(0, 102)), 6)
            self.assertEqual(exact_band_area(row, HeightInterval(0, 107)), 13)

    def test_geos_empty_corner_touch_is_certified_without_epsilon(self):
        domain = shapely.Polygon([(1, 0), (0, 1), (1, 1)])
        row = body(domain, lower=(100, 1, 1), upper=(101, 1, 1))
        h = HeightInterval(0, 101)
        self.assertEqual(exact_band_area(row, h), 0)
        self.assertTrue(row.conflict_geometry(h).is_empty)

    def test_empty_ring_never_converts_rational_area_to_float(self):
        domain = shapely.MultiPolygon(
            [shapely.box(0, 0, 1, 1), shapely.box(2, 0, 3, 1)]
        )
        row = body(domain, lower=(100, 3, 0), upper=(101, 3, 0))
        area = exact_band_area(row, HeightInterval(0, 101))
        self.assertIsInstance(area, F)
        self.assertEqual(area, F(1, 3))

    def test_exact_work_budget_exhaustion_is_not_reported_as_free(self):
        row = body(shapely.box(0, 0, 1, 1))
        with self.assertRaisesRegex(OccupancyError, "vertex budget"):
            exact_band_area(row, HeightInterval(0, 102), max_visits=5)
        with self.assertRaises(ValueError):
            exact_band_area(row, HeightInterval(0, 102), max_visits=0)


if __name__ == "__main__":
    unittest.main()
