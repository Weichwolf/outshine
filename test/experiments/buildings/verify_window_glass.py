"""Real importer bindings: preserve geometry and explicit source materials."""

import unittest
from pathlib import Path

import bpy
import numpy as np
from blender_city import render
from blender_city_mesh import NORMAL_ATTRIBUTE, import_mesh
from blender_window_glass import configure


class WindowGlass(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.data = {
            "vertices": np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]]),
            "faces": np.array([[0, 1, 2]], dtype=np.uint32),
            "normals": np.tile([0.0, 0.0, 1.0], (3, 1)),
            "uv": np.array([[0.0, 0.0], [1.0, 0.0], [0.0, 1.0]]),
            "materials": np.array([3], dtype=np.uint16),
        }
        self.report = {
            "city": "glass-binding",
            "source": {"material_definitions": {}},
            "facade_materials": {},
        }

    def test_default_retains_original_principled_and_scene(self):
        obj, _ = import_mesh(self.report, self.data)
        tree = obj.data.materials[0].node_tree
        shader = tree.nodes["Principled BSDF"]
        roughness = shader.inputs["Roughness"].default_value
        nodes = len(tree.nodes)
        proof = configure(obj, bpy.context.scene, "source")
        self.assertEqual(len(tree.nodes), nodes)
        self.assertEqual(shader.inputs["Roughness"].default_value, roughness)
        self.assertEqual(shader.inputs["Transmission Weight"].default_value, 1)
        self.assertEqual(proof["pane_triangles"], 1)
        self.assertFalse(bpy.context.scene.eevee.use_raytracing)

    def test_explicit_source_definition_is_not_silently_overridden(self):
        self.report["source"]["material_definitions"]["3"] = {
            "color.r": 0.4,
            "color.g": 0.3,
            "color.b": 0.2,
            "roughness": 0.8,
            "metallic": 0,
        }
        obj, _ = import_mesh(self.report, self.data)
        proof = configure(obj, bpy.context.scene, "source")
        self.assertEqual(proof["pane_triangles"], 0)
        with self.assertRaises(ValueError):
            configure(obj, bpy.context.scene, "thin-slab")
        self.assertEqual(obj.data.materials[0].name, "material-3")

    def test_thin_keeps_positions_faces_and_source_normal_path(self):
        obj, _ = import_mesh(self.report, self.data)
        before = np.array([v.co[:] for v in obj.data.vertices])
        indices = [tuple(p.vertices) for p in obj.data.polygons]
        configure(obj, bpy.context.scene, "thin-slab")
        np.testing.assert_array_equal(before, [v.co[:] for v in obj.data.vertices])
        self.assertEqual(indices, [tuple(p.vertices) for p in obj.data.polygons])
        tree = obj.data.materials[0].node_tree
        shader = tree.nodes["Principled BSDF"]
        fresnel = next(n for n in tree.nodes if n.type == "FRESNEL")
        self.assertEqual(
            fresnel.inputs["Normal"].links[0].from_socket,
            shader.inputs["Normal"].links[0].from_socket,
        )
        self.assertIn(NORMAL_ATTRIBUTE, obj.data.attributes)
        self.assertFalse(bpy.context.scene.eevee.use_raytracing)

    def test_unknown_profile_rejects_before_changes(self):
        obj, _ = import_mesh(self.report, self.data)
        with self.assertRaises(ValueError):
            configure(obj, bpy.context.scene, "missing")

    def test_render_rejects_output_outside_tmp_before_reading_inputs(self):
        for path in (
            "/var/tmp/outshine-buildings-test",
            "/tmp",
            "/tmp/../var/tmp/test",
        ):
            with self.assertRaises(ValueError):
                render(None, Path(path), 16)


if __name__ == "__main__":
    unittest.main()
