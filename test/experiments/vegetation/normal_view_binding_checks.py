"""Actual Blender view grouping preserves every source quad, weight and identity."""

import unittest
from unittest.mock import patch

import bpy
import numpy as np
from normal_component_portrait import bind_views
from window_portrait_blender import bind_mesh
from window_portrait_geometry import planes


class ViewBindingTests(unittest.TestCase):
    def test_empty_view_demand_never_builds_material_or_geometry(self):
        with (
            patch(
                "normal_component_portrait.material",
                side_effect=AssertionError("material"),
            ),
            patch(
                "normal_component_portrait.bind_mesh",
                side_effect=AssertionError("geometry"),
            ),
        ):
            self.assertEqual(
                bind_views(
                    None,
                    None,
                    None,
                    None,
                    None,
                    None,
                    np.array([], int),
                    0,
                    "four",
                    None,
                ),
                (0, 0, 0, 0),
            )

    def test_grouped_views_preserve_actual_geometry_and_complete_window_membership(
        self,
    ):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        image = np.zeros((32, 32, 9), np.float32)
        image[..., 3] = 1
        image[..., 6] = 1
        bank = [
            (image, (np.eye(3), np.array([-1.0, -1.0]), np.array([1.0, 1.0]), 0))
            for _ in range(64)
        ]
        windows = {
            "origins": np.arange(12).reshape(4, 3).astype(float),
            "frames": np.tile(np.eye(3), (4, 1, 1)),
            "weights": np.array([1.0, 0.5, 0.75, 0.25]),
            "first_needle_ids": np.array([100, 200, 300, 400], np.uint64),
        }
        selected = np.array([9, 0, 9, 63])
        indices = np.arange(4)
        original = planes(windows, bank, selected, indices, 0)
        vectors = np.zeros((64, 32, 32, 4, 3), np.float32)
        vectors[..., 0, 2] = 1
        captured = []

        def capture(name, data, surface):
            captured.append(data)
            return bind_mesh(name, data, surface)

        with patch("normal_component_portrait.bind_mesh", side_effect=capture):
            triangles, payload, images, groups = bind_views(
                windows,
                bank,
                vectors,
                np.array([image] * 64),
                np.tile(np.eye(3), (64, 1, 1)),
                selected,
                indices,
                0,
                "four",
                [0, 0, 1],
            )
        self.assertEqual((triangles, payload, images, groups), (8, 245760, 15, 3))
        np.testing.assert_array_equal(
            np.sort(np.concatenate([d["source_window_indices"] for d in captured])),
            indices,
        )
        for data in captured:
            members = data["source_window_indices"]
            np.testing.assert_array_equal(
                data["vertices"].reshape(-1, 4, 3),
                original["vertices"].reshape(-1, 4, 3)[members],
            )
            np.testing.assert_array_equal(
                data["first_needle_ids"], windows["first_needle_ids"][members]
            )
            np.testing.assert_array_equal(
                data["weights"], np.repeat(windows["weights"][members], 2)
            )
            np.testing.assert_allclose(
                data["uv"].reshape(-1, 4, 2),
                np.tile([[0, 0], [1, 0], [1, 1], [0, 1]], (len(members), 1, 1)),
                atol=1e-14,
            )
        self.assertEqual(
            len([o for o in bpy.context.scene.objects if o.type == "MESH"]), 3
        )
        bpy.ops.wm.read_factory_settings(use_empty=True)


if __name__ == "__main__":
    unittest.main()
