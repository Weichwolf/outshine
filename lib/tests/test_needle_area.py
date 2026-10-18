import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import (
    AxisLevel,
    Needle,
    NeedledPlant,
    NeedlePattern,
    PlantPlan,
)


def plant(seed=17):
    return NeedledPlant(
        PlantPlan(
            seed,
            (AxisLevel(1, 0.3, 0.01, 0.005), AxisLevel(3, 0.12, 0.001, 0.0004)),
            (),
        ),
        (
            NeedlePattern(spacing=0.011, group_size=1, scale_variation=0.2),
            NeedlePattern(spacing=0.007, group_size=2, scale_variation=0.07),
        ),
        (Needle(length=0.02), Needle(length=0.03), Needle(length=0.04)),
    )


class NeedleAreaTests(unittest.TestCase):
    def test_area_and_variants_match_independent_complete_id_ranges(self):
        for seed in (0, 17, 2**64 - 1):
            p = plant(seed)
            with (
                patch.object(
                    PlantPlan, "axis", side_effect=AssertionError("area axis")
                ),
                patch.object(
                    NeedlePattern, "detail", side_effect=AssertionError("area detail")
                ),
            ):
                result = p.area_summary((0.001, 0.002, 0.004))
            counts = np.zeros(3, int)
            expected = 0.0
            first = 0
            for n, pattern in zip(p.needle_counts, p.patterns):
                for identity in range(first, first + n):
                    variant = (identity + seed) % 3
                    counts[variant] += 1
                    v = pattern.scale_variation
                    nodes = (-v / np.sqrt(3), v / np.sqrt(3))
                    factor = sum((1 + s) ** 2 for s in nodes) / 2
                    expected += (0.001, 0.002, 0.004)[variant] * factor
                first += n
            self.assertEqual(result["variant_counts"], tuple(counts))
            self.assertAlmostEqual(result["expected_area_m2"], expected, places=13)
            self.assertFalse(p.wood._axes)
            self.assertFalse(p._groups)

    def test_zero_variance_matches_explicit_variant_areas_of_near_organs(self):
        from dataclasses import replace

        p = plant()
        p = replace(
            p,
            patterns=tuple(
                replace(pattern, scale_variation=0) for pattern in p.patterns
            ),
        )
        arrays = p.tables(100000, 100000)
        areas = np.array((0.001, 0.002, 0.004))
        expected = float(areas[arrays["foliage_variants"]].sum())
        self.assertAlmostEqual(
            p.area_summary(areas)["expected_area_m2"], expected, places=14
        )

    def test_inputs_are_validated_and_not_retained_as_mutable_arrays(self):
        p = plant()
        areas = np.array((0.001, 0.002, 0.004))
        result = p.area_summary(areas)
        areas[:] = 1
        self.assertEqual(result["unit_areas_m2"], (0.001, 0.002, 0.004))
        for bad in (
            (1, 2),
            (1, 2, 0),
            (1, 2, -1),
            (1, 2, np.inf),
            (1, 2, np.nan),
            (True, False, True),
            ["1", "2", "3"],
            np.ones((3, 1)),
            [1 + 1j, 2, 3],
        ):
            with self.assertRaises(ValueError):
                p.area_summary(bad)
        with self.assertRaises(ValueError):
            p.area_summary((1e308, 1e308, 1e308))
        if np.finfo(np.longdouble).minexp < -1074:
            with self.assertRaises(ValueError):
                p.area_summary(np.full(3, np.finfo(np.longdouble).tiny))
