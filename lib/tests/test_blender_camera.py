"""Actual Blender matrices independently reproduce declared perspective and ortho extents."""

import importlib.util
import tempfile
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera


@unittest.skipUnless(
    importlib.util.find_spec("bpy"), "Optional Blender host is unavailable"
)
class BlenderCameras(unittest.TestCase):
    def setUp(self):
        import bpy

        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.scene = bpy.context.scene
        self.folder = tempfile.TemporaryDirectory(
            prefix="outshine-tests." + Path.cwd().name + "-blender-camera-"
        )
        self.addCleanup(self.folder.cleanup)
        self.vertices = np.array([[-1.0, -1.0, 0.0], [1.0, 1.0, 2.0]])

    def configuration(self, projection):
        return {
            "id": "control",
            "output": str(Path(self.folder.name) / "image.png"),
            "eye": [12.0, -18.0, 9.0],
            "target": [0.0, 0.0, 1.0],
            "projection": projection,
            "profile": [1280, 720],
            "span_m": 20.0,
            "vertical_fov_deg": 48.0,
            "clip_near_m": 0.1,
            "clip_far_m": 10000.0,
        }

    def matrices(self, config):
        import bpy

        camera = configure_camera(config)
        self.scene.camera = camera
        self.scene.render.resolution_x, self.scene.render.resolution_y = config[
            "profile"
        ]
        bpy.context.view_layer.update()
        projection = np.asarray(
            camera.calc_matrix_camera(
                bpy.context.evaluated_depsgraph_get(),
                x=config["profile"][0],
                y=config["profile"][1],
            )
        )
        return projection, np.asarray(camera.matrix_world.inverted())

    def test_actual_perspective_camera_and_vertical_fov(self):
        config = self.configuration("perspective")
        config |= {"eye": [12, -18, 9], "target": [0, 0, 1]}
        projection, view = self.matrices(config)
        proof = verify_camera(config, projection, view, self.vertices)
        np.testing.assert_allclose(
            proof["vertical_ndc_probes"], [[0, 0], [0, 1], [0, -1]], atol=2e-5
        )
        with self.assertRaisesRegex(ValueError, "vertical FOV"):
            verify_camera(
                config | {"vertical_fov_deg": 65.0}, projection, view, self.vertices
            )

    def test_orthographic_horizontal_span_for_both_aspects(self):
        for profile in ([1280, 720], [720, 1280]):
            config = self.configuration("orthographic") | {"profile": profile}
            projection, view = self.matrices(config)
            proof = verify_camera(config, projection, view, self.vertices)
            np.testing.assert_allclose(
                proof["ndc_probes"],
                [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1]],
                atol=2e-5,
            )
            with self.assertRaisesRegex(ValueError, "orthographic span"):
                verify_camera(
                    config | {"span_m": 25.0}, projection, view, self.vertices
                )

    def test_translation_preserves_projection_and_input_vertices(self):
        config = self.configuration("orthographic")
        shift = np.array([2000.0, -6000.0, 100.0])
        config |= {
            "eye": (np.asarray(config["eye"]) + shift).tolist(),
            "target": (np.asarray(config["target"]) + shift).tolist(),
        }
        points = self.vertices + shift
        before = points.copy()
        projection, view = self.matrices(config)
        verify_camera(config, projection, view, points)
        np.testing.assert_array_equal(points, before)

    def test_whole_ortho_model_rejects_depth_clipped_vertices(self):
        config = self.configuration("orthographic")
        projection, view = self.matrices(config)
        eye, target = np.asarray(config["eye"]), np.asarray(config["target"])
        behind = eye + (eye - target)
        with self.assertRaisesRegex(ValueError, "outside"):
            verify_camera(config, projection, view, np.r_[self.vertices, [behind]])

    def test_whole_ortho_model_rejects_visible_and_offscreen_mixture(self):
        config = self.configuration("orthographic")
        projection, view = self.matrices(config)
        with self.assertRaisesRegex(ValueError, "outside"):
            verify_camera(
                config, projection, view, np.r_[self.vertices, [[1000.0, 0.0, 1.0]]]
            )


if __name__ == "__main__":
    unittest.main()
