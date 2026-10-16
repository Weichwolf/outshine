"""Scene-transform and existing file/array mesh binding controls, no GPU required."""

import tempfile
import unittest
from pathlib import Path

import bpy
import numpy as np
from render_fern import model, model_arrays
from render_stand_prototype import scene_population
from stand_pack_checks import fixture
from stand_prototype_scene import place_template, world_matrix


class StandSceneChecks(unittest.TestCase):
    def test_pair_poses_and_uniform_scale_have_the_declared_world_action(self):
        _, origins, frames, scales, context, digest = scene_population("pair", None)
        self.assertEqual(origins.shape, (2, 3))
        self.assertEqual(context, [])
        self.assertIsNone(digest)
        np.testing.assert_allclose(np.linalg.det(frames), 1, atol=2e-15)
        point = np.array((0.2, -0.4, 0.7, 1))
        for origin, frame, scale in zip(origins, frames, scales):
            expected = origin + frame @ point[:3] * scale
            actual = np.array(world_matrix(origin, frame, scale)) @ point
            np.testing.assert_allclose(actual[:3], expected, atol=3e-7)

    def test_all_modes_refuse_bad_world_transforms_before_publishing_objects(self):
        count = len(bpy.data.objects)
        empty = (None, None, None, None)
        for mode in ("direct", "nested", "packed"):
            with self.assertRaises(ValueError):
                place_template(
                    empty, np.zeros((1, 3)), np.eye(3)[None], np.array((-1.0,)), mode
                )
            with self.assertRaises(ValueError):
                place_template(
                    empty, np.zeros((1, 3)), np.zeros((1, 3, 3)), np.ones(1), mode
                )
        self.assertEqual(len(bpy.data.objects), count)

    def test_existing_file_model_and_array_model_preserve_bound_source_attributes(self):
        _, meshes = fixture()
        source = meshes[0]
        mats = [bpy.data.materials.new(f"stand fixture {i}") for i in range(7)]
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.vegetation-scene-"
        ) as temp:
            path = Path(temp) / "leaf.npz"
            np.savez_compressed(path, **source)
            objects = (model(path, mats), model_arrays("array leaf", source, mats))
            for obj in objects:
                vertices = np.empty(source["vertices"].size, np.float32)
                obj.data.vertices.foreach_get("co", vertices)
                np.testing.assert_array_equal(
                    vertices.reshape(-1, 3), source["vertices"].astype(np.float32)
                )
                normals = np.empty(source["normals"].size, np.float32)
                obj.data.attributes["fern_source_normal"].data.foreach_get(
                    "vector", normals
                )
                np.testing.assert_array_equal(
                    normals.reshape(-1, 3), source["normals"].astype(np.float32)
                )
                uv = np.empty(source["corner_uv"].size, np.float32)
                obj.data.uv_layers.active.data.foreach_get("uv", uv)
                np.testing.assert_array_equal(
                    uv.reshape(-1, 2), source["corner_uv"].astype(np.float32)
                )
                index = np.empty(len(source["surface"]), np.int32)
                obj.data.polygons.foreach_get("material_index", index)
                np.testing.assert_array_equal(index, source["surface"])


if __name__ == "__main__":
    unittest.main()
