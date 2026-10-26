"""Blender retains supplied topology, point/corner normals and UVs through import."""

import importlib.util
import unittest

import numpy as np
from outshine_ref.rendering.blender_mesh import mesh_object


@unittest.skipUnless(
    importlib.util.find_spec("bpy"), "Optional Blender host is unavailable"
)
class BlenderMeshes(unittest.TestCase):
    def setUp(self):
        import bpy

        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.surface = bpy.data.materials.new("control")
        self.vertices = np.array(
            [[0.0, 0.0, 0.0], [2.0, 0.0, 0.0], [2.0, 3.0, 0.0], [0.0, 3.0, 0.0]]
        )
        self.faces = np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32)
        self.normals = np.tile([0.0, 0.0, 1.0], (4, 1))

    def test_supplied_point_normals_uv_and_topology(self):
        uv = self.vertices[:, :2] / [2.0, 3.0]
        obj = mesh_object(
            "control", self.vertices, self.faces, self.normals, self.surface, uv=uv
        )
        positions = np.empty(self.vertices.size, dtype=np.float32)
        obj.data.vertices.foreach_get("co", positions)
        np.testing.assert_array_equal(
            positions.reshape(-1, 3), self.vertices.astype(np.float32)
        )
        indices = np.empty(self.faces.size, dtype=np.int32)
        obj.data.loops.foreach_get("vertex_index", indices)
        np.testing.assert_array_equal(indices.reshape(-1, 3), self.faces)
        actual = np.empty(self.faces.size * 2, dtype=np.float32)
        obj.data.uv_layers.active.data.foreach_get("uv", actual)
        np.testing.assert_array_equal(
            actual.reshape(-1, 2), uv[self.faces].reshape(-1, 2)
        )

    def test_explicit_corner_normals_remain_different(self):
        normals = np.array([[0.0, 0.0, 1.0]] * 3 + [[0.0, 0.6, 0.8]] * 3)
        obj = mesh_object(
            "corners",
            self.vertices,
            self.faces,
            normals,
            self.surface,
            corner_normals=True,
        )
        actual = np.empty(normals.size, dtype=np.float32)
        obj.data.corner_normals.foreach_get("vector", actual)
        np.testing.assert_allclose(actual.reshape(-1, 3), normals, rtol=0, atol=1e-4)

    def test_invalid_domains_and_indices_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "point/corner"):
            mesh_object(
                "bad",
                self.vertices,
                self.faces,
                self.normals,
                self.surface,
                corner_normals=True,
            )
        with self.assertRaisesRegex(ValueError, "UVs"):
            mesh_object(
                "bad",
                self.vertices,
                self.faces,
                self.normals,
                self.surface,
                uv=np.zeros((3, 2)),
            )
        with self.assertRaisesRegex(ValueError, "indices"):
            mesh_object(
                "bad", self.vertices, self.faces + 100, self.normals, self.surface
            )


if __name__ == "__main__":
    unittest.main()
