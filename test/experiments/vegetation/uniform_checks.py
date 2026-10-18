"""Independent inherited-length, cyclic-area and actual graph-demand controls."""

import math
import unittest
from itertools import pairwise
from unittest.mock import patch

import numpy as np
from fine_hierarchy_execute import execute
from outshine_ref.vegetation import NeedlePattern, Profile
from parent_profile_graph import ParentProfileGraph
from spruce_fine_hierarchy import child_lengths
from spruce_study_source import INPUT, binding_metadata, read_input
from spruce_uniform import span_areas, uniform


class UniformHierarchyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, cls.original = read_input(INPUT)
        cls.physical = cls.data["physical_source"]
        cls.metadata = binding_metadata(cls.data["reference_catalog_metadata"])

    def test_bounded_complete_child_lengths_against_independent_scalar_formula(self):
        graph = ParentProfileGraph.from_recipe(uniform(self.original))
        parents = np.array([0.05, 0.21, 0.8, 2.0, 10.0])
        expected = []
        for length in parents:
            count = min(19, math.floor((0.95 - 0.16) * length / 0.015) + 1)
            for group in range(count):
                t = 0.95 - (group / (count - 1) if count > 1 else 0) * (0.95 - 0.16)
                expected.append(length * 0.7 * (1.05 - 0.8 * t))
        np.testing.assert_allclose(
            child_lengths(graph, 4, parents, 1000), expected, rtol=2e-15
        )
        with (
            patch.object(
                Profile, "evaluate", side_effect=AssertionError("late scale work")
            ),
            self.assertRaises(ValueError),
        ):
            child_lengths(graph, 4, parents, 0)

    def test_cyclic_area_of_disjoint_global_spans_matches_scalar_organs(self):
        counts = np.array([0, 1, 2, 3, 6, 10, 17])
        starts = np.array([0, 2, 5, 13, 15, 100, 203])
        areas = np.array([0.13, 0.27, 0.31])
        for seed in (0, 815):
            expected = [
                sum(float(areas[(int(start) + seed + i) % 3]) for i in range(int(n)))
                * (1 + 0.12**2 / 3)
                for n, start in zip(counts, starts, strict=True)
            ]
            np.testing.assert_allclose(
                span_areas(counts, starts, areas, seed, 0.12),
                expected,
                rtol=1e-15,
                atol=1e-16,
            )

    def test_actual_variable_graph_and_demand_replay_match_the_complete_scalar_plan(
        self,
    ):
        recipe = uniform(self.original, 3)
        main = recipe["nodes"][2]
        main["shape"]["length"] = 0.6
        main["placement"].update(first=0.12, last=0.2)
        main["placement"]["scale"]["table"] = [1.0] * 4
        main["placement"]["pattern"].update(limit_groups=1, whorl_size=2)
        graph = ParentProfileGraph.from_recipe(recipe)
        primaries = graph.instances(2, 100)
        values = [np.array([33.25]), np.array([p.length for p in primaries])]
        for key in (3, 4):
            values.append(child_lengths(graph, key, values[-1], 1000))
        counts, windows = [], 0
        for p, lengths in zip(self.physical["patterns"], values, strict=True):
            pattern = NeedlePattern.from_recipe(p) if p else None
            local = (
                [pattern.count(float(length)) for length in lengths]
                if p
                else [0] * len(lengths)
            )
            counts.append(sum(local))
            windows += sum((n + 255) // 256 for n in local)
        expected = {
            "axes": sum(map(len, values)),
            "orders": list(map(len, values)),
            "needle_counts": counts,
            "windows": windows,
            "graph_identity": graph.identity,
        }
        with patch.object(
            ParentProfileGraph, "instances", side_effect=AssertionError("bulk poses")
        ):
            _, axes, rows, reports = execute(
                recipe, self.physical, self.metadata, expected
            )
        self.assertEqual(len(rows["origins"]), windows)
        self.assertEqual(reports[-1]["stored_graph_poses"], expected["axes"] + 1)
        paths = [
            tuple(map(tuple, axes["path_pairs"][int(a) : int(b)].tolist()))
            for a, b in pairwise(axes["path_offsets"])
        ]
        fresh = ParentProfileGraph.from_recipe(recipe)
        for index in (0, len(paths) // 2, len(paths) - 1):
            actual = fresh.instance(paths[index])
            np.testing.assert_array_equal(actual.origin, axes["origins"][index])
            np.testing.assert_array_equal(actual.frame, axes["rotations"][index])
        self.assertLess(len(fresh._poses), 20)


if __name__ == "__main__":
    unittest.main()
