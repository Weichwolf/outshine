"""Pixel/world reprojection laws for the native material/TAA diagnostic."""

import unittest

import numpy as np
from material_temporal_camera import sequence


class MaterialCameraTests(unittest.TestCase):
    def test_static_camera_has_identical_queries_and_zero_velocity(self):
        queries, motion, _ = sequence(16, 60, False)
        np.testing.assert_array_equal(motion, 0)
        np.testing.assert_array_equal(
            queries, np.broadcast_to(queries[0], queries.shape)
        )

    def test_production_ndc_motion_reprojects_to_the_same_world_point(self):
        size = 16
        queries, motion, poses = sequence(size, 60, True)
        x, y = np.meshgrid(np.arange(size), np.arange(size))
        pixels = np.column_stack((x.ravel(), y.ravel()))
        for frame in range(1, len(queries)):
            past = pixels + motion[frame] * [-size / 2, size / 2]
            previous = poses[frame - 1]
            world = previous[:, 2] + (past - (size - 1) / 2) @ previous[:, :2].T
            np.testing.assert_allclose(world, queries[frame, :, :2], rtol=0, atol=1e-8)

    def test_all_pixel_footprints_remain_in_the_resident_tile(self):
        queries, _, _ = sequence(32, 60, True)
        half = (np.abs(queries[:, :, 2:4]) + np.abs(queries[:, :, 4:6])) / 2
        self.assertTrue(np.all(queries[:, :, :2] - half >= 0))
        self.assertTrue(np.all(queries[:, :, :2] + half <= 1))

    def test_invalid_dimensions_fail_before_generation(self):
        for size, frames in ((0, 60), (129, 60), (32, 0), (32, 129), (True, 60)):
            with self.subTest(size=size, frames=frames), self.assertRaises(ValueError):
                sequence(size, frames, True)
        with self.assertRaises(ValueError):
            sequence(2, 128, True)


if __name__ == "__main__":
    unittest.main()
