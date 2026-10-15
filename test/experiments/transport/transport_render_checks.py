"""Actual Blender attribute/shader consumption, independent of packed mesh normal spaces."""

import json
import tempfile
import unittest
from pathlib import Path

import bpy
import numpy as np
from camera_height_inputs import digest
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera
from render_transport_research import NORMAL_ATTRIBUTE, import_body


class TransportRendering(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.vertices = np.array([[0.0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])
        self.faces = np.array([[0, 1, 2], [0, 2, 3]])
        self.normal = np.array([0.2, 0.4, np.sqrt(0.8)], np.float32)
        self.normals = np.tile(self.normal, (6, 1))

    def tearDown(self):
        self.temporary.cleanup()

    def source(self, *, normals=None):
        path = self.root / "source.npz"
        np.savez_compressed(
            path,
            vertices=self.vertices,
            triangles=self.faces,
            normals=self.normals if normals is None else normals,
            materials=np.zeros(2, int),
        )
        return {
            "id": "normal-control",
            "mesh": str(path),
            "mesh_sha256": digest(path),
            "colours": [[0.5, 0.5, 0.5, 1]],
        }

    def test_original_normals_reach_the_actual_shader_as_float_vectors(self):
        row = self.source()
        _vertices, proof = import_body(row)
        self.assertTrue(proof["source_normal_attribute_float32_exact"])
        obj = bpy.data.objects[row["id"]]
        attribute = obj.data.attributes[NORMAL_ATTRIBUTE]
        self.assertEqual(
            (attribute.domain, attribute.data_type), ("CORNER", "FLOAT_VECTOR")
        )
        actual = np.empty_like(self.normals)
        attribute.data.foreach_get("vector", actual.ravel())
        np.testing.assert_array_equal(actual, self.normals)
        material = obj.data.materials[0]
        shader = material.node_tree.nodes.get("Principled BSDF")
        normalized = shader.inputs["Normal"].links[0].from_node
        self.assertEqual(normalized.operation, "NORMALIZE")
        transform = normalized.inputs[0].links[0].from_node
        self.assertEqual(
            (transform.convert_from, transform.convert_to), ("OBJECT", "WORLD")
        )
        self.assertEqual(
            transform.inputs["Vector"].links[0].from_node.attribute_name,
            NORMAL_ATTRIBUTE,
        )

    def test_stale_or_wrong_domain_source_is_rejected(self):
        row = self.source()
        with self.assertRaises(ValueError):
            import_body(dict(row, mesh_sha256="0" * 64))
        with self.assertRaises(ValueError):
            import_body(self.source(normals=self.normals[:4]))

    def test_eevee_reads_the_source_attribute_in_a_real_pixel(self):
        from PIL import Image

        row = self.source()
        import_body(row)
        obj = bpy.data.objects[row["id"]]
        material = obj.data.materials[0]
        shader = material.node_tree.nodes.get("Principled BSDF")
        source_vector = shader.inputs["Normal"].links[0].from_socket
        emission = material.node_tree.nodes.new("ShaderNodeEmission")
        material.node_tree.links.new(source_vector, emission.inputs["Color"])
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
        config = {
            "id": "normal-control",
            "output": str(self.root / "normal.png"),
            "projection": "orthographic",
            "eye": [0.5, 0.5, 5],
            "target": [0.5, 0.5, 0],
            "camera_basis": np.eye(3).tolist(),
            "span_m": 1.4,
            "profile": [64, 64],
        }
        scene.camera = configure_camera(config)
        scene.render.resolution_x, scene.render.resolution_y = config["profile"]
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        scene.render.filepath = config["output"]
        bpy.context.view_layer.update()
        projection = np.asarray(
            scene.camera.calc_matrix_camera(
                bpy.context.evaluated_depsgraph_get(), x=64, y=64
            )
        )
        verify_camera(
            config,
            projection,
            np.asarray(scene.camera.matrix_world.inverted()),
            self.vertices,
        )
        bpy.ops.render.render(write_still=True)
        with Image.open(config["output"]) as image:
            actual = np.array(image.convert("RGB"))[32, 32] / 255
        np.testing.assert_allclose(actual, self.normal, rtol=0, atol=2 / 255)
        print(
            json.dumps(
                {
                    "control": "actual_eevee_source_normal_pixel",
                    "pixel": actual.tolist(),
                    "expected": self.normal.tolist(),
                }
            ),
            flush=True,
        )
        (self.root / "proof.json").write_text(
            json.dumps({"pixel": actual.tolist(), "expected": self.normal.tolist()})
        )


if __name__ == "__main__":
    unittest.main()
