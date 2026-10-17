"""Scalar count/replay, real cached paths and unchanged legacy window controls."""

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_windows import bind_patterns, pattern_key, plan_windows
from generator_graph import Instance
from generator_specs import Generator, Placement, TubeShape
from outshine_ref.vegetation import (
    AxisLevel,
    DistancePattern,
    Needle,
    NeedledPlant,
    NeedlePattern,
    PlantPlan,
    Profile,
)
from outshine_ref.vegetation.needle_windows import window_tables
from parent_profile_graph import ParentProfileGraph
from spruce_growth_execute import execute
from spruce_growth_probe import measure
from spruce_growth_product import pack_axes, support


def control():
    fixed = {"pitch_variance": 0, "scale_variance": 0, "azimuth_variance": 0}
    nodes = (
        Generator(0, "root", "root", ()),
        Generator(
            1,
            "stem",
            "axis",
            (0,),
            TubeShape(1, 0.1, Profile(np.linspace(0.03, 0.01, 4))),
            Placement(first=0, last=0, pitch=0, **fixed),
        ),
        Generator(
            2,
            "primary",
            "axis",
            (1,),
            TubeShape(0.3, 0.1, Profile(np.linspace(0.003, 0.001, 4))),
            Placement(
                count=2,
                first=0.2,
                last=0.8,
                scale_variance=0.12,
                pitch_variance=0,
                azimuth_variance=0,
            ),
        ),
        Generator(
            3,
            "secondary",
            "axis",
            (2,),
            TubeShape(0.15, 0.1, Profile(np.linspace(0.0003, 0.0001, 4))),
            Placement(
                first=0.1,
                last=0.9,
                pattern=DistancePattern(
                    spacing=Profile(np.full(4, 0.05)), limit_groups=4
                ),
                **fixed,
            ),
        ),
        Generator(
            4,
            "terminal",
            "axis",
            (3,),
            TubeShape(0.06, 0.1, Profile(np.linspace(0.00003, 0.00001, 4))),
            Placement(first=0.4, last=0.4, **fixed),
        ),
    )
    graph = ParentProfileGraph(
        17,
        nodes,
        parent_length_ratios=((3, 0.5), (4, 0.4)),
        inclination_profiles=((2, Profile(np.linspace(1, 1.2, 4))),),
    )
    pattern = NeedlePattern(spacing=0.01, group_size=1, short_length=0)
    physical = {"patterns": [None, *[pattern.recipe()] * 3]}
    metadata = {pattern_key(pattern): {"needles": 8, "family": 0, "phase": 0}}
    return graph, pattern, physical, metadata


class SpruceGrowthTests(unittest.TestCase):
    def test_count_only_lengths_counts_and_later_complete_execution_agree(self):
        graph, pattern, physical, metadata = control()
        result = measure(graph.recipe(), physical, metadata, np.array((1e-5,)))
        descriptor, axes, rows, reports = execute(
            graph.recipe(), physical, metadata, result
        )
        self.assertEqual(descriptor, graph.recipe())
        self.assertEqual(len(axes["origins"]), result["axes"])
        self.assertEqual(len(rows["origins"]), result["windows"])
        self.assertEqual([r["needles"] for r in reports], result["needle_counts"])
        for index in range(1, len(axes["origins"])):
            axis = support(axes, index)
            self.assertEqual(
                int(axes["needle_counts"][index]), pattern.count(axis.length)
            )
        np.testing.assert_array_equal(
            rows["first_needle_ids"][1:],
            rows["first_needle_ids"][:-1] + rows["needle_counts"][:-1],
        )
        self.assertLess(result["prepared_poses"], result["axes"])

    def test_full_pose_expansion_is_forbidden_during_preflight_and_budget_rejects(self):
        graph, _, physical, metadata = control()
        original = ParentProfileGraph.instance

        def bounded(instance, path):
            if len(path) > 3:
                raise AssertionError("full secondary poses during count-only stage")
            return original(instance, path)

        with (
            patch.object(ParentProfileGraph, "instance", new=bounded),
            patch("spruce_growth_probe.MAX_AXES", 5),
            self.assertRaises(ValueError),
        ):
            measure(graph.recipe(), physical, metadata, np.array((1e-5,)))

    def test_real_large_graph_slots_survive_cache_and_produce_physical_needles(self):
        pose = Instance(
            ((0, 0), (1, 0), (2, 239), (3, 200), (4, 0)),
            np.array((2.0, 3, 4)),
            np.eye(3),
            1,
            0.03,
            0.1,
            np.array((2.0, 3, 4)),
        )
        pattern = NeedlePattern(spacing=0.003, group_size=1)
        count = pattern.count(pose.length)
        axes = pack_axes([pose], [count])
        axis = support(axes, 0)
        self.assertEqual(axis.path, (239, 200, 0))
        detail = pattern.detail(axis, (0.0002, 0.0001), 815, count)
        self.assertEqual(len(detail["ids"]), count)
        np.testing.assert_array_equal(detail["ids"], np.arange(count))
        self.assertTrue(np.isfinite(detail["origins"]).all())

    def test_original_window_arrays_and_binding_remain_exact(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-old-windows-", dir="/tmp"
        ) as temp:
            old = Path(temp) / "original.py"
            old.write_bytes(
                subprocess.check_output(
                    [
                        "git",
                        "show",
                        "8bbcb5503:lib/src/outshine_ref/vegetation/needle_windows.py",
                    ]
                )
            )
            spec = importlib.util.spec_from_file_location(
                "outshine_ref.vegetation._original_windows", old
            )
            previous = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(previous)
            for seed in (0, 29, 2**32 - 1):
                plant = NeedledPlant(
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
                before = previous.window_tables(plant, 100, 10000)
                after = window_tables(plant, 100, 10000)
                for key in before:
                    self.assertEqual(before[key].dtype, after[key].dtype)
                    np.testing.assert_array_equal(before[key], after[key])
                pattern = plant.patterns[1]
                info = {
                    pattern_key(pattern): {
                        "needles": int(after["capacities"][0]),
                        "family": 0,
                        "phase": 0,
                    }
                }
                bound = bind_patterns(plant.patterns, after, info)
                direct = plan_windows(plant, info)
                for key in bound:
                    np.testing.assert_array_equal(bound[key], direct[key])
                info[pattern_key(pattern)]["needles"] += 1
                with self.assertRaises(ValueError):
                    bind_patterns(plant.patterns, after, info)


if __name__ == "__main__":
    unittest.main()
