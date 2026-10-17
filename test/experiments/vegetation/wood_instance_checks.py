"""Actual Blender instance transforms and analytic-normal source controls."""

import unittest
from unittest.mock import patch

import bpy
import numpy as np
from blender_preview import material
from fern_material import NORMAL_ATTRIBUTE
from scipy.spatial.transform import Rotation
from window_portrait_source import wood
from window_wood_instances import bind, groups


def control():
    graph = {"nodes": [{}]}
    for order in range(4):
        graph["nodes"].append(
            {
                "shape": {
                    "length": 2 / (order + 1),
                    "bend": 0.3,
                    "radius": {"table": [0.02, 0.015, 0.01, 0.005], "kind": "custom"},
                }
            }
        )
    axes = {
        "path_offsets": np.r_[0, np.cumsum(np.repeat(np.arange(2, 6), 2))],
        "origins": np.arange(24).reshape(8, 3) * 0.5,
        "rotations": Rotation.from_euler(
            "xyz", np.arange(24).reshape(8, 3) * 0.17
        ).as_matrix(),
        "uniform_scales": np.linspace(0.4, 1.2, 8),
    }
    return graph, axes


class WoodInstanceTests(unittest.TestCase):
    def test_same_templates_axis_ids_and_scalar_geometry(self):
        graph, axes = control()
        expanded = wood(graph, axes)
        vertex, face = 0, 0
        selected = []
        for _, mesh, ids, origins, frames, scales in groups(graph, axes):
            selected.extend(ids)
            for i, origin, frame, scale in zip(
                ids, origins, frames, scales, strict=True
            ):
                points = (mesh["vertices"] @ frame.T * scale + origin).astype(
                    np.float32
                )
                nv, nf = len(points), len(mesh["faces"])
                np.testing.assert_array_equal(
                    expanded["vertices"][vertex : vertex + nv], points
                )
                np.testing.assert_array_equal(
                    expanded["faces"][face : face + nf], mesh["faces"] + vertex
                )
                self.assertEqual(scale[0], axes["uniform_scales"][i])
                vertex += nv
                face += nf
        self.assertEqual(selected, list(range(8)))
        self.assertEqual(face, len(expanded["faces"]))

    def test_real_depsgraph_transforms_corner_normals_and_all_instances(self):
        graph, axes = control()
        bpy.ops.wm.read_factory_settings(use_empty=True)
        report = bind(graph, axes, material("control wood", (0.14, 0.105, 0.037), 0.8))
        bpy.context.view_layer.update()
        expected = {
            f"all cached wood order {order}": group
            for group in groups(graph, axes)
            for order in (group[0],)
        }
        seen = []
        for item in bpy.context.evaluated_depsgraph_get().object_instances:
            if not item.is_instance:
                continue
            _, mesh, ids, *_ = expected[item.parent.original.name]
            index = int(ids[item.persistent_id[1]])
            matrix = np.array(item.matrix_world)
            actual = (
                mesh["vertices"].astype(np.float32) @ matrix[:3, :3].T + matrix[:3, 3]
            )
            target = (
                mesh["vertices"]
                @ axes["rotations"][index].T
                * axes["uniform_scales"][index]
                + axes["origins"][index]
            )
            np.testing.assert_allclose(actual, target, rtol=0, atol=2e-6)
            normals = item.object.data.attributes[NORMAL_ATTRIBUTE]
            values = np.empty(len(mesh["normals"]) * 3, dtype=np.float32)
            normals.data.foreach_get("vector", values)
            actual = values.reshape(-1, 3) @ np.linalg.inv(matrix[:3, :3])
            actual /= np.linalg.norm(actual, axis=1)[:, None]
            target = mesh["normals"] @ axes["rotations"][index].T
            np.testing.assert_allclose(actual, target, rtol=0, atol=5e-7)
            seen.append(index)
        self.assertEqual(sorted(seen), list(range(8)))
        self.assertEqual(len(bpy.context.scene.objects), 8)
        self.assertEqual(report["triangles"], len(wood(graph, axes)["faces"]))

    def test_budget_rejects_before_templates_or_blender_objects(self):
        with (
            patch(
                "window_wood_instances.wood_templates",
                side_effect=AssertionError("late bound"),
            ),
            self.assertRaises(ValueError),
        ):
            groups({}, {"path_offsets": np.arange(60002) * 2})


if __name__ == "__main__":
    unittest.main()
