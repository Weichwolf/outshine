"""Real requested geometry, allocation admission and frozen matched-projection controls."""

import copy
import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from cached_twig_source import local_windows, physical_axis
from conifer_window_catalog import family_geometry_for_seed, template_image
from conifer_window_comparison import compare_sources
from conifer_window_render import tree_bounds
from conifer_windows import bind_patterns, pattern_key
from foliage_selection import directions
from outshine_ref.vegetation import Needle, NeedlePattern, Profile
from outshine_ref.vegetation.needle_windows import _axis_window_values
from parent_profile_graph import ParentProfileGraph
from spruce_growth_product import pack_axes, support
from spruce_study_source import INPUT, read_input
from spruce_whorl_balance import balanced


class CachedTwigTests(unittest.TestCase):
    def test_declared_large_subpixel_reference_has_enough_raster_pair_capacity(self):
        geometry = {
            "vertices": np.array(
                ((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)), float
            ),
            "faces": np.array(((0, 1, 2), (0, 2, 3)), np.int32),
            "normals": np.broadcast_to((0.0, 0.0, 1.0), (2, 3, 3)),
            "colour": np.full((2, 3, 3), 0.2),
            "leaf": np.ones(2, bool),
        }
        image, _ = template_image(
            geometry,
            np.array((0, 0, 1)),
            128,
            8,
            projection_data=(
                np.eye(3),
                np.array((-1.0, -1.0)),
                np.array((1.0, 1.0)),
                0,
            ),
        )
        np.testing.assert_array_equal(image[..., 3], np.ones((128, 128)))
        np.testing.assert_allclose(image[..., :3], 0.2, atol=1e-7)

    @classmethod
    def setUpClass(cls):
        data, base = read_input(INPUT)
        graph = ParentProfileGraph.from_recipe(balanced(base, 48, 1.6))
        pose = graph.instance(((0, 0), (1, 0), (2, 239)))
        cls.pattern = NeedlePattern.from_recipe(data["physical_source"]["patterns"][1])
        cls.axes = pack_axes([pose], [cls.pattern.count(pose.length)])
        cls.axes["first_needle_ids"][0] = 41
        cls.graph, cls.physical = graph.recipe(), data["physical_source"]
        cls.meshes = []
        for params in data["physical_source"]["needles"]:
            mesh = Needle(**params).mesh(8, 8)
            colour = np.broadcast_to((0.035, 0.105, 0.042), (len(mesh["faces"]), 3, 3))
            cls.meshes.append((mesh, colour))
        source, info = family_geometry_for_seed(718, cls.pattern, cls.meshes, 32)
        info["family"] = 0
        cls.banks = [[template_image(source, d, 32, 2) for d in directions(64)]]
        raw = _axis_window_values(
            support(cls.axes, 0),
            cls.pattern,
            int(cls.axes["needle_counts"][0]),
            32,
            815,
            41,
        )
        cls.windows = bind_patterns(
            [None, cls.pattern], raw, {pattern_key(cls.pattern): info}
        )

    def test_actual_world_and_local_detail_variants_meshes_and_owner_ranges(self):
        source, detail, facts = physical_axis(
            self.graph, self.axes, self.physical, self.meshes, 0
        )
        axis = support(self.axes, 0)
        shape = self.graph["nodes"][2]["shape"]
        radius = (
            Profile(
                np.array(shape["radius"]["table"]), shape["radius"]["kind"]
            ).evaluate(np.array((0, 1)))
            * self.axes["uniform_scales"][0]
        )
        expected = self.pattern.detail(axis, radius, 815, facts["needles"])
        np.testing.assert_allclose(
            detail["origins"] @ axis.frame.T + axis.origin,
            expected["origins"],
            atol=8e-15,
        )
        np.testing.assert_allclose(
            np.einsum("ij,njk->nik", axis.frame, detail["frames"]),
            expected["frames"],
            atol=8e-16,
        )
        np.testing.assert_array_equal(detail["scales"], expected["scales"])
        counts = np.bincount(
            (np.arange(41, 41 + facts["needles"]) + 815) % 3, minlength=3
        )
        self.assertEqual(facts["variants"], counts.tolist())
        self.assertEqual(
            int(source["leaf"].sum()),
            facts["needles"] * len(self.meshes[0][0]["faces"]),
        )
        self.assertEqual(len(source["faces"]), facts["triangles"])
        self.assertGreaterEqual(int(source["faces"].min()), 0)
        self.assertLess(int(source["faces"].max()), len(source["vertices"]))
        np.testing.assert_allclose(
            np.linalg.norm(source["normals"], axis=-1), 1, atol=2e-14
        )
        near = local_windows(self.axes, self.windows, 0)
        self.assertEqual(int(near["needle_counts"].sum()), facts["needles"])
        np.testing.assert_allclose(
            near["origins"] @ axis.frame.T + axis.origin,
            self.windows["origins"],
            atol=8e-15,
        )
        np.testing.assert_array_equal(
            near["first_needle_ids"], self.windows["first_needle_ids"]
        )

    def test_needle_and_triangle_capacity_precede_detail_allocation(self):
        for count, limit in ((1025, 250000), (500, 1)):
            axes = copy.deepcopy(self.axes)
            axes["needle_counts"][0] = count
            with (
                patch("cached_twig_source.MAX_TRIANGLES", limit),
                patch.object(
                    NeedlePattern, "detail", side_effect=AssertionError("too late")
                ),
                self.assertRaisesRegex(ValueError, "budgets"),
            ):
                physical_axis(self.graph, axes, self.physical, self.meshes, 0)

    def test_window_subset_keeps_actual_seeded_needle_positions_and_global_variants(
        self,
    ):
        source, details, facts = physical_axis(
            self.graph, self.axes, self.physical, self.meshes, 0, needle_range=(5, 8)
        )
        self.assertEqual(facts["needles"], 3)
        self.assertEqual(facts["first_needle_id"], 46)
        self.assertEqual(facts["variants"], [1, 1, 1])
        first = next(i for i in range(5, 8) if (41 + i + 815) % 3 == 0)
        mesh = self.meshes[0][0]
        expected = (
            mesh["vertices"] * details["scales"][first] @ details["frames"][first].T
            + details["origins"][first]
        )
        np.testing.assert_allclose(
            source["vertices"][: len(mesh["vertices"])], expected, atol=1e-16
        )
        self.assertEqual(int(source["leaf"].sum()), 3 * len(mesh["faces"]))
        for interval in ((-1, 5), (5, 5), (0, 10000), (True, 3)):
            with (
                patch.object(
                    NeedlePattern, "detail", side_effect=AssertionError("too late")
                ),
                self.assertRaises(ValueError),
            ):
                physical_axis(
                    self.graph,
                    self.axes,
                    self.physical,
                    self.meshes,
                    0,
                    needle_range=interval,
                )

    def test_original_matched_projection_metrics_remain_exact(self):
        source, _, facts = physical_axis(
            self.graph, self.axes, self.physical, self.meshes, 0
        )
        near = local_windows(self.axes, self.windows, 0)
        wood = {
            "vertices": np.empty((0, 3)),
            "faces": np.empty((0, 3), np.int32),
            "normals": np.empty((0, 3, 3)),
        }
        bounds = tree_bounds(near, self.banks, source)
        source["axis_path"] = facts["path"]
        source["ids"] = [("leaf", i) for i in range(facts["needles"])]
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-old-projection-", dir="/tmp"
        ) as temp:
            path = Path(temp) / "legacy.py"
            path.write_bytes(
                subprocess.check_output(
                    [
                        "git",
                        "show",
                        "813112870:test/experiments/vegetation/conifer_window_comparison.py",
                    ]
                )
            )
            spec = importlib.util.spec_from_file_location("legacy_twig_compare", path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            with patch.object(
                module,
                "shoot_sources",
                return_value=(source, near, self.banks, wood, bounds),
            ):
                expected = module.compare_one(
                    "picea-abies", Path("/tmp/unused"), size=32
                )["directions"]
        actual = compare_sources(source, near, self.banks, wood, bounds, size=32)
        self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
