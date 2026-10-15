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
from outshine_ref.vegetation.curve_frame import arc_frame


def plant(seed=29):
    return NeedledPlant(
        PlantPlan(
            seed,
            (
                AxisLevel(1, 0.21, 0.01, 0.004, bend=0.2),
                AxisLevel(3, 0.12, 0.001, 0.0005, tip_scale=0.5, bend=0.3),
            ),
            (),
        ),
        (None, NeedlePattern(spacing=0.007, group_size=2)),
        (Needle(length=0.03),),
    )


class NeedleWindowTests(unittest.TestCase):
    def test_budget_and_summary_never_expand_axes_or_needles(self):
        p = plant()
        with (
            patch.object(PlantPlan, "axis", side_effect=AssertionError("axes")),
            patch.object(
                NeedlePattern, "detail", side_effect=AssertionError("needles")
            ),
        ):
            summary = p.window_summary(nominal_span_m=0.02, max_groups=3)
            self.assertEqual(summary["axes"], 4)
            self.assertEqual(summary["groups_per_window"], (0, 2))
            self.assertIsNone(p.windows(3, 100000, nominal_span_m=0.02, max_groups=3))
            self.assertIsNone(
                p.windows(4, summary["windows"] - 1, nominal_span_m=0.02, max_groups=3)
            )
        self.assertFalse(p.wood._axes)
        self.assertFalse(p._groups)

    def test_complete_ranges_match_independent_near_queries(self):
        p = plant()
        with (
            patch.object(
                NeedledPlant, "foliage", side_effect=AssertionError("foliage")
            ),
            patch.object(
                NeedlePattern, "detail", side_effect=AssertionError("pattern")
            ),
            patch.object(Needle, "mesh", side_effect=AssertionError("mesh")),
        ):
            windows = p.windows(4, 100000, nominal_span_m=0.02, max_groups=3)
        self.assertFalse(p._groups)
        ids, counts = windows["first_needle_ids"], windows["needle_counts"]
        self.assertEqual(int(ids[0]), 0)
        np.testing.assert_array_equal(ids[1:], ids[:-1] + counts[:-1])
        self.assertEqual(int(ids[-1]) + int(counts[-1]), sum(p.needle_counts))
        for path in ((0,), (1,), (2,)):
            near = plant()
            detail = near.foliage(path, 100000)
            axis = near.wood.axis(path)
            selected = np.flatnonzero(windows["axis_ids"] == axis.id)
            self.assertEqual(int(ids[selected[0]]), int(detail["ids"][0]))
            self.assertEqual(int(counts[selected].sum()), len(detail["ids"]))
            first_group = (ids[selected] - detail["ids"][0]) // 2
            last_group = first_group + counts[selected] // 2 - 1
            expected = (
                detail["parent_parameters"][first_group]
                + detail["parent_parameters"][last_group]
            ) / 2
            np.testing.assert_allclose(
                windows["parent_parameters"][selected], expected, atol=1e-15
            )
            centre, tangent, lateral, normal = arc_frame(
                axis.length, axis.bend, 0, expected
            )
            np.testing.assert_allclose(
                windows["origins"][selected],
                centre @ axis.frame.T + axis.origin,
                atol=1e-15,
            )

    def test_owned_readonly_buffers_survive_later_queries_and_recipe_replay(self):
        p = plant()
        identity = p.identity
        recipe = p.recipe()
        a = p.windows(4, 100000)
        p.foliage((0,), 100000)
        p.windows(4, 100000, nominal_span_m=0.01)
        b = NeedledPlant.from_recipe(recipe).windows(4, 100000)
        self.assertEqual(p.identity, identity)
        self.assertEqual(p.recipe(), recipe)
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
            with self.assertRaises(ValueError):
                a[key].setflags(write=True)
        np.testing.assert_allclose(
            np.einsum("nji,njk->nik", a["frames"], a["frames"]),
            np.broadcast_to(np.eye(3), a["frames"].shape),
            atol=2e-14,
        )
        self.assertEqual(a["first_needle_ids"].dtype, np.uint64)
        self.assertEqual(a["needle_counts"].dtype, np.uint32)
        with self.assertRaises(TypeError):
            a["new"] = np.zeros(1)

    def test_invalid_window_parameters_reject(self):
        p = plant()
        for span in (0, -1, np.nan, np.inf, True, [0.1], 1j, "0.1"):
            with self.assertRaises(ValueError):
                p.window_summary(nominal_span_m=span)
        for groups in (0, 4097, True, 1.5):
            with self.assertRaises(ValueError):
                p.window_summary(max_groups=groups)
        for budget in (-1, 100001, True, 1.5):
            with self.assertRaises(ValueError):
                p.windows(budget, 100)
            with self.assertRaises(ValueError):
                p.windows(4, budget)
