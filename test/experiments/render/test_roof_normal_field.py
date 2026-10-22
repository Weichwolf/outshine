"""Independent analytic normal, lineage, ready-reader and roof eligibility controls."""

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from bake_roof_normal_field import bake
from far_height_profiles import emit, plan_source
from roof_normal_field import roof_uv, sample_uv
from test_far_height_profiles import source


class RoofNormalField(unittest.TestCase):
    def test_gable_directions_and_source_owned_product(self):
        original = source()
        saved = copy.deepcopy(original)
        field = bake(plan_source(17, original))
        x, y, size = field["rects"][0]
        height, width = field["pixels"].shape[:2]
        uv = np.array(
            [[x + size / 2, y + size / 4], [x + size / 2, y + 3 * size / 4]]
        ) / [width, height]
        normal, valid = sample_uv(field, uv, np.eye(3))
        np.testing.assert_array_equal(valid, [True, True])
        expected = np.array([[0, -1, 1], [0, 1, 1]]) / np.sqrt(2)
        eps = np.sqrt(3) / 255
        self.assertTrue(
            np.all(np.linalg.norm(normal - expected, axis=1) < 2 * eps / (1 - eps))
        )
        rotation = np.array([[0, 1, 0], [-1, 0, 0], [0, 0, 1]])
        moved, _ = sample_uv(field, uv, rotation)
        np.testing.assert_allclose(moved, normal @ rotation.T, rtol=0, atol=1e-15)
        self.assertEqual(original, saved)

    def test_only_generated_roof_gets_normal_override(self):
        parts = plan_source(17, source(holes=True))
        mesh = emit(parts)
        mesh["plan_binding"] = mesh.pop("profile_binding")
        mesh["native_binding"] = np.full((len(mesh["indices"]), 6), -1, np.int32)
        field = bake(parts)
        uv = roof_uv(mesh, field, np.eye(3), np.zeros(3))
        roof = mesh["plan_binding"][:, 2] == 1
        np.testing.assert_array_equal(np.all(uv >= 0, axis=(1, 2)), roof)
        mesh["native_binding"][:] = 0
        self.assertIsNone(roof_uv(mesh, field, np.eye(3), np.zeros(3)))
        _, valid = sample_uv(field, np.array([[-1, -1], [2, 2]]), np.eye(3))
        self.assertFalse(valid.any())

    def test_identical_anonymous_forms_retain_distinct_ordinals_and_gutters(self):
        a, b = plan_source(17, source()), plan_source(23, source())
        field = bake([*a, *b])
        np.testing.assert_array_equal(field["keys"], [[17, 0], [23, 0]])
        np.testing.assert_array_equal(field["roof_codes"], [-82, -82])
        x, y, size = field["rects"][0]
        pixels = field["pixels"]
        np.testing.assert_array_equal(
            pixels[y - 1, x : x + size], pixels[y, x : x + size]
        )
        with self.assertRaisesRegex(ValueError, "Duplicate original"):
            bake([*a, *a])

    def test_flat_roofs_need_no_atlas_or_uv_and_unknown_keys_fail(self):
        original = source()
        original["shapes"][0]["roof_kind"] = 0
        parts = plan_source(17, original)
        field = bake(parts)
        self.assertEqual(field["pixels"].nbytes, 0)
        np.testing.assert_array_equal(field["no_field_codes"], [-83])
        _, valid = sample_uv(field, np.array([[0.5, 0.5]]), np.eye(3))
        self.assertFalse(valid.any())
        np.testing.assert_array_equal(field["no_field_keys"], [[17, 0]])
        mesh = emit(parts)
        mesh["plan_binding"] = mesh.pop("profile_binding")
        mesh["native_binding"] = np.full((len(mesh["indices"]), 6), -1, np.int32)
        self.assertIsNone(roof_uv(mesh, field, np.eye(3), np.zeros(3)))
        mesh["plan_binding"][:, 0] = 23
        with self.assertRaisesRegex(ValueError, "no contract"):
            roof_uv(mesh, field, np.eye(3), np.zeros(3))

    def test_fresh_ready_reader_uses_no_forms_or_geos(self):
        field = bake(plan_source(17, source()))
        with tempfile.TemporaryDirectory(
            prefix="outshine-roof-field-test-"
        ) as directory:
            path = Path(directory) / "field.npz"
            np.savez_compressed(path, **field)
            code = """import sys,json
sys.modules['shapely']=None
sys.modules['prepared_surface_recipes']=None
sys.modules['far_height_profiles']=None
import numpy as np
from roof_normal_field import sample_uv
with np.load(sys.argv[1],allow_pickle=False) as f:
 n,v=sample_uv(f,np.array([[0.5,0.5]]),np.eye(3))
print(json.dumps({'normal':n.tolist(),'valid':v.tolist()}))
"""
            result = json.loads(
                subprocess.check_output(
                    [sys.executable, "-B", "-W", "error", "-c", code, str(path)],
                    text=True,
                )
            )
            self.assertEqual(result["valid"], [True])
            self.assertAlmostEqual(np.linalg.norm(result["normal"][0]), 1)


if __name__ == "__main__":
    unittest.main()
