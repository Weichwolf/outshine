"""Scalar branch counts/radii, pre-work refusal and actual bounded hierarchy replay."""

import copy
import math
import unittest
from itertools import pairwise
from unittest.mock import patch

import numpy as np
from fine_hierarchy_coverage import common_envelope, project
from fine_hierarchy_execute import execute
from outshine_ref.vegetation import NeedlePattern, Profile
from parent_profile_graph import ParentProfileGraph
from spruce_fine_hierarchy import branching, child_lengths, preflight
from spruce_growth_probe import measure
from spruce_study_source import INPUT, binding_metadata, read_input
from spruce_whorl_balance import balanced


class FineHierarchyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, cls.original = read_input(INPUT)
        cls.physical = cls.data["physical_source"]
        cls.metadata = binding_metadata(cls.data["reference_catalog_metadata"])
        cls.areas = np.array([1e-5, 1.5e-5, 2e-5])

    def test_children_match_independent_scalar_positions_lengths_and_counts(self):
        graph = ParentProfileGraph.from_recipe(branching(self.original, 0.045))
        parents = np.array([0.05, 0.21, 0.8, 2.0])
        actual = child_lengths(graph, 4, parents, 1000)
        expected = []
        for length in parents:
            n = math.floor((0.95 - 0.16) * length / 0.045) + 1
            for i in range(n):
                u = 0.95 - i * 0.045 / length
                expected.append(length * 0.7 * (1.05 - 0.8 * u))
        np.testing.assert_allclose(actual, expected, rtol=1e-14, atol=1e-17)

    def test_radius_binding_and_unchanged_control_are_preserved(self):
        saved = copy.deepcopy(self.original)
        recipe = branching(self.original, 0.045)
        self.assertEqual(saved, self.original)
        control = balanced(self.original, 48, 1.6)
        self.assertEqual(recipe["nodes"][:3], control["nodes"][:3])
        for key, ratio in ((3, 0.5), (4, 0.7)):
            old = np.array(saved["nodes"][key]["shape"]["radius"]["table"])
            new = np.array(recipe["nodes"][key]["shape"]["radius"]["table"])
            np.testing.assert_allclose(
                new * ratio, old * dict(saved["parent_length_ratios"])[key], rtol=2e-16
            )
        a = preflight(control, self.physical, self.metadata, self.areas)
        b = measure(control, self.physical, self.metadata, self.areas)
        for k in (
            "graph_identity",
            "axes",
            "orders",
            "windows",
            "needle_counts",
            "expected_area_m2",
        ):
            self.assertEqual(a[k], b[k])
        self.assertEqual(
            a["graph_identity"],
            "4731ad577134903f76f8b67b2ff6a21f1d97b8e78cd6be8ea6ab65ff43dea4f5",
        )

    def test_admitted_small_graph_uses_complete_actual_paths_not_upper_capacities(self):
        recipe = branching(self.original, 0.045)
        main = recipe["nodes"][2]
        main["shape"]["length"] = 0.6
        main["placement"]["first"], main["placement"]["last"] = 0.12, 0.2
        main["placement"]["scale"]["table"] = [1.0] * 4
        main["placement"]["pattern"]["limit_groups"] = 1
        main["placement"]["pattern"]["whorl_size"] = 2
        graph = ParentProfileGraph.from_recipe(recipe)
        primary = graph.instances(2, 100)
        values = [np.array([33.25]), np.array([p.length for p in primary])]
        for key in (3, 4):
            values.append(child_lengths(graph, key, values[-1], 1000))
        needle_counts, windows = [], 0
        for p, lengths in zip(self.physical["patterns"], values, strict=True):
            count = (
                [
                    math.floor((p["last"] - p["first"]) * float(length) / p["spacing"])
                    + 1
                    for length in lengths
                ]
                if p
                else [0] * len(lengths)
            )
            needle_counts.append(sum(count))
            windows += sum((n + 255) // 256 for n in count)
        expected = {
            "axes": sum(map(len, values)),
            "orders": list(map(len, values)),
            "windows": windows,
            "needle_counts": needle_counts,
            "graph_identity": graph.identity,
        }
        self.assertGreater(graph.capacities[4], expected["axes"])
        self.assertIsNone(graph.instances(4, expected["axes"]))
        with patch.object(
            ParentProfileGraph,
            "instances",
            side_effect=AssertionError("conservative bulk request"),
        ):
            _, axes, rows, reports = execute(
                recipe, self.physical, self.metadata, expected
            )
        self.assertEqual(len(axes["origins"]), expected["axes"])
        self.assertEqual(len(rows["origins"]), windows)
        self.assertEqual(reports[-1]["stored_graph_poses"], expected["axes"] + 1)
        self.assertLess(reports[-1]["mean_initial_world_z"], 0)
        pairs = axes["path_pairs"]
        offsets = axes["path_offsets"]
        paths = [
            tuple(map(tuple, pairs[int(a) : int(b)].tolist()))
            for a, b in pairwise(offsets)
        ]
        self.assertEqual(len(set(paths)), len(paths))
        fresh = ParentProfileGraph.from_recipe(recipe)
        for i in (0, len(paths) // 2, len(paths) - 1):
            pose = fresh.instance(paths[i])
            np.testing.assert_array_equal(pose.origin, axes["origins"][i])
            np.testing.assert_array_equal(pose.frame, axes["rotations"][i])
        self.assertLess(len(fresh._poses), 20)

    def test_rejections_precede_unbounded_pose_or_child_detail_work(self):
        graph = ParentProfileGraph.from_recipe(branching(self.original, 0.045))
        with (
            patch.object(
                Profile, "evaluate", side_effect=AssertionError("late child values")
            ),
            self.assertRaises(ValueError),
        ):
            child_lengths(graph, 4, np.array([0.8]), 0)
        with self.assertRaises(ValueError):
            child_lengths(graph, 4, np.array([10.0]), 10000)
        with (
            patch.object(
                ParentProfileGraph,
                "from_recipe",
                side_effect=AssertionError("early pose graph"),
            ),
            self.assertRaises(ValueError),
        ):
            execute({}, {}, {}, {"axes": 60001, "orders": [60001], "windows": 1})
        for spacing in (0, float("nan"), 0.2):
            with self.assertRaises(ValueError):
                branching(self.original, spacing)

    def test_shared_envelope_uses_both_products(self):
        a = {"origins": np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]])}
        b = {"origins": np.array([[1.0, 1.0, 0.0]])}
        mask = common_envelope(
            [a, b], np.eye(3), np.array([0.0, 0.0]), np.array([1.0, 1.0]), 4
        )
        self.assertTrue(mask.all())
        triangle = common_envelope(
            [a], np.eye(3), np.array([0.0, 0.0]), np.array([1.0, 1.0]), 4
        )
        self.assertTrue(triangle[0, 0])
        self.assertFalse(triangle[-1, -1])

    def test_projection_keeps_leaf_weight_distinct_from_total_alpha(self):
        values = np.zeros((3, 9))
        values[:, 3] = [0.5, 1.0, 0.25]
        values[:, 7] = [0.5, 0.0, 0.2]
        fragments = np.array([0, 0, 1]), np.array([2.0, 1.0, 1.0]), values
        with patch("fine_hierarchy_coverage.window_fragments", return_value=fragments):
            image, cost = project(
                {},
                [],
                np.array([1.0, 0.0, 0.0]),
                np.eye(3),
                np.zeros(2),
                np.ones(2),
                size=2,
                samples=1,
            )
        self.assertEqual(cost["fragments"], 3)
        np.testing.assert_allclose(image[..., 3], [[1.0, 0.25], [0.0, 0.0]])
        np.testing.assert_allclose(image[..., 7], [[0.5, 0.2], [0.0, 0.0]])

    def test_angular_spread_preserves_physical_mass_and_default_graph(self):
        hanging = branching(self.original, 0.045)
        radial = branching(self.original, 0.045, posture="radial")
        self.assertEqual(
            ParentProfileGraph.from_recipe(hanging).identity,
            "1af43d204ed2b6c4751d45146642b99508507018b438139e14e406ec5e8db04e",
        )
        self.assertEqual(hanging["nodes"][:4], radial["nodes"][:4])
        self.assertEqual(hanging["nodes"][4]["shape"], radial["nodes"][4]["shape"])
        a = preflight(hanging, self.physical, self.metadata, self.areas)
        b = preflight(radial, self.physical, self.metadata, self.areas)
        for key in (
            "axes",
            "orders",
            "needle_counts",
            "windows",
            "expected_area_m2",
            "maximum_length_m_by_order",
        ):
            self.assertEqual(a[key], b[key])

    def test_count_boundary_and_prototype_roundtrip_match_actual_float64_contracts(
        self,
    ):
        graph = ParentProfileGraph.from_recipe(branching(self.original, 0.045))
        # The existing placement contract retains the closed endpoint within
        # 32epsilon; raw floor would incorrectly discard the twelfth branch.
        values = child_lengths(graph, 4, np.array([0.6265822784810127]), 100)
        self.assertEqual(len(values), 12)
        parent = 0.0640987595703009
        values = child_lengths(graph, 4, np.array([parent]), 100)
        local = float(graph.by_id[4].placement.scale.evaluate(0.95))
        raw = parent * 0.7 * local
        prototype = graph.by_id[4].shape.length
        self.assertEqual(values[0], prototype * (raw / prototype))
        pattern = NeedlePattern.from_recipe(self.physical["patterns"][3])
        self.assertEqual(pattern.count(raw), 10)
        self.assertEqual(pattern.count(float(values[0])), 9)


if __name__ == "__main__":
    unittest.main()
