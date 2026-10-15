"""Actual Eevee source-normal attributes, material links and inverse-transpose scale."""

import tempfile
import unittest
from pathlib import Path

import bpy
import numpy as np
from blender_city_mesh import NORMAL_ATTRIBUTE, import_mesh
from outshine_ref.rendering.blender_camera import configure_camera
from PIL import Image


class CityNormals(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.normal = np.array([0.2, 0.4, np.sqrt(0.8)])
        self.data = {
            "vertices": np.array(
                [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [1.0, 1.0, 0.0], [0.0, 1.0, 0.0]]
            ),
            "faces": np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32),
            "normals": np.tile(self.normal, (4, 1)),
            "uv": np.array([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]),
            "materials": np.zeros(2, dtype=np.uint16),
        }
        self.obj, self.proof = import_mesh(
            {
                "city": "normal-control",
                "source": {"material_definitions": {}},
                "facade_materials": {},
            },
            self.data,
        )

    def test_explicit_float32_vectors_and_material_normal_path(self):
        values = np.empty((6, 3), np.float32)
        self.obj.data.attributes[NORMAL_ATTRIBUTE].data.foreach_get(
            "vector", values.ravel()
        )
        np.testing.assert_array_equal(
            values,
            self.data["normals"][self.data["faces"]].reshape(-1, 3).astype(np.float32),
        )
        self.assertTrue(self.proof["source_normal_attribute_float32_exact"])
        shader = self.obj.data.materials[0].node_tree.nodes["Principled BSDF"]
        normalize = shader.inputs["Normal"].links[0].from_node
        self.assertEqual(normalize.operation, "NORMALIZE")
        transform = normalize.inputs[0].links[0].from_node
        self.assertEqual(
            (transform.vector_type, transform.convert_from, transform.convert_to),
            ("NORMAL", "OBJECT", "WORLD"),
        )
        self.assertEqual(
            transform.inputs["Vector"].links[0].from_node.attribute_name,
            NORMAL_ATTRIBUTE,
        )

    def test_real_eevee_normal_pixel_including_nonuniform_scale(self):
        surface = self.obj.data.materials[0]
        tree = surface.node_tree
        source = tree.nodes["Principled BSDF"].inputs["Normal"].links[0].from_socket
        emission = tree.nodes.new("ShaderNodeEmission")
        tree.links.new(source, emission.inputs["Color"])
        tree.links.new(
            emission.outputs[0], tree.nodes["Material Output"].inputs["Surface"]
        )
        scene = bpy.context.scene
        scene.render.engine = "BLENDER_EEVEE"
        scene.render.threads_mode, scene.render.threads = "FIXED", 2
        scene.eevee.taa_render_samples = 8
        scene.view_settings.view_transform = "Raw"
        scene.view_settings.exposure = 0
        scene.render.resolution_x = scene.render.resolution_y = 64
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests." + Path.cwd().name + "-normal-"
        ) as folder:
            for index, scale in enumerate(((1.0, 1.0, 1.0), (2.0, 0.5, 1.0))):
                self.obj.scale = scale
                target = [scale[0] / 2, scale[1] / 2, 0.0]
                config = {
                    "eye": [target[0], target[1], 5.0],
                    "target": target,
                    "projection": "orthographic",
                    "span_m": 2.4,
                    "camera_basis": np.eye(3).tolist(),
                }
                scene.camera = configure_camera(config)
                scene.render.filepath = str(Path(folder) / (str(index) + ".png"))
                bpy.context.view_layer.update()
                bpy.ops.render.render(write_still=True)
                with Image.open(scene.render.filepath) as im:
                    pixel = np.asarray(im.convert("RGB"))[32, 32] / 255.0
                expected = self.normal / scale
                expected /= np.linalg.norm(expected)
                np.testing.assert_allclose(pixel, expected, rtol=0, atol=2 / 255.0)
                print(
                    "Actual source-normal Eevee pixel",
                    scale,
                    pixel.tolist(),
                    "expected",
                    expected.tolist(),
                    flush=True,
                )


if __name__ == "__main__":
    unittest.main()
