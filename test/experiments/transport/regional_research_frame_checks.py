"""Whole real topology through actual Eevee imports and changing camera frames."""

import tempfile
import unittest
from pathlib import Path

import bpy
import numpy as np
from camera_height_inputs import digest
from regional_render_frame import render_camera, scene_bindings
from render_transport_research import import_body, import_scene
from stair_regional_precision import position_payload


class RegionalResearchFrames(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="outshine-regional-frame-")
        self.root = Path(self.temporary.name)
        source = (
            Path(__file__).parents[2]
            / "outshine/data/transport/regional-terrain-topology.npz"
        )
        with np.load(source) as saved:
            self.points = np.c_[saved["xy"], np.zeros(len(saved["xy"]))]
            self.faces = saved["faces"]

    def tearDown(self):
        self.temporary.cleanup()

    def body(self, name, *, regional):
        origin = np.array([-704, 320, 0.0])
        vertices = position_payload(self.points, origin) if regional else self.points
        path = self.root / (name + ".npz")
        np.savez_compressed(
            path,
            vertices=vertices,
            triangles=self.faces,
            normals=np.tile([0.0, 0, 1], (len(self.faces) * 3, 1)),
            materials=np.zeros(len(self.faces), dtype="u2"),
        )
        result = {
            "id": name,
            "mesh": str(path),
            "mesh_sha256": digest(path),
            "colours": [[0.2, 0.3, 0.4, 1]],
        }
        return result | (
            {"origin_m": origin.tolist(), "encoded_float32": True} if regional else {}
        )

    def test_whole_real_geometry_and_models_are_translation_invariant_in_both_frames(
        self,
    ):
        body = self.body("whole", regional=True)
        for motion in (np.zeros(3), np.array([8, -4, 1])):
            results = []
            for shift in (0, 2**30):
                bpy.ops.wm.read_factory_settings(use_empty=True)
                eye = np.array([-200.125, -300.25, 600.5]) + motion + shift
                row = {
                    "id": "scene",
                    "eye": eye.tolist(),
                    "target": (np.array([-704, 320, 0]) + motion + shift).tolist(),
                    "render_origin_m": eye.tolist(),
                    "bodies": [
                        dict(
                            body,
                            origin_m=(np.asarray(body["origin_m"]) + shift).tolist(),
                        )
                    ],
                }
                actual, proof = import_scene(row)
                camera = render_camera(row)
                bpy.ops.object.camera_add(location=camera["eye"])
                bpy.context.view_layer.update()
                frame = scene_bindings(row, proof, bpy.context.object)
                obj = bpy.data.objects[body["id"]]
                np.testing.assert_array_equal(np.asarray(obj.rotation_euler), [0, 0, 0])
                np.testing.assert_array_equal(np.asarray(obj.scale), [1, 1, 1])
                self.assertEqual(proof["parts"][0]["triangles"], 1896)
                results.append(
                    (
                        actual,
                        proof["parts"][0]["regional_binding"],
                        camera["target"],
                        frame,
                    )
                )
            np.testing.assert_array_equal(results[0][0], results[1][0])
            np.testing.assert_array_equal(
                results[0][1]["actual_model_matrix_f32"],
                results[1][1]["actual_model_matrix_f32"],
            )
            self.assertEqual(
                results[0][1]["actual_vertex_payload_sha256"],
                results[1][1]["actual_vertex_payload_sha256"],
            )
            self.assertEqual(results[0][2], results[1][2])

    def test_legacy_input_and_default_binding_remain_exact(self):
        body = self.body("legacy", regional=False)
        actual, proof = import_body(body)
        np.testing.assert_array_equal(actual, self.points)
        self.assertNotIn("regional_binding", proof)
        np.testing.assert_array_equal(
            np.asarray(bpy.data.objects[body["id"]].matrix_basis), np.eye(4)
        )

    def test_invalid_owner_payload_and_frame_fail_instead_of_silent_requantization(
        self,
    ):
        body = self.body("legacy", regional=False)
        with self.assertRaisesRegex(ValueError, "actual Float32"):
            import_body(dict(body, origin_m=[0, 0, 0], encoded_float32=True))
        for frame in ([0, 0], [0, float("nan"), 0]):
            with self.assertRaises(ValueError):
                render_camera(
                    {"render_origin_m": frame, "eye": [0, 0, 0], "target": [0, 1, 0]}
                )


if __name__ == "__main__":
    unittest.main()
