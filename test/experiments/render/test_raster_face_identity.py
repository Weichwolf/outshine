"""Raster ownership agrees with independent triangle rays and survives batches."""

import unittest

import numpy as np
from capture_raster_reference import input_keys
from current_visibility import project, raster
from triangle_image_reference import image_rays, intersect_source, prepare_source


class RasterFaceIdentityTests(unittest.TestCase):
    def test_comparison_keys_detect_input_changes_and_do_not_invent_face_identity(self):
        def source():
            return {
                "positions": np.zeros((3, 3), np.float32),
                "uv": np.zeros((3, 2), np.float32),
                "normal": np.zeros(3, np.uint32),
                "colour": np.ones((3, 4), np.float32),
                "metres": np.zeros((3, 2), np.float32),
                "indices": np.array([[0, 1, 2]]),
                "binding": np.array([[1, 22, 0, 4, 0, 0]]),
            }

        meshes = {name: source() for name in ("Fine", "Full", "Captured")}
        meshes["Fine"]["binding"][0, 2:4] = -1
        classes, _ = input_keys(meshes)
        np.testing.assert_array_equal(classes["Fine"], classes["Full"])
        np.testing.assert_array_equal(meshes["Fine"]["binding"][0, 2:4], [-1, -1])
        meshes["Captured"]["normal"][0] ^= 1
        classes, _ = input_keys(meshes)
        self.assertNotEqual(classes["Captured"][0], classes["Full"][0])

    def test_occlusion_backfaces_and_empty_pixels_against_independent_rays(self):
        points = np.array(
            [
                [-2, -2, 0],
                [2, -2, 0],
                [0, 2, 0],
                [-0.5, -0.5, 2],
                [0.5, -0.5, 2],
                [0, 0.5, 2],
            ],
            float,
        )
        indices = np.array([[0, 1, 2], [3, 4, 5], [5, 4, 3]])
        view = (np.array([0, 0, 4.0]), np.diag([1.0, 1.0, -1.0]), 8.0, 16, 16)
        projected, z = project(points, view)
        depth, owners = raster(projected, z, indices, 16, 16, with_faces=True)
        np.testing.assert_array_equal(depth, raster(projected, z, indices, 16, 16))
        origins, directions = image_rays(
            view, np.array([[0, 0], [16, 16]]), 16, 16, supersample=1
        )
        source = {"positions": points, "indices": indices}
        distances, faces = intersect_source(
            source, origins, directions, prepared=prepare_source(source)
        )
        np.testing.assert_array_equal(owners.ravel(), faces)
        expected = np.divide(
            0.1, distances, out=np.zeros_like(distances), where=np.isfinite(distances)
        )
        np.testing.assert_allclose(depth.ravel(), expected, atol=1e-16, rtol=1e-14)

    def test_equal_computed_depth_keeps_first_triangle_and_empty_input_stays_empty(
        self,
    ):
        projected = np.array([[2, 2, 0.1], [8, 2, 0.1], [2, 8, 0.1]])
        indices = np.array([[0, 2, 1], [0, 2, 1]])
        depth, owners = raster(projected, np.ones(3), indices, 10, 10, with_faces=True)
        self.assertTrue((owners[depth > 0] == 0).all())
        self.assertTrue((owners[depth == 0] == -1).all())
        empty, ids = raster(projected, np.ones(3), indices[:0], 10, 10, with_faces=True)
        self.assertFalse(empty.any())
        self.assertTrue((ids == -1).all())


if __name__ == "__main__":
    unittest.main()
