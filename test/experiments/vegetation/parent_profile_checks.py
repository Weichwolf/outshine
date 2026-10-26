"""Independent inclination, frame, binding and explicit-version controls."""

import json
import unittest

import numpy as np
from outshine_ref.vegetation import Profile
from parent_growth_graph import ParentGrowthGraph
from parent_growth_models import control_nodes
from parent_profile_graph import ParentProfileGraph


def graph(profile=None, bias=0):
    return ParentProfileGraph(
        17,
        control_nodes(),
        parent_length_ratios=((2, 0.5),),
        inclination_profiles=(
            (2, Profile(np.linspace(0.2, 1.2, 4)) if profile is None else profile),
        ),
        upward_biases=((2, bias),),
    )


class ParentProfileTests(unittest.TestCase):
    def test_effective_angle_follows_actual_parent_parameter(self):
        g = graph()
        for pose in g.instances(2, 100):
            parent = g.instance(pose.path[:-1])
            site = g.site_table(2, parent)[pose.path[-1][1], 0]
            expected = 0.2 + site
            self.assertAlmostEqual(
                float(pose.frame[:, 1] @ (0, 0, 1)), np.cos(expected), places=14
            )
            np.testing.assert_allclose(pose.frame.T @ pose.frame, np.eye(3), atol=2e-14)
            self.assertAlmostEqual(np.linalg.det(pose.frame), 1, places=14)
            np.testing.assert_array_equal(pose.origin, pose.anchor)
            self.assertAlmostEqual(pose.length, parent.length * 0.5)

    def test_upward_bias_changes_direction_with_normalized_transport(self):
        profile = Profile(np.full(4, np.pi / 2))
        flat, biased = graph(profile), graph(profile, 0.2)
        for a, b in zip(flat.instances(2, 100), biased.instances(2, 100), strict=True):
            expected = 0.8 * a.frame[:, 1] + 0.2 * np.array((0, 0, 1))
            expected /= np.linalg.norm(expected)
            np.testing.assert_allclose(b.frame[:, 1], expected, atol=5e-16)
            np.testing.assert_array_equal(a.origin, b.origin)
            self.assertEqual(a.length, b.length)

    def test_json_replay_is_exact_and_old_descriptor_rejects_new_fields(self):
        original = graph(bias=0.06)
        recipe = json.loads(json.dumps(original.recipe()))
        replay = ParentProfileGraph.from_recipe(recipe)
        self.assertEqual(original.identity, replay.identity)
        for a, b in zip(
            original.instances(3, 1000), replay.instances(3, 1000), strict=True
        ):
            self.assertEqual(a.path, b.path)
            np.testing.assert_array_equal(a.origin, b.origin)
            np.testing.assert_array_equal(a.frame, b.frame)
        with self.assertRaises(ValueError):
            ParentGrowthGraph.from_recipe(recipe)
        with self.assertRaises(ValueError):
            ParentProfileGraph.from_recipe(
                ParentGrowthGraph(
                    17, control_nodes(), parent_length_ratios=((2, 0.5),)
                ).recipe()
            )

    def test_invalid_and_duplicate_bindings_refuse_before_poses(self):
        cases = (
            {"inclination_profiles": ((2, Profile(np.zeros(4))),)},
            {"inclination_profiles": ((2, Profile(np.full(4, np.pi))),)},
            {"inclination_profiles": ((3, Profile(np.ones(4))),)},
            {"inclination_profiles": ((2, Profile(np.ones(4))),) * 2},
            {"upward_biases": ((2, True),)},
            {"upward_biases": ((2, np.array([0.1])),)},
            {"upward_biases": ((2, 0.5),)},
            {"upward_biases": ((2, np.nan),)},
            {},
        )
        for fields in cases:
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                ParentProfileGraph(
                    17, control_nodes(), parent_length_ratios=((2, 0.5),), **fields
                )


if __name__ == "__main__":
    unittest.main()
