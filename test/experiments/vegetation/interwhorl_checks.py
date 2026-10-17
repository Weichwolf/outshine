"""Independent physical descendants, primary ownership, demand and old controls."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
from interwhorl_distribution import area_weights, grouped_area
from interwhorl_execute import execute
from outshine_ref.vegetation import NeedlePattern
from parent_profile_graph import ParentProfileGraph
from spruce_growth_probe import measure
from spruce_growth_product import support
from spruce_interwhorl import intercalated, preflight
from spruce_study_source import INPUT, binding_metadata, read_input
from spruce_whorl_balance import balanced


def small_recipe(original):
    recipe = intercalated(original, 1.45)
    recipe["nodes"][1]["shape"]["length"] = 1.5
    for key in (2, 5):
        recipe["nodes"][key]["shape"]["length"] = 0.3
    recipe["nodes"][2]["placement"]["pattern"]["limit_groups"] = 3
    recipe["nodes"][5]["placement"]["count"] = 3
    recipe["nodes"][3]["placement"]["pattern"]["spacing"]["table"] = [0.05] * 4
    return recipe


class IntercalatedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, cls.original = read_input(INPUT)
        cls.physical = cls.data["physical_source"]
        cls.metadata = binding_metadata(cls.data["reference_catalog_metadata"])
        cls.areas = np.array([1e-5, 1.5e-5, 2e-5])

    def test_original_control_and_shape_are_preserved(self):
        original = copy.deepcopy(self.original)
        recipe = intercalated(self.original, 1.45)
        self.assertEqual(self.original, original)
        for key in range(1, 5):
            self.assertEqual(
                recipe["nodes"][key]["shape"], original["nodes"][key]["shape"]
            )
        self.assertEqual(recipe["nodes"][5]["shape"], original["nodes"][2]["shape"])
        control = balanced(original, 48, 1.6)
        a = preflight(control, self.physical, self.metadata, self.areas)
        b = measure(control, self.physical, self.metadata, self.areas)
        for key in (
            "graph_identity",
            "axes",
            "orders",
            "windows",
            "needle_counts",
            "expected_area_m2",
        ):
            self.assertEqual(a[key], b[key])
        self.assertEqual(
            a["graph_identity"],
            "4731ad577134903f76f8b67b2ff6a21f1d97b8e78cd6be8ea6ab65ff43dea4f5",
        )

    def test_scalar_child_lengths_and_distinct_primary_owners(self):
        recipe = small_recipe(self.original)
        graph = ParentProfileGraph.from_recipe(recipe)
        actual = graph.instances(3, 10000)
        self.assertIsNotNone(actual)
        expected_paths = set()
        for parent_key in (2, 5):
            for parent in graph.instances(parent_key, 1000):
                pitch = recipe["nodes"][3]["placement"]
                count = (
                    int(
                        np.floor(
                            (pitch["last"] - pitch["first"]) * parent.length / 0.05
                        )
                    )
                    + 1
                )
                for slot in range(count):
                    u = pitch["last"] - slot * 0.05 / parent.length
                    local_scale = np.interp(
                        u, np.linspace(0, 1, 4), pitch["scale"]["table"]
                    )
                    path = parent.path + ((3, slot),)
                    pose = graph.instance(path)
                    self.assertAlmostEqual(
                        pose.length, parent.length * (1 / 3) * local_scale
                    )
                    expected_paths.add(path)
        self.assertEqual({p.path for p in actual}, expected_paths)
        self.assertEqual({p.path[-2][0] for p in actual}, {2, 5})
        self.assertLess(len(graph._poses), 1000)

    def test_complete_cached_ledger_and_requested_small_population_detail(self):
        recipe = small_recipe(self.original)
        expected = preflight(recipe, self.physical, self.metadata, self.areas)
        _, axes, windows, _ = execute(recipe, self.physical, self.metadata, expected)
        np.testing.assert_array_equal(
            windows["first_needle_ids"][1:],
            windows["first_needle_ids"][:-1] + windows["needle_counts"][:-1],
        )
        first, counts = axes["first_needle_ids"], axes["needle_counts"]
        self.assertEqual(
            int(first[-1]) + int(counts[-1]), sum(expected["needle_counts"])
        )
        primary = np.flatnonzero(np.diff(axes["path_offsets"]) == 3)
        self.assertEqual(len(primary), sum(expected["primary_by_generator"].values()))
        graph = ParentProfileGraph.from_recipe(recipe)
        seen = set()
        for index in primary:
            lo, hi = map(int, axes["path_offsets"][index : index + 2])
            path = tuple(map(tuple, axes["path_pairs"][lo:hi].tolist()))
            pose = graph.instance(path)
            np.testing.assert_array_equal(axes["origins"][index], pose.origin)
            seen.add(path[-1][0])
            axis = support(axes, index)
            pattern = NeedlePattern.from_recipe(self.physical["patterns"][1])
            profile = recipe["nodes"][path[-1][0]]["shape"]["radius"]["table"]
            radii = np.array([profile[0], profile[-1]]) * axes["uniform_scales"][index]
            self.assertIsNone(
                pattern.detail(
                    axis,
                    radii,
                    recipe["seed"],
                    int(counts[index]) - 1,
                )
            )
            detail = pattern.detail(axis, radii, recipe["seed"], int(counts[index]))
            self.assertEqual(len(detail["ids"]), counts[index])
        self.assertEqual(seen, {2, 5})

    def test_physical_and_global_work_limits_refuse_incomplete_crowns(self):
        recipe = small_recipe(self.original)
        with patch("spruce_interwhorl.MAX_AXES", 1), self.assertRaises(ValueError):
            preflight(recipe, self.physical, self.metadata, self.areas)
        with patch("spruce_interwhorl.MAX_WINDOWS", 1), self.assertRaises(ValueError):
            preflight(recipe, self.physical, self.metadata, self.areas)
        recipe["nodes"][3]["placement"]["pattern"]["limit_groups"] = 1
        with self.assertRaises(ValueError):
            preflight(recipe, self.physical, self.metadata, self.areas)
        for factor, ratio in ((float("nan"), 0.7), (1.45, 1), (1.1, 0.7)):
            with self.assertRaises(ValueError):
                intercalated(self.original, factor, relative_length=ratio)

    def test_window_area_matches_scalar_global_variant_cycle(self):
        recipe = {
            "wood": {"seed": 815},
            "physical_source": copy.deepcopy(self.physical),
        }
        for i in (1, 2, 3):
            recipe["physical_source"]["patterns"][i]["scale_variation"] = i / 10
        windows = {
            "needle_counts": np.array([1, 2, 3, 4, 7, 257], dtype=np.uint32),
            "first_needle_ids": np.array([5, 6, 8, 11, 15, 22], dtype=np.uint64),
            "orders": np.array([1, 2, 3, 1, 2, 3], dtype=np.uint8),
        }
        expected = []
        for n, first, order in zip(*windows.values(), strict=True):
            area = sum(self.areas[(int(first) + i + 815) % 3] for i in range(int(n)))
            expected.append(area * (1 + (int(order) / 10) ** 2 / 3))
        actual = area_weights(windows, recipe, self.areas)
        np.testing.assert_allclose(actual, expected, rtol=1e-14)
        np.testing.assert_allclose(
            grouped_area(
                np.array([0.2, 0.9, 1.2]),
                np.array([0, 1, 2]),
                np.array([1.0, 2.0, 4.0]),
            ),
            [3.0, 4.0],
        )
        with self.assertRaises(ValueError):
            grouped_area(np.array([2.0]), np.array([0, 1]), np.array([1.0]))


if __name__ == "__main__":
    unittest.main()
