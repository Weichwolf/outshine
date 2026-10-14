"""Whorled wood and independent physical needle attachment/identity counterexamples."""

import json
import unittest

import numpy as np
from outshine_ref.vegetation import (
    AxisLevel,
    AxisState,
    Blade,
    NeedlePattern,
    PlantPlan,
)
from outshine_ref.vegetation.curve_frame import arc_frame


def twig(length=0.18, bend=0.4):
    return AxisState(
        (0, 0), 7, length, bend, np.array((0.1, -0.2, 0.3)), np.eye(3), 0.4, 0.3
    )


class ConiferPatternTests(unittest.TestCase):
    def test_whorl_heights_crown_lengths_and_inclinations_independently(self):
        level = AxisLevel(
            25,
            1,
            0.004,
            0.001,
            whorl_size=5,
            angle=1.6,
            angle_tip=0.7,
            tip_scale=0.2,
            first=0.2,
            last=0.9,
        )
        plan = PlantPlan(
            37,
            (
                AxisLevel(1, 3, 0.03, 0.006, tip_scale=1, bend=0, bend_variation=0),
                level,
            ),
            (),
        )
        trunk = plan.axis(())
        for group in range(5):
            expected_t = 0.2 + group * 0.7 / 4
            children = [plan.axis((group * 5 + i,)) for i in range(5)]
            for axis in children:
                np.testing.assert_allclose(
                    axis.origin,
                    trunk.origin + trunk.frame @ (0, 3 * expected_t, 0),
                    atol=2e-16,
                )
                self.assertAlmostEqual(axis.length, 1 - 0.8 * group / 4)
                inclination = np.arccos(axis.frame[:, 1] @ trunk.frame[:, 1])
                target = 1.6 - 0.9 * group / 4
                self.assertLessEqual(
                    abs(inclination - target), level.angle_variation + 1e-14
                )
        self.assertEqual(plan.summary()["leaves"], 0)
        self.assertEqual(plan.tables(26, 0)["leaf_frames"].shape, (0, 3, 3))
        self.assertEqual(plan.recipe()["version"], "branching-plant-3")
        restored = PlantPlan.from_recipe(json.loads(json.dumps(plan.recipe())))
        for name, value in plan.tables(26, 0).items():
            np.testing.assert_array_equal(value, restored.tables(26, 0)[name])

    def test_old_recipe_has_no_new_semantics_and_v2_cannot_smuggle_them(self):
        plan = PlantPlan(3, (AxisLevel(1, 1, 0.02, 0.005),), (Blade(),))
        recipe = plan.recipe()
        self.assertEqual(recipe["version"], "branching-plant-2")
        self.assertNotIn("whorl_size", recipe["levels"][0])
        self.assertNotIn("angle_tip", recipe["levels"][0])
        bad = json.loads(json.dumps(recipe))
        bad["levels"][0]["whorl_size"] = 4
        with self.assertRaises(ValueError):
            PlantPlan.from_recipe(bad)
        with self.assertRaises(ValueError):
            PlantPlan(3, (AxisLevel(1, 1, 0.02, 0.005, leaf_internode=0.1),), ())

    def test_physical_nodes_fit_the_twig_surface_and_needles_reach_short_tips(self):
        axis = twig()
        for members, spread in ((1, 0), (2, 0), (5, 0), (30, 0.65)):
            pattern = NeedlePattern(group_size=members, spread=spread)
            result = pattern.detail(axis, (0.001, 0.0004), 38, 100000)
            t = result["parent_parameters"]
            centre = (
                arc_frame(axis.length, axis.bend, 0, t)[0] @ axis.frame.T + axis.origin
            )
            radius = 0.001 + (0.0004 - 0.001) * t
            np.testing.assert_allclose(
                np.linalg.norm(result["short_origins"] - centre, axis=1),
                radius,
                atol=4e-17,
            )
            tips = (
                result["short_origins"]
                + pattern.short_length * result["short_frames"][:, :, 1]
            )
            np.testing.assert_array_equal(
                result["origins"], np.repeat(tips, members, axis=0)
            )
            np.testing.assert_allclose(
                np.einsum("nji,njk->nik", result["frames"], result["frames"]),
                np.eye(3)[None] + np.zeros((len(result["frames"]), 3, 3)),
                atol=5e-16,
            )
            np.testing.assert_allclose(np.linalg.det(result["frames"]), 1, atol=5e-16)
            self.assertTrue(np.all(t >= pattern.first) and np.all(t <= pattern.last))

    def test_group_rotation_distinguishes_fascicles_and_radial_fans(self):
        axis = twig(bend=0)
        for count in (2, 5):
            pattern = NeedlePattern(group_size=count)
            result = pattern.detail(axis, (0.001, 0.0004), 1, 100000)
            direction = result["frames"][:count, :, 1]
            np.testing.assert_array_equal(direction, np.tile(direction[0], (count, 1)))
            lateral = result["frames"][:count, :, 0]
            np.testing.assert_allclose(lateral.sum(axis=0), 0, atol=5e-16)
        pattern = NeedlePattern(group_size=30, spread=0.65)
        result = pattern.detail(axis, (0.001, 0.0004), 1, 100000)
        dot = result["frames"][:30, :, 1] @ result["short_frames"][0, :, 1]
        np.testing.assert_allclose(dot, np.cos(0.65), atol=5e-16)

    def test_needle_scale_does_not_inherit_twig_length_and_request_order_is_exact(self):
        pattern = NeedlePattern()
        short, long = (
            pattern.detail(twig(length), (0.001, 0.0004), 61, 100000)
            for length in (0.12, 0.8)
        )
        for output in (short, long):
            self.assertTrue(
                np.all(output["scales"] >= 0.9) and np.all(output["scales"] <= 1.1)
            )
            for value in output.values():
                with self.assertRaises(ValueError):
                    value.setflags(write=True)
        restored = NeedlePattern.from_recipe(json.loads(json.dumps(pattern.recipe())))
        for key, value in short.items():
            np.testing.assert_array_equal(
                value, restored.detail(twig(0.12), (0.001, 0.0004), 61, 100000)[key]
            )

    def test_capacity_precedes_detail_and_invalid_patterns_reject(self):
        pattern = NeedlePattern(group_size=30)
        self.assertIsNone(pattern.detail(twig(), (0.001, 0.0004), 1, 1))
        with self.assertRaises(ValueError):
            NeedlePattern(spacing=1e-6).count(1)
        for kwargs in (
            {"group_size": True},
            {"group_size": 41},
            {"spacing": 0},
            {"spread": np.pi},
            {"last": 1.1},
        ):
            with self.assertRaises(ValueError):
                NeedlePattern(**kwargs)
        with self.assertRaises(ValueError):
            AxisLevel(25, 1, 0.004, 0.001, whorl_size=0)
        with self.assertRaises(ValueError):
            AxisLevel(25, 1, 0.004, 0.001, angle_tip=0)
