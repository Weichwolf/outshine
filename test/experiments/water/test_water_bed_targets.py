import unittest
from fractions import Fraction

import numpy as np
from compare_water_geographic_query import geographic
from test_water_geographic_query import constant_height_field, plane_field
from water_bed_targets import bed_targets
from water_cell_field import sample_parameters
from water_geographic_query import query_geographic


class WaterBedTargets(unittest.TestCase):
    def test_exact_shared_observation_and_outward_subtraction(self):
        water = query_geographic(plane_field(500), [[0, 0], [0.001, -0.001]])
        for depth in (0, 2, 0.1, 1000):
            bed = bed_targets(water, depth_m=depth)
            self.assertIs(bed.water, water)
            self.assertEqual(bed.water.states, ("water", "water"))
            for i in range(2):
                exact = Fraction(float(water.height_m[i])) - Fraction(depth)
                self.assertLessEqual(Fraction(float(bed.height_bounds_m[i, 0])), exact)
                self.assertGreaterEqual(
                    Fraction(float(bed.height_bounds_m[i, 1])), exact
                )
                for side in (0, 1):
                    end = Fraction(float(water.height_bounds_m[i, side])) - Fraction(
                        depth
                    )
                    self.assertLessEqual(
                        Fraction(float(bed.height_bounds_m[i, 0])), end
                    )
                    self.assertGreaterEqual(
                        Fraction(float(bed.height_bounds_m[i, 1])), end
                    )
            self.assertEqual(bed.report["additional_geometry_queries"], 0)
            self.assertFalse(bed.height_m.flags.writeable)
            self.assertFalse(bed.height_bounds_m.flags.writeable)
            self.assertFalse(np.shares_memory(bed.height_m, water.height_m))
        np.testing.assert_array_equal(
            bed_targets(water, depth_m=0).height_m, water.height_m
        )

    def test_miss_and_budget_refusals_do_not_publish_bed_or_partial_sources(self):
        field = plane_field()
        for options in ({"predicate_limit": 0}, {"member_limit": 0}, {}):
            water = query_geographic(field, [[-20, 0]], **options)
            bed = bed_targets(water, depth_m=2)
            self.assertIs(bed.water, water)
            self.assertTrue(np.isnan(bed.height_m).all())
            self.assertTrue(np.isnan(bed.height_bounds_m).all())
        for options in ({"predicate_limit": 0}, {"member_limit": 0}):
            water = query_geographic(field, [[0, 0]], **options)
            bed = bed_targets(water, depth_m=2)
            self.assertNotEqual(water.states, ("water",))
            self.assertTrue(np.isnan(bed.height_m).all())
            self.assertEqual(len(bed.water.source_members), 0)

    def test_explicit_depth_is_not_inferred_or_clamped(self):
        water = query_geographic(plane_field(-500), [[0, 0]])
        self.assertEqual(bed_targets(water, depth_m=2).height_m[0], -502)
        for depth in (
            -1,
            float("nan"),
            float("inf"),
            True,
            "2",
            [2],
            10**400,
            np.finfo(float).max,
        ):
            with self.subTest(depth=str(depth)), self.assertRaises(ValueError):
                bed_targets(water, depth_m=depth)

    def test_shared_depth_keeps_neighbour_seam_and_corner_height_agreement(self):
        fields = [
            constant_height_field((14, x, y))
            for x in (8192, 8193)
            for y in (8192, 8193)
        ]
        world = sample_parameters(
            fields[0], [[1, 0.25], [1, 0.75], [1, 1], [0.25, 1], [0.75, 1]]
        ).world
        points, _height = geographic(world)
        beds = [bed_targets(query_geographic(f, points), depth_m=2) for f in fields]
        for i in range(len(points)):
            hits = [b for b in beds if b.water.states[i] == "water"]
            self.assertTrue(hits)
            self.assertLessEqual(
                max(b.height_bounds_m[i, 0] for b in hits),
                min(b.height_bounds_m[i, 1] for b in hits),
            )


if __name__ == "__main__":
    unittest.main()
