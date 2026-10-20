import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from native_backface_cost import (
    STATE_INPUTS,
    cell_rays,
    compare,
    cone_rejects,
    face_rejects,
    load_batch_states,
    unit_normals,
)
from native_visibility_data import digest
from test_native_visibility_cost import scene


class NativeBackfaceCost(unittest.TestCase):
    def test_cone_keeps_front_and_degenerate_geometry(self):
        points = np.array([[[-1, -1, 0], [1, -1, 0], [0, 1, 0]]], dtype=float)
        normals = unit_normals(points)
        self.assertFalse(cone_rejects(points, normals, np.array([0, 0, 3]), 1e-6))
        self.assertTrue(cone_rejects(points, normals, np.array([0, 0, -3]), 1e-6))
        points[:] = 0
        self.assertFalse(
            cone_rejects(points, unit_normals(points), np.array([0, 0, -3]), 1e-6)
        )

    def test_truncated_face_support_retains_front_facing_samples(self):
        normals = np.eye(3)
        offsets = np.array([-0.5, 0.5, -0.25])
        rejected = 0
        for grid in (1, 2):
            for axis in range(3):
                for sign in (-1, 1):
                    for u in (-0.75, 0.75):
                        for v in (-0.75, 0.75):
                            direction = np.zeros(3)
                            direction[axis] = sign
                            direction[(axis + 1) % 3] = u
                            direction[(axis + 2) % 3] = v
                            rays, _ = cell_rays(direction, grid)
                            dropped = face_rejects(normals, offsets, rays, 2, 1e-6)
                            rejected += int(dropped.sum())
                            for first in rays:
                                for second in rays:
                                    for mix in (0, 0.25, 0.5, 1):
                                        for distance in (2, 3, 1000):
                                            eye = (
                                                (1 - mix) * first + mix * second
                                            ) * distance
                                            self.assertTrue(
                                                (
                                                    (normals @ eye + offsets)[dropped]
                                                    < 0
                                                ).all()
                                            )
        self.assertGreater(rejected, 0)

    def test_material_and_motion_flags_exclude_unqualified_geometry(self):
        snapshot = scene()
        snapshot.clip = np.array(
            [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 0.1], [0, 0, -1, 0]]
        )
        spheres = np.array([[0, 0, 0.2, 0.4], [0, 0, 0.8, 0.4]])
        for states in (
            np.array([[0, 0, 1], [0, 1, 0]]),
            np.array([[0, 1, 1], [0, 1, 1]]),
        ):
            result = compare(snapshot, states, spheres, limits=(128,))
            expected = 4 if states[:, 1:].all() else 0
            self.assertEqual(result["eligible_rigid_triangles"], expected)
            self.assertEqual(result["cones"][0]["rejected_triangles"], expected)
            self.assertEqual(
                result["cones"][0]["all_backface_group_triangles"], expected
            )
            self.assertEqual(result["false_rejections_vs_double_margin"], 0)

        snapshot.shift[2] = -0.2 + 1e-9
        result = compare(
            snapshot, np.array([[0, 1, 1], [0, 1, 1]]), spheres, limits=(128,)
        )
        self.assertEqual(result["eligible_geometric_backfaces"], 4)
        self.assertEqual(result["cones"][0]["rejected_triangles"], 2)
        self.assertEqual(result["cones"][0]["all_backface_group_triangles"], 4)

    def test_state_loader_rejects_gaps_and_wrong_input_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name in STATE_INPUTS:
                (root / name).write_bytes(b"fixed input")
            data = {
                "schema": 1,
                "input_sha256": {name: digest(root / name) for name in STATE_INPUTS},
                "rows": 2,
                "runs": [[0, 1, 0, 1, 0], [1, 2, 0, 1, 1]],
            }
            path = root / "states.json"
            path.write_text(json.dumps(data))
            np.testing.assert_array_equal(
                load_batch_states(root, path), [[0, 1, 0], [0, 1, 1]]
            )
            data["runs"][1][0] = 2
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                load_batch_states(root, path)
            data["runs"][1][0] = 1
            path.write_text(json.dumps(data))
            (root / "vertices.bin").write_bytes(b"other geometry")
            with self.assertRaises(ValueError):
                load_batch_states(root, path)


if __name__ == "__main__":
    unittest.main()
