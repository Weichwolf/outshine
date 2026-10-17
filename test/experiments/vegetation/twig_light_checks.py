"""Actual corner field binding, camera units and independent normal-mixture limit."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import bpy
import numpy as np
from PIL import Image
from twig_light_blender import VIEW, bind_physical, bind_windows, configure_scene
from twig_light_render import projected_span
from twig_light_source import requested_axis


class TwigLightTests(unittest.TestCase):
    def test_physical_corner_colour_normals_and_leaf_membership_reach_mesh(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        geometry = {
            "vertices": np.array(
                [[-0.5, -0.5, 0], [0.5, -0.5, 0], [0.5, 0.5, 0], [-0.5, 0.5, 0]]
            ),
            "faces": np.array([[0, 1, 2], [0, 2, 3]]),
            "normals": np.tile([0.0, 0.0, 1.0], (2, 3, 1)),
            "colour": np.arange(18).reshape(2, 3, 3) / 36,
            "leaf": np.array([True, False]),
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-twig-light.", dir="/tmp"
        ) as name:
            directory = Path(name)
            for kind in ("colour", "normal", "leaf"):
                Image.new("RGBA", (1, 1), (255, 255, 255, 255)).save(
                    directory / f"{kind}.png"
                )
            obj = bind_physical(
                geometry,
                directory,
                {
                    "colour": "colour.png",
                    "normal": "normal.png",
                    "leaf_weight": "leaf.png",
                },
            )
            data = obj.data
            values = np.empty(24, np.float32)
            data.color_attributes["saved source reflectance"].data.foreach_get(
                "color", values
            )
            np.testing.assert_allclose(
                values.reshape(6, 4)[:, :3],
                geometry["colour"].reshape(-1, 3),
                atol=3e-8,
            )
            members = np.empty(2, np.float32)
            data.attributes["saved leaf membership"].data.foreach_get("value", members)
            np.testing.assert_array_equal(members, [1, 0])
            np.testing.assert_allclose(
                [tuple(n.vector) for n in data.corner_normals],
                [[0, 0, 1]] * 6,
                atol=1e-7,
            )
            surface = data.materials[0]
            shader = surface.node_tree.nodes.get("Principled BSDF")
            self.assertFalse(shader.inputs["Normal"].is_linked)
            self.assertEqual(
                shader.inputs["Base Color"].links[0].from_node.type, "VERTEX_COLOR"
            )

    def test_actual_camera_pixel_span_matches_declared_metric_extent(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        data = {"vertices": np.array([[0, -0.5, 0], [0, 0.5, 0], [0, 0, 0.1]])}
        configure_scene(1.0, 384, VIEW)
        actual = projected_span(data, bpy.context.scene)
        self.assertAlmostEqual(actual[0], 384, delta=1e-4)
        self.assertGreater(actual[1], 0)

    def test_symmetric_normal_mixture_cannot_use_its_normalized_mean_for_diffuse(self):
        normals = np.array([[np.sqrt(3) / 2, 0, 0.5], [-np.sqrt(3) / 2, 0, 0.5]])
        side = np.array([1.0, 0, 0])
        physical = float(np.maximum(normals @ side, 0).mean())
        mean = normals.mean(axis=0)
        mean /= np.linalg.norm(mean)
        collapsed = max(float(mean @ side), 0)
        self.assertAlmostEqual(physical, np.sqrt(3) / 4)
        self.assertEqual(collapsed, 0)
        # A known mathematical approximation limit, not proof of an atlas bug.
        self.assertAlmostEqual(float(np.linalg.norm(normals.mean(axis=0))), 0.5)

    def test_source_selection_rejects_missing_complete_tail_intervals(self):
        axes = {"path_offsets": np.array([0, 5]), "needle_counts": np.array([547])}
        windows = {
            "axis_ids": np.array([0, 0, 0]),
            "needle_counts": np.array([256, 256, 35]),
        }
        self.assertEqual(requested_axis(axes, windows), 0)
        windows["needle_counts"][-1] = 12
        with self.assertRaises(ValueError):
            requested_axis(axes, windows)

    def test_coaxial_single_surface_and_refusal_before_atlas_binding(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        image = np.zeros((32, 32, 9))
        image[..., :4] = [0.1, 0.2, 0.1, 1]
        image[..., 6] = 1
        image[..., 7] = 1
        bank = [
            (image, (np.eye(3), np.array([-0.5, -0.5]), np.array([0.5, 0.5]), 0))
        ] * 64
        window = {
            "families": np.array([0]),
            "frames": np.eye(3)[None],
            "origins": np.zeros((1, 3)),
            "weights": np.array([0.5]),
            "first_needle_ids": np.array([10]),
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-vegetation-twig-light.", dir="/tmp"
        ) as name:
            bind_windows(
                window, [bank], Path(name) / "atlas", VIEW, VIEW, merge_matching=True
            )
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        self.assertEqual(len(meshes), 1)
        self.assertEqual(len(meshes[0].data.polygons), 2)
        with (
            patch(
                "twig_light_blender.select_views",
                side_effect=[np.array([1]), np.array([2])],
            ),
            patch(
                "twig_light_blender.textures",
                side_effect=AssertionError("late atlas work"),
            ),
            self.assertRaisesRegex(ValueError, "identical complete"),
        ):
            bind_windows(
                window,
                [bank],
                Path("/tmp/unused-twig-atlas"),
                VIEW,
                -VIEW,
                merge_matching=True,
            )


if __name__ == "__main__":
    unittest.main()
