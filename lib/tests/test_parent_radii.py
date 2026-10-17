import json
import unittest
from dataclasses import replace
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


def wood(seed=17):
    return PlantPlan(
        seed,
        (
            AxisLevel(1, 3, 0.1, 0.005, bend=0.4),
            AxisLevel(
                5,
                0.6,
                0.05,
                0.005,
                first=0.15,
                last=0.9,
                attachment_jitter=0.3,
                radius_parent_ratio=0.5,
            ),
            AxisLevel(
                4,
                0.18,
                0.015,
                0.003,
                first=0.1,
                last=0.9,
                attachment_jitter=0.25,
                radius_parent_ratio=0.6,
            ),
        ),
        (),
    )


class ParentRadiusTests(unittest.TestCase):
    def test_actual_attachments_and_conservative_limits_agree_independently(self):
        for seed in range(10):
            p = wood(seed)
            self.assertFalse(p._axes)
            limits = p.radius_limits
            self.assertFalse(p._axes)
            expected = {}
            for path in p.paths():
                axis, level = p.axis(path), p.levels[len(path)]
                if path:
                    parent_base, parent_tip = expected[path[:-1]]
                    parent = (
                        parent_base * (1 - axis.parent_parameter)
                        + parent_tip * axis.parent_parameter
                    )
                    base = min(level.base_radius, parent * level.radius_parent_ratio)
                    expected[path] = (base, base / level.base_radius * level.tip_radius)
                    self.assertLessEqual(p.radii(path)[0], parent)
                else:
                    expected[path] = (0.1, 0.005)
                np.testing.assert_allclose(
                    p.radii(path), expected[path], atol=2e-17, rtol=2e-15
                )
                self.assertLessEqual(
                    p.radii(path)[0], limits[len(path)][path[-1] if path else 0] + 2e-17
                )

    def test_only_requested_ancestry_is_expanded_and_budgets_still_reject_first(self):
        p = wood()
        with patch.object(PlantPlan, "axis", side_effect=AssertionError("budget axis")):
            self.assertEqual(p.summary()["axes"], 26)
            self.assertIsNone(p.tables(25, 0))
        self.assertFalse(p._axes)
        p.radii((4, 3))
        self.assertEqual(set(p._axes), {(), (4,), (4, 3)})
        self.assertFalse(p._leaves)
        with self.assertRaises(ValueError):
            p.radii((5,))

    def test_v4_tables_and_json_replay_preserve_actual_radii_in_owned_buffers(self):
        p = wood()
        recipe = p.recipe()
        self.assertEqual(recipe["version"], "branching-plant-4")
        q = PlantPlan.from_recipe(json.loads(json.dumps(recipe)))
        self.assertEqual(p.identity, q.identity)
        a, b = p.tables(26, 0), q.tables(26, 0)
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
            with self.assertRaises(ValueError):
                a[key].setflags(write=True)
        for path in p.paths():
            np.testing.assert_array_equal(
                a["axis_radii"][p.axis(path).id], p.radii(path)
            )

    def test_physical_needles_use_actual_radii_without_axis_work_in_constructor(self):
        p = wood()
        pattern = NeedlePattern(spacing=0.007, short_length=0, group_size=1)
        with patch.object(
            PlantPlan, "axis", side_effect=AssertionError("constructor axis")
        ):
            plant = NeedledPlant(p, (None, pattern, pattern), (Needle(length=0.02),))
        path = (4, 3)
        detail = plant.foliage(path, 100000)
        state = p.axis(path)
        t = detail["parent_parameters"]
        centre = (
            arc_frame(state.length, state.bend, 0, t)[0] @ state.frame.T + state.origin
        )
        measured = np.linalg.norm(detail["origins"] - centre, axis=1)
        base, tip = p.radii(path)
        np.testing.assert_allclose(measured, base * (1 - t) + tip * t, atol=2e-15)
        self.assertLess(base, p.levels[2].base_radius)

    def test_old_explicit_geometry_rule_and_new_ratio_validation(self):
        p = wood()
        with self.assertRaises(ValueError):
            PlantPlan(
                p.seed,
                tuple(replace(l, radius_parent_ratio=None) for l in p.levels),
                (),
            )
        with self.assertRaises(ValueError):
            PlantPlan(
                p.seed,
                (replace(p.levels[0], radius_parent_ratio=0.5), *p.levels[1:]),
                (),
            )
        with self.assertRaises(ValueError):
            PlantPlan(
                p.seed,
                (*p.levels[:2], replace(p.levels[2], radius_parent_ratio=None)),
                (),
            )
        for value in (False, 0, -1, 1.1, np.nan, np.inf, [0.5], 1j, "0.5"):
            with self.assertRaises(ValueError):
                replace(p.levels[1], radius_parent_ratio=value)

    def test_versions_cannot_smuggle_or_omit_radius_semantics(self):
        recipe = wood().recipe()
        for version in ("branching-plant-2", "branching-plant-3", "branching-plant-5"):
            with self.assertRaises(ValueError):
                PlantPlan.from_recipe(recipe | {"version": version})
        bad = json.loads(json.dumps(recipe))
        del bad["levels"][0]["radius_parent_ratio"]
        with self.assertRaises(ValueError):
            PlantPlan.from_recipe(bad)
        bad = json.loads(json.dumps(recipe))
        for level in bad["levels"]:
            level["radius_parent_ratio"] = None
        with self.assertRaises(ValueError):
            PlantPlan.from_recipe(bad)

    def test_ratio_is_an_owned_serializable_scalar(self):
        value = np.array(0.5)
        level = replace(wood().levels[1], radius_parent_ratio=value)
        value[...] = 0
        self.assertIs(type(level.radius_parent_ratio), float)
        self.assertEqual(level.radius_parent_ratio, 0.5)
        self.assertEqual(
            replace(level, radius_parent_ratio=np.uint64(1)).radius_parent_ratio, 1.0
        )

    def test_wide_v4_whorls_preserve_old_capacity_rejections(self):
        p = wood()
        with self.assertRaises(ValueError):
            replace(p.levels[0], branches=65)
        with self.assertRaises(ValueError):
            replace(p.axis(()), path=(64,))
        with self.assertRaises(ValueError):
            replace(p.levels[1], branches=257)
        q = PlantPlan(
            p.seed,
            (
                p.levels[0],
                replace(p.levels[1], branches=240),
                replace(p.levels[2], branches=1),
            ),
            (),
        )
        self.assertEqual(q.summary()["axes"], 481)
        self.assertFalse(q._axes)
        state = q.axis((239, 0))
        self.assertEqual(state.path_capacity, 256)
        self.assertLessEqual(q.radii((239, 0))[0], q.radii((239,))[0])
        r = PlantPlan.from_recipe(json.loads(json.dumps(q.recipe())))
        for key, values in q.tables(481, 0).items():
            np.testing.assert_array_equal(values, r.tables(481, 0)[key])
