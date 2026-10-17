"""Independent profile-only changes and full interval/control preservation."""

import copy
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import DistancePattern, Profile
from parent_profile_graph import ParentProfileGraph
from spruce_growth_checks import control
from spruce_growth_probe import internode_profile
from upper_crown_profiles import profiles, variants


class UpperCrownTests(unittest.TestCase):
    def test_density_bias_preserves_legacy_tables_and_complete_interval(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-profile-", dir="/tmp"
        ) as temp:
            path = Path(temp) / "reference.py"
            path.write_bytes(
                subprocess.check_output(
                    [
                        "git",
                        "show",
                        "1f6bf7674:test/experiments/vegetation/spruce_growth_probe.py",
                    ]
                )
            )
            spec = importlib.util.spec_from_file_location("old_spruce_profile", path)
            old = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(old)
        for seed in (1, 815):
            args = (33.25, 0.118, 0.995, 36, seed, 0.35)
            before = old.internode_profile(*args)
            np.testing.assert_array_equal(before, internode_profile(*args))
            counts = []
            for bias in (0, 0.5, 1.0):
                spacing = internode_profile(*args, upper_bias=bias)
                self.assertTrue(np.all(np.array(spacing) > 0))
                pattern = DistancePattern(
                    spacing=Profile(np.array(spacing)), whorl_size=5, limit_groups=36
                )
                rows = pattern.tables(np.array((33.25,)), 0.118, 0.995, max_sites=180)[
                    0
                ]
                self.assertEqual(rows.shape, (180, 2))
                self.assertLess(abs(rows[:, 0].min() - 0.118) * 33.25, 1e-9)
                self.assertEqual(rows[:, 0].max(), 0.995)
                counts.append(int((rows[:, 0] >= 0.75).sum()))
            self.assertLess(counts[0], counts[1])
            self.assertLess(counts[1], counts[2])
        for bias in (-1, 1.6, float("nan")):
            with self.assertRaises(ValueError):
                internode_profile(33.25, 0.118, 0.995, 36, 815, 0.35, upper_bias=bias)

    def test_inclination_only_preserves_lengths_sites_origins_and_old_input(self):
        graph, *_ = control()
        original = graph.recipe()
        frozen = copy.deepcopy(original)
        candidate = ParentProfileGraph.from_recipe(profiles(original, 1.05))
        self.assertEqual(original, frozen)
        for before, after in zip(
            graph.instances(2, 200), candidate.instances(2, 200), strict=True
        ):
            self.assertEqual(before.path, after.path)
            self.assertEqual(before.length, after.length)
            self.assertEqual(before.scale, after.scale)
            np.testing.assert_array_equal(before.origin, after.origin)
            parent = candidate.instance(after.path[:-1])
            t = candidate.site_table(2, parent)[after.path[-1][1], 0]
            angle = 1.04 + (1.05 - 1.04) * (t - 0.2) / (0.8 - 0.2)
            tangent = parent.frame @ np.array((0, np.cos(0.1 * t), np.sin(0.1 * t)))
            self.assertAlmostEqual(
                after.frame[:, 1] @ tangent, np.cos(angle), places=14
            )
        for a, b in zip(original["nodes"], candidate.recipe()["nodes"], strict=True):
            self.assertEqual(a, b)

    def test_leader_interval_complete_whorls_and_profile_endpoints(self):
        graph, *_ = control()
        original = graph.recipe()
        primary = original["nodes"][2]["placement"]
        primary["first"], primary["last"] = 0.118, 0.96
        primary["pattern"] = DistancePattern(
            spacing=Profile(np.full(4, 0.8)), whorl_size=5, limit_groups=36
        ).recipe()
        original["nodes"][1]["shape"]["length"] = 33.25
        changed = profiles(
            original, 1.05, last=0.995, terminal_fraction=0.02, exponent=0.9
        )
        placement = changed["nodes"][2]["placement"]
        pattern = DistancePattern.from_recipe(placement["pattern"])
        table = pattern.tables(np.array((33.25,)), 0.118, 0.995, max_sites=180)[0]
        self.assertEqual(table.shape, (180, 2))
        self.assertLess(abs(table[::5, 0].min() - 0.118) * 33.25, 1e-9)
        self.assertEqual(table[::5, 0].max(), 0.995)
        shape = Profile(np.array(placement["scale"]["table"]))
        base = float(Profile(np.array(primary["scale"]["table"])).evaluate(0.118))
        # Uniform2001point table includes this author's exact0.118/0.995knots.
        self.assertAlmostEqual(
            shape.evaluate(0.5),
            base * (0.02 + 0.98 * ((0.995 - 0.5) / (0.995 - 0.118)) ** 0.9),
            places=5,
        )
        self.assertAlmostEqual(shape.evaluate(0.118), base, places=14)
        self.assertAlmostEqual(shape.evaluate(0.995), base * 0.02, places=14)
        self.assertTrue(np.all(np.diff(shape.table) <= 0))
        self.assertEqual(changed["nodes"][3:], original["nodes"][3:])

    def test_count_variants_need_no_descendant_growth_and_invalid_controls_fail(self):
        graph, *_ = control()
        original = graph.recipe()
        original["nodes"][2]["placement"]["pattern"] = DistancePattern(
            spacing=Profile(np.ones(4)), whorl_size=5, limit_groups=36
        ).recipe()
        original["nodes"][2]["placement"]["last"] = 0.96
        with patch.object(
            ParentProfileGraph, "instances", side_effect=AssertionError("full growth")
        ):
            cases = variants(original)
        self.assertEqual(len(cases), 8)
        self.assertEqual(cases[0][1], original)
        self.assertEqual(cases[0][1], json.loads(json.dumps(cases[0][1])))
        for fields in (
            {"upper_angle": 0.5},
            {"upper_angle": 1.05, "last": 1.0},
            {"upper_angle": 1.05, "last": 0.99},
            {"upper_angle": 1.05, "terminal_fraction": 0.5},
        ):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                profiles(original, **fields)


if __name__ == "__main__":
    unittest.main()
