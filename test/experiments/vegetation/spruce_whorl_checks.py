"""Fresh fixed-input regeneration and independent macro/fine allocation controls."""

import copy
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_window_catalog import family_geometry, family_geometry_for_seed
from europe_conifer_recipes import conifers
from outshine_ref.vegetation import NeedledPlant, PlantPlan
from parent_profile_graph import ParentProfileGraph
from spruce_study_source import INPUT, binding_metadata, create_source, read_input
from spruce_whorl_balance import balanced


class SourceRebuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-source-", dir="/tmp"
        )
        cls.root = Path(cls.temp.name)
        with (
            patch.object(
                NeedledPlant, "from_recipe", side_effect=AssertionError("legacy parser")
            ),
            patch.object(
                NeedledPlant, "foliage", side_effect=AssertionError("whole plant")
            ),
            patch.object(PlantPlan, "axis", side_effect=AssertionError("growth")),
        ):
            cls.report = create_source(
                INPUT, cls.root / "source", verify_reference=False
            )

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_fresh_sources_keep_separate_seeds_layout_and_physical_channels(self):
        data, _ = read_input(INPUT)
        self.assertEqual(self.report["plant_seed"], 815)
        self.assertEqual(self.report["catalog_seed"], 718)
        # Strict same-host hash compatibility is an explicit CLI proof, not a
        # cross-platform floating-point contract for this ordinary control.
        self.assertFalse(self.report["reference_fields_verified"])
        for actual, expected in zip(
            self.report["needle_fields"],
            data["reference_fields"]["needle_meshes"],
            strict=True,
        ):
            for name, values in actual.items():
                self.assertEqual(values["shape"], expected[name]["shape"])
                self.assertEqual(values["dtype"], expected[name]["dtype"])
        with np.load(
            self.root / "source/catalog/family-0.npz", allow_pickle=False
        ) as stored:
            image = stored["images"]
            self.assertEqual(image.shape, (64, 32, 32, 9))
            self.assertTrue(np.isfinite(image).all())
            self.assertTrue(np.all((image[..., 3] >= 0) & (image[..., 3] <= 1)))
            self.assertTrue(
                np.all(np.linalg.norm(image[..., 4:7], axis=-1) <= image[..., 3] + 2e-7)
            )
        recipe = json.loads((self.root / "source/plant-recipe.json").read_text())
        self.assertNotIn("wood", recipe["physical_source"])
        metadata = json.loads((self.root / "source/catalog/catalog.json").read_text())
        actual = binding_metadata(metadata)
        expected = binding_metadata(data["reference_catalog_metadata"])
        self.assertEqual(actual.keys(), expected.keys())
        for key in actual:
            for field in actual[key]:
                if field == "source_extent_m":
                    np.testing.assert_allclose(
                        actual[key][field], expected[key][field], rtol=0, atol=1e-12
                    )
                else:
                    self.assertEqual(actual[key][field], expected[key][field])
        # Order-specific attachment ranges do not enter canonical family sampling.
        self.assertNotEqual(metadata, data["reference_catalog_metadata"])

    def test_invalid_inputs_fail_before_creating_outputs(self):
        for key, value in (
            ("groups", 0),
            ("groups", 2**32),
            ("seed", True),
            ("directions", 128),
        ):
            data = json.loads(INPUT.read_text())
            data["catalog"][key] = value
            path = self.root / "invalid.json"
            path.write_text(json.dumps(data))
            output = self.root / "must-not-exist"
            with self.assertRaises(ValueError):
                create_source(path, output)
            self.assertFalse(output.exists())

    def test_family_triangle_capacity_precedes_canonical_expansion(self):
        data = json.loads(INPUT.read_text())
        data["physical_source"]["patterns"][1]["group_size"] = 40
        path = self.root / "large.json"
        path.write_text(json.dumps(data))
        with (
            patch(
                "spruce_study_source.family_geometry_for_seed",
                side_effect=AssertionError("too late"),
            ),
            self.assertRaisesRegex(ValueError, "triangle budget"),
        ):
            create_source(path, self.root / "large")


class WhorlAllocationTests(unittest.TestCase):
    def test_unmodified_case_is_exact_and_macro_allocation_preserves_shapes(
        self,
    ):
        _, original = read_input(INPUT)
        before = copy.deepcopy(original)
        self.assertEqual(balanced(original, 36, 1.0), original)
        candidate = balanced(original, 48, 1.6)
        self.assertEqual(original, before)
        graph = ParentProfileGraph.from_recipe(candidate)
        primary = graph.instances(2, 242)
        self.assertEqual(len(primary), 240)
        self.assertEqual(len(graph._poses), 242)
        self.assertEqual(primary[-1].path[-1], (2, 239))
        for parent in (primary[0], primary[-1]):
            rows = graph.site_table(3, parent)
            spacing = 0.01013175 * 1.6
            count = min(201, int(np.floor((0.95 - 0.16) * parent.length / spacing)) + 1)
            self.assertEqual(len(rows), count)
            np.testing.assert_allclose(
                rows[:, 0],
                0.95 - np.arange(count) * spacing / parent.length,
                rtol=0,
                atol=2e-14,
            )
        self.assertEqual(candidate["nodes"][4], original["nodes"][4])
        for index in range(1, 5):
            self.assertEqual(
                candidate["nodes"][index]["shape"], original["nodes"][index]["shape"]
            )
        self.assertEqual(candidate["inclinations"], original["inclinations"])
        for n, scale in ((True, 1.0), (52, 1.0), (48, 0.5), (48, float("nan"))):
            with self.assertRaises(ValueError):
                balanced(original, n, scale)

    def test_legacy_seed_wrapper_retains_canonical_geometry_and_metadata(self):
        species = conifers()[0]
        plant = species.plant(718)
        pattern = species.patterns[1]
        mesh = species.needles[0].mesh(8, 8)
        colour = np.broadcast_to((0.035, 0.105, 0.042), (len(mesh["faces"]), 3, 3))
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-old-family-", dir="/tmp"
        ) as temp:
            path = Path(temp) / "reference.py"
            path.write_bytes(
                subprocess.check_output(
                    [
                        "git",
                        "show",
                        "6df639bf9:test/experiments/vegetation/conifer_window_catalog.py",
                    ]
                )
            )
            spec = importlib.util.spec_from_file_location("legacy_family", path)
            old = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(old)
            expected, info = old.family_geometry(plant, pattern, [(mesh, colour)], 8)
        for source, metadata in (
            family_geometry(plant, pattern, [(mesh, colour)], 8),
            family_geometry_for_seed(718, pattern, [(mesh, colour)], 8),
        ):
            self.assertEqual(metadata, info)
            for name in expected:
                np.testing.assert_array_equal(source[name], expected[name])


if __name__ == "__main__":
    unittest.main()
