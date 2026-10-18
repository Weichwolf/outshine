"""Actual Blender vertex payloads and Double-origin subtraction before float upload."""

import hashlib
import tempfile
import unittest
from pathlib import Path

import bpy
import numpy as np
from render_transport_solid import imported_body


class RegionalRenderBindings(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)

    def test_actual_mesh_keeps_float32_bits_and_large_world_origin_difference(self):
        vertices = np.array([[0.125, 0, 0], [0, 0.25, 0], [0, 0, 0.5]], dtype="f4")
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-render-bind-", dir="/tmp"
        ) as directory:
            path = Path(directory) / "body.npz"
            np.savez_compressed(
                path,
                vertices=vertices,
                triangles=np.array([[0, 1, 2]]),
                materials=np.array([0]),
            )
            root = np.full(3, 1e9)
            item = {
                "mesh": str(path),
                "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "origin_m": (root + [0.125, 0.25, 0.5]).tolist(),
                "encoded_float32": True,
            }
            actual, count, body, _binding = imported_body(
                {"id": "binding"}, item, "binding", root
            )
            self.assertEqual(count, 1)
            np.testing.assert_array_equal(np.asarray(body.location), [0.125, 0.25, 0.5])
            np.testing.assert_array_equal(
                actual, vertices.astype(float) + [0.125, 0.25, 0.5]
            )

    def test_shared_encoded_world_point_has_same_actual_blender_position(self):
        world = np.array(
            [
                [1e9 + 0.125, 1e9 + 0.25, 1e9 + 0.5],
                [1e9 + 1, 1e9 + 2, 1e9 + 3],
                [1e9 + 2, 1e9 + 4, 1e9 + 6],
            ]
        )
        root = np.full(3, 1e9)
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-render-bind-", dir="/tmp"
        ) as directory:
            for index, origin in enumerate((root, root + 64)):
                vertices = (world - origin).astype("f4")
                path = Path(directory) / (str(index) + ".npz")
                np.savez_compressed(
                    path,
                    vertices=vertices,
                    triangles=np.array([[0, 1, 2]]),
                    materials=np.array([0]),
                )
                item = {
                    "mesh": str(path),
                    "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "origin_m": origin.tolist(),
                    "encoded_float32": True,
                }
                imported_body({"id": "binding"}, item, str(index), root)
            bpy.context.view_layer.update()
            # Object transforms must be evaluated after their assignment.
            imported = [
                np.array([tuple(obj.matrix_world @ v.co) for v in obj.data.vertices])
                for obj in bpy.context.scene.objects
            ]
            np.testing.assert_array_equal(*imported)


if __name__ == "__main__":
    unittest.main()
