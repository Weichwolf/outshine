"""Independent phase/frame controls and exact legacy graph compatibility."""

import copy
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

import numpy as np
from generator_graph import GeneratorGraph
from generator_specs import Generator, Placement, TubeShape
from outshine_ref.vegetation import DistancePattern, Profile
from parent_profile_graph import ParentProfileGraph
from spruce_branch_planes import branch_form
from spruce_study_source import INPUT, read_input
from spruce_whorl_balance import balanced


def specimen(phase=0.0, jitter=0.0):
    return GeneratorGraph(
        815,
        (
            Generator(0, "root", "root", ()),
            Generator(
                1,
                "parent",
                "axis",
                (0,),
                TubeShape(length=1, bend=0.2),
                Placement(pitch=0, pitch_variance=0, phase=0, scale_variance=0),
            ),
            Generator(
                2,
                "sides",
                "axis",
                (1,),
                TubeShape(length=0.2),
                Placement(
                    first=0.2,
                    last=0.8,
                    pitch=1.1,
                    pitch_variance=0.1,
                    azimuth_variance=jitter,
                    scale_variance=0,
                    pattern=DistancePattern(
                        spacing=Profile(np.full(4, 0.1)), divergence=np.pi
                    ),
                    phase=phase,
                ),
            ),
        ),
    )


class BranchPhaseTests(unittest.TestCase):
    def test_scalar_parent_phase_origin_direction_and_seeded_variation(self):
        for phase, jitter in ((0.0, 0.0), (0.37, 0.18)):
            graph = specimen(phase, jitter)
            parent = graph.instance(((0, 0), (1, 0)))
            children = graph.instances(2, 100)
            self.assertEqual(len(children), 7)
            for slot, pose in enumerate(children):
                t = 0.8 - slot * 0.1
                a = 0.2 * t
                centre = np.array((0, np.sin(a) / 0.2, (1 - np.cos(a)) / 0.2))
                tangent = np.array((0, np.cos(a), np.sin(a)))
                lateral = np.array((1, 0, 0))
                normal = np.array((0, -np.sin(a), np.cos(a)))
                rng = np.random.default_rng(
                    np.random.SeedSequence([815, 0, 0, 1, 0, 2, slot])
                )
                azimuth = phase + slot * np.pi + rng.uniform(-jitter, jitter)
                pitch = 1.1 + rng.uniform(-0.1, 0.1)
                radial = np.cos(azimuth) * lateral + np.sin(azimuth) * normal
                direction = np.cos(pitch) * tangent + np.sin(pitch) * radial
                np.testing.assert_allclose(
                    pose.origin, parent.origin + parent.frame @ centre, atol=1e-15
                )
                np.testing.assert_allclose(
                    pose.frame[:, 1], parent.frame @ direction, atol=8e-16
                )
                np.testing.assert_allclose(
                    pose.frame.T @ pose.frame, np.eye(3), atol=2e-15
                )
                self.assertAlmostEqual(np.linalg.det(pose.frame), 1, places=14)
                np.testing.assert_array_equal(pose.origin, pose.anchor)

    def test_explicit_version_roundtrip_and_invalid_controls(self):
        original = specimen(0.37, 0.18)
        record = json.loads(json.dumps(original.recipe()))
        replay = GeneratorGraph.from_recipe(record)
        self.assertEqual(original.identity, replay.identity)
        for a, b in zip(
            original.instances(2, 100), replay.instances(2, 100), strict=True
        ):
            np.testing.assert_array_equal(a.origin, b.origin)
            np.testing.assert_array_equal(a.frame, b.frame)
        for value in (True, np.nan, np.inf, [0.0], np.pi + 1e-10):
            with self.assertRaises(ValueError):
                Placement(phase=value)
        for value in (
            None,
            0.0,
            {"version": "other", "radians": 0.0},
            {"version": "fixed-parent-azimuth-1", "radians": None},
        ):
            bad = copy.deepcopy(record)
            bad["nodes"][2]["placement"]["phase"] = value
            with self.assertRaises(ValueError):
                GeneratorGraph.from_recipe(bad)

    def test_legacy_random_phase_keeps_exact_buffers_and_descriptors(self):
        current = specimen(None, 0.18)
        current = GeneratorGraph(
            current.seed,
            tuple(
                replace(n, placement=replace(n.placement, phase=None))
                for n in current.nodes
            ),
        )
        self.assertTrue(
            all("phase" not in n["placement"] for n in current.recipe()["nodes"])
        )
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-phase-reference-", dir="/tmp"
        ) as temp:
            path = Path(temp) / "legacy_graph.py"
            path.write_bytes(
                subprocess.check_output(
                    [
                        "git",
                        "show",
                        "360b58469:test/experiments/vegetation/generator_graph.py",
                    ]
                )
            )
            spec = importlib.util.spec_from_file_location("legacy_phase_graph", path)
            module = importlib.util.module_from_spec(spec)
            sys.modules[spec.name] = module
            try:
                spec.loader.exec_module(module)
            finally:
                sys.modules.pop(spec.name, None)
            legacy = module.GeneratorGraph(current.seed, current.nodes)
            self.assertEqual(current.recipe(), legacy.recipe())
            self.assertEqual(current.identity, legacy.identity)
            for a, b in zip(
                current.instances(2, 100), legacy.instances(2, 100), strict=True
            ):
                np.testing.assert_array_equal(a.origin, b.origin)
                np.testing.assert_array_equal(a.frame, b.frame)
                self.assertEqual(a.scale, b.scale)

    def test_span_compensation_preserves_actual_fine_lengths_sites_and_budget(self):
        _, source = read_input(INPUT)
        control = balanced(source, 48, 1.6)
        self.assertEqual(branch_form(control), control)
        changed = branch_form(control, planar=True, fixed=True, span=1.25)
        a, b = map(ParentProfileGraph.from_recipe, (control, changed))
        before, after = a.instances(2, 242), b.instances(2, 242)
        self.assertEqual(len(before), len(after))
        for i in (0, 60, 120, 180, 239):
            self.assertAlmostEqual(after[i].length / before[i].length, 1.25, places=14)
            x, y = a.site_table(3, before[i]), b.site_table(3, after[i])
            self.assertEqual(len(x), len(y))
            np.testing.assert_allclose(x[:, 0], y[:, 0], atol=3e-15, rtol=0)
            expected = (
                before[i].length
                * a.ratios[3]
                * a.by_id[3].placement.scale.evaluate(x[:, 0])
            )
            actual = (
                after[i].length
                * b.ratios[3]
                * b.by_id[3].placement.scale.evaluate(y[:, 0])
            )
            np.testing.assert_allclose(actual, expected, atol=2e-15, rtol=0)
        self.assertEqual(changed["nodes"][4], control["nodes"][4])
        self.assertEqual(changed["inclinations"], control["inclinations"])
        for kwargs in (
            {"span": True},
            {"span": 1.5},
            {"fixed": True},
            {"span": np.nan},
        ):
            with self.assertRaises(ValueError):
                branch_form(control, **kwargs)


if __name__ == "__main__":
    unittest.main()
