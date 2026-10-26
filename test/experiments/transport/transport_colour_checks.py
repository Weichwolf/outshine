"""Actual optional linear face-colour import and GPU shader input consumption."""

import tempfile
import unittest
from pathlib import Path

import bpy
import numpy as np
from camera_height_inputs import digest
from outshine_ref.rendering.blender_camera import configure_camera
from render_transport_research import import_body
from transport_surface_colour import COLOUR_ATTRIBUTE


class TransportColourChecks(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def source(self, colours):
        path = self.root / "body.npz"
        np.savez_compressed(
            path,
            vertices=[[0.0, 0, 0], [1.0, 0, 0], [1.0, 1, 0], [0.0, 1, 0]],
            triangles=[[0, 1, 2], [0, 2, 3]],
            normals=np.tile([0.0, 0, 1], (6, 1)),
            materials=[0, 0],
            face_base_colour_linear_rgba=colours,
        )
        return {
            "id": "face-colours",
            "mesh": str(path),
            "mesh_sha256": digest(path),
            "colours": [[0.5, 0.5, 0.5, 1.0]],
        }

    def test_exact_linear_corner_colours_reach_base_colour(self):
        colours = np.array([[0.08, 0.2, 0.4, 1], [0.4, 0.1, 0.02, 1]], np.float32)
        row = self.source(colours)
        _v, proof = import_body(row)
        self.assertTrue(proof["source_base_colour_attribute_float32_exact"])
        obj = bpy.data.objects[row["id"]]
        attribute = obj.data.attributes[COLOUR_ATTRIBUTE]
        self.assertEqual(
            (attribute.domain, attribute.data_type), ("CORNER", "FLOAT_COLOR")
        )
        actual = np.empty((6, 4), np.float32)
        attribute.data.foreach_get("color", actual.ravel())
        np.testing.assert_array_equal(actual, np.repeat(colours, 3, axis=0))
        shader = obj.data.materials[0].node_tree.nodes.get("Principled BSDF")
        self.assertEqual(
            shader.inputs["Base Color"].links[0].from_node.attribute_name,
            COLOUR_ATTRIBUTE,
        )

    def test_wrong_count_nan_srgb_range_and_transparency_rejected(self):
        for colours in [
            np.ones((1, 4)),
            [[np.nan, 0, 0, 1]] * 2,
            [[1.2, 0, 0, 1]] * 2,
            [[0.2, 0.2, 0.2, 0.5]] * 2,
        ]:
            with self.subTest(colours=colours), self.assertRaises(ValueError):
                bpy.ops.wm.read_factory_settings(use_empty=True)
                import_body(self.source(colours))

    def test_actual_pixels_keep_two_face_colours_and_missing_binding_fails(self):
        from PIL import Image

        colours = np.array([[0.08, 0.2, 0.4, 1], [0.4, 0.1, 0.02, 1]], np.float32)
        row = self.source(colours)
        import_body(row)
        obj = bpy.data.objects[row["id"]]
        material = obj.data.materials[0]
        shader = material.node_tree.nodes.get("Principled BSDF")
        source = shader.inputs["Base Color"].links[0].from_socket
        emission = material.node_tree.nodes.new("ShaderNodeEmission")
        material.node_tree.links.new(source, emission.inputs["Color"])
        material.node_tree.links.new(
            emission.outputs[0],
            material.node_tree.nodes.get("Material Output").inputs["Surface"],
        )
        scene = bpy.context.scene
        scene.render.engine = "BLENDER_EEVEE"
        scene.render.threads_mode, scene.render.threads = "FIXED", 2
        scene.eevee.taa_render_samples = 8
        scene.view_settings.view_transform = "Raw"
        scene.view_settings.exposure = 0
        scene.camera = configure_camera(
            {
                "id": "face-colours",
                "projection": "orthographic",
                "eye": [0.5, 0.5, 5],
                "target": [0.5, 0.5, 0],
                "camera_basis": np.eye(3).tolist(),
                "span_m": 1.4,
                "profile": [64, 64],
            }
        )
        scene.render.resolution_x = 64
        scene.render.resolution_y = 64
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        path = self.root / "actual.png"
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        with Image.open(path) as im:
            actual = np.array(im.convert("RGB")) / 255
        np.testing.assert_allclose(actual[43, 43], colours[0, :3], rtol=0, atol=2 / 255)
        np.testing.assert_allclose(actual[20, 20], colours[1, :3], rtol=0, atol=2 / 255)
        material.node_tree.links.remove(emission.inputs["Color"].links[0])
        emission.inputs["Color"].default_value = [0.5, 0.5, 0.5, 1]
        scene.render.filepath = str(self.root / "missing.png")
        bpy.ops.render.render(write_still=True)
        with Image.open(scene.render.filepath) as im:
            missing = np.array(im.convert("RGB")) / 255
        self.assertGreater(float(np.max(abs(missing[43, 43] - colours[0, :3]))), 0.1)


if __name__ == "__main__":
    unittest.main()
