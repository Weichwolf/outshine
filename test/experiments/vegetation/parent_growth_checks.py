"""Small independent parent-size, physical-needle and conservative-bound controls."""

import json
import unittest
from collections import Counter
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape
from outshine_ref.vegetation import AxisLevel, PlantPlan, Profile
from parent_growth_area_probe import AxisBudgetExceeded, length_orders
from parent_growth_graph import ParentGrowthGraph
from parent_growth_models import control_nodes


def graph(ratio=0.5):
    return ParentGrowthGraph(17, control_nodes(), parent_length_ratios=((2, ratio),))


class ParentGrowthTests(unittest.TestCase):
    def test_single_child_policy_and_independent_mode_budget_refusal(self):
        wood = PlantPlan(
            17,
            (
                AxisLevel(1, 2, 0.02, 0.01),
                AxisLevel(3, 1.5, 0.003, 0.001, tip_scale=1 / 3),
                AxisLevel(7, 0.75, 0.0002, 0.0001, tip_scale=1, first=0.1, last=0.9),
            ),
            (),
        )
        with patch("parent_growth_area_probe.MAX_AXES", 21):
            for mode in ("absolute-fixed", "relative-fixed"):
                with self.assertRaises(AxisBudgetExceeded):
                    length_orders(wood, mode)
            self.assertEqual(
                sum(map(len, length_orders(wood, "relative-distance"))), 19
            )
        single = replace(
            wood, levels=(*wood.levels[:2], replace(wood.levels[2], branches=1))
        )
        np.testing.assert_allclose(
            length_orders(single, "relative-distance")[2], (0.75, 0.5, 0.25)
        )
        self.assertFalse(wood._axes)
        self.assertFalse(single._axes)

    def test_length_count_probe_matches_the_independent_three_parent_control(self):
        wood = PlantPlan(
            17,
            (
                AxisLevel(1, 2, 0.02, 0.01),
                AxisLevel(3, 1.5, 0.003, 0.001, tip_scale=1 / 3),
                AxisLevel(7, 0.75, 0.0002, 0.0001, tip_scale=1, first=0.1, last=0.9),
            ),
            (),
        )
        absolute = length_orders(wood, "absolute-fixed")
        relative = length_orders(wood, "relative-fixed")
        distance = length_orders(wood, "relative-distance")
        np.testing.assert_allclose(absolute[2], np.full(21, 0.75))
        np.testing.assert_allclose(relative[2], np.repeat((0.75, 0.5, 0.25), 7))
        np.testing.assert_allclose(distance[2], np.repeat((0.75, 0.5, 0.25), (7, 5, 3)))
        self.assertFalse(wood._axes)

    def test_actual_parent_lengths_drive_child_lengths_counts_and_fixed_needles(self):
        g = graph()
        self.assertFalse(g._poses)
        children = g.instances(2, 1000)
        self.assertEqual(Counter(p.path[-2][1] for p in children), {0: 3, 1: 5, 2: 7})
        for pose in children:
            expected = (0.25, 0.5, 0.75)[pose.path[-2][1]]
            self.assertAlmostEqual(pose.length, expected, places=15)
            self.assertAlmostEqual(g.by_id[2].shape.length * pose.scale, expected)
        needles = g.instances(3, 1000)
        self.assertEqual(len(needles), 151)
        self.assertTrue(all(p.length == 0.02 and p.scale == 1 for p in needles))

    def test_descendant_length_bounds_are_not_the_fixed_prototype_lengths(self):
        g = graph(2.0)
        self.assertEqual(g.length_bounds[2], 3.0)
        pose = g.instance(((0, 0), (1, 2), (2, 0)))
        tip = pose.origin + pose.frame[:, 1] * pose.length
        self.assertAlmostEqual(pose.length, 3.0)
        self.assertGreater(np.linalg.norm(tip), 4.0)
        self.assertLessEqual(np.linalg.norm(tip), g.bounds[2])
        self.assertFalse(graph()._poses)

    def test_known_mature_spruce_counterexample_scales_through_two_orders(self):
        nodes = control_nodes()
        nodes = (
            nodes[0],
            replace(
                nodes[1],
                shape=TubeShape(
                    length=2.565,
                    bend=0.0,
                    radius=Profile(np.linspace(0.025, 0.0008, 4)),
                ),
                placement=replace(
                    nodes[1].placement,
                    count=2,
                    scale=Profile(np.linspace(0.09, 1.0, 4)),
                ),
            ),
            replace(
                nodes[2],
                shape=TubeShape(
                    length=0.855,
                    bend=0.0,
                    radius=Profile(np.linspace(0.0035, 0.0006, 4)),
                ),
                placement=Placement(scale_variance=0),
            ),
            Generator(
                3,
                "next wood order",
                "axis",
                (2,),
                TubeShape(
                    length=0.399, bend=0, radius=Profile(np.linspace(0.0007, 0.0002, 4))
                ),
                Placement(scale_variance=0),
            ),
        )
        g = ParentGrowthGraph(
            17, nodes, parent_length_ratios=((2, 1 / 3), (3, 0.399 / 0.855))
        )
        second = g.instances(2, 100)
        third = g.instances(3, 100)
        np.testing.assert_allclose(
            [p.length for p in second], [0.07695, 0.855], atol=2e-16
        )
        np.testing.assert_allclose(
            [p.length for p in third], [0.03591, 0.399], atol=2e-16
        )

    def test_budget_and_invalid_actual_child_do_not_commit_ancestry(self):
        g = graph()
        with patch.object(
            GeneratorGraph, "instance", side_effect=AssertionError("budget pose")
        ):
            self.assertIsNone(g.instances(3, 0))
        with self.assertRaises(ValueError):
            g.instance(((0, 0), (1, 0), (2, 6)))
        self.assertFalse(g._poses)
        self.assertFalse(g._sites)

    def test_parent_radius_fit_rejects_before_any_poses(self):
        nodes = list(control_nodes())
        nodes[2] = replace(
            nodes[2], shape=TubeShape(length=0.4, radius=Profile(np.full(4, 0.01)))
        )
        with self.assertRaises(ValueError):
            ParentGrowthGraph(17, tuple(nodes), parent_length_ratios=((2, 0.5),))

    def test_interior_radius_and_scale_profile_knots_are_checked(self):
        nodes = list(control_nodes())
        nodes[1] = replace(
            nodes[1],
            shape=TubeShape(
                length=1.0,
                bend=0,
                radius=Profile(np.array([0.006, 0.0001, 0.006, 0.006])),
            ),
            placement=replace(
                nodes[1].placement,
                count=1,
                scale=Profile(np.full(4, 0.4 / (0.9 - 1 / 3))),
            ),
        )
        with self.assertRaises(ValueError):
            ParentGrowthGraph(17, tuple(nodes), parent_length_ratios=((2, 0.5),))
        nodes = list(control_nodes())
        nodes[2] = replace(
            nodes[2],
            placement=replace(
                nodes[2].placement, scale=Profile(np.array([1, 30, 1, 1]))
            ),
        )
        with self.assertRaises(ValueError):
            ParentGrowthGraph(17, tuple(nodes), parent_length_ratios=((2, 0.5),))

    def test_two_different_parent_prototypes_use_actual_metres(self):
        nodes = list(control_nodes())
        nodes.append(
            Generator(
                4,
                "different nominal parent",
                "axis",
                (0,),
                TubeShape(
                    length=4.0, bend=0, radius=Profile(np.linspace(0.02, 0.008, 4))
                ),
                Placement(
                    scale=Profile(np.full(4, 0.25)), scale_variance=0, pitch_variance=0
                ),
            )
        )
        nodes[2] = replace(nodes[2], parents=(1, 4))
        g = ParentGrowthGraph(17, tuple(nodes), parent_length_ratios=((2, 0.5),))
        children = g.instances(2, 1000)
        from_four_metre_prototype = [p for p in children if p.path[-2][0] == 4]
        self.assertEqual(len(from_four_metre_prototype), 5)
        self.assertTrue(all(p.length == 0.5 for p in from_four_metre_prototype))
        self.assertEqual(g.length_bounds[2], 0.75)

    def test_warm_queries_and_json_replay_retain_exact_shapes_and_paths(self):
        g = graph()
        q = ParentGrowthGraph.from_recipe(json.loads(json.dumps(g.recipe())))
        self.assertEqual(g.identity, q.identity)
        a, b = g.instances(3, 1000), q.instances(3, 1000)
        self.assertEqual(
            tuple(id(p) for p in g.instances(3, 0)), tuple(id(p) for p in a)
        )
        for x, y in zip(a, b, strict=True):
            self.assertEqual(x.path, y.path)
            self.assertEqual((x.length, x.scale), (y.length, y.scale))
            np.testing.assert_array_equal(x.origin, y.origin)
            np.testing.assert_array_equal(x.frame, y.frame)
        with self.assertRaises(ValueError):
            GeneratorGraph.from_recipe(g.recipe())
        with self.assertRaises(ValueError):
            ParentGrowthGraph.from_recipe(GeneratorGraph(17, control_nodes()).recipe())

    def test_ratios_are_explicit_owned_and_cannot_target_non_axes(self):
        ratio = np.array(0.5)
        g = graph(ratio)
        ratio[...] = 0
        self.assertEqual(g.parent_length_ratios, ((2, 0.5),))
        for bad in (True, 0, -1, np.inf, np.nan, [0.5], "0.5", 1j):
            with self.assertRaises(ValueError):
                graph(bad)
        for pairs in (
            (),
            ((2, 0.5), (2, 0.6)),
            ((0, 0.5),),
            ((1, 0.5),),
            ((3, 0.5),),
            ((99, 0.5),),
        ):
            with self.assertRaises(ValueError):
                ParentGrowthGraph(17, control_nodes(), parent_length_ratios=pairs)
        with self.assertRaises(ValueError):
            graph(1e308)


if __name__ == "__main__":
    unittest.main()
