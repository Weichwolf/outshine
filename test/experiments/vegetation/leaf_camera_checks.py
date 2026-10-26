"""Check that the finite diagnostic camera never discards source geometry."""

import unittest

import numpy as np
from grass_light_preview import camera_depth_receipt, mesh_object, setup, sheet
from leaf_stack_cases import VIEW, full_geometry


class LeafCameraChecks(unittest.TestCase):
    def setUp(self):
        self.scene = setup(VIEW)
        self.vertices, faces = full_geometry()
        mesh_object("whole fixture", self.vertices, faces, sheet("opaque", (0.2,) * 3))

    def test_every_bound_is_inside_the_finite_camera(self):
        receipt = camera_depth_receipt(self.scene)
        expected = 4 - self.vertices @ VIEW
        np.testing.assert_allclose(
            receipt["all_mesh_bounds_depth_m"],
            [expected.min(), expected.max()],
            atol=1e-6,
        )
        self.assertEqual(receipt["mesh_vertices"], len(self.vertices))
        self.assertEqual(receipt["bound_corners_checked"], 8)

    def test_clipping_geometry_is_rejected_at_either_plane(self):
        self.scene.camera.data.clip_end = 3
        with self.assertRaises(ValueError):
            camera_depth_receipt(self.scene)
        self.scene.camera.data.clip_end = 10
        self.scene.camera.data.clip_start = 5
        with self.assertRaises(ValueError):
            camera_depth_receipt(self.scene)


if __name__ == "__main__":
    unittest.main()
