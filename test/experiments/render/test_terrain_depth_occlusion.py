"""Independent projection, coverage and fail-open controls for native depth replay."""

import unittest

import numpy as np
from terrain_depth_occlusion import depth_pyramid, geometric_levels, occluded, project


def view(width=16, height=16):
    data = np.zeros(48, dtype="<f4")
    data[:16] = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0.05, 0]
    data[20:24] = [0, 0, width, height]
    data.view("<u4")[36:40] = [1, width, height, 256]
    data[44] = 0.5
    return data.tobytes()


def bounds():
    return np.array(
        [[[-0.1, -0.1, 3.9, 1], [0.1, 0.1, 4.1, 0], [0, 0.001, 0.002, 0.004]]]
    )


class TerrainDepthOcclusion(unittest.TestCase):
    def test_clear_depth_never_occludes(self):
        self.assertFalse(occluded(project(bounds(), view()), np.zeros((16, 16)))[0])

    def test_covering_nearer_depth_occludes(self):
        self.assertTrue(occluded(project(bounds(), view()), np.ones((16, 16)))[0])

    def test_hole_inside_the_rectangle_preserves_visibility(self):
        depth = np.ones((16, 16))
        depth[8, 8] = 0
        self.assertFalse(occluded(project(bounds(), view()), depth)[0])
        depth[8, 8], depth[0, 0] = 1, 0
        self.assertTrue(occluded(project(bounds(), view()), depth)[0])

    def test_equal_depth_and_existing_bias_preserve_visibility(self):
        self.assertFalse(
            occluded(project(bounds(), view()), np.full((16, 16), 0.05 / 4))[0]
        )

    def test_unsafe_and_unknown_inputs_remain_visible(self):
        for kind in ["unknown", "nonfinite", "behind"]:
            source = bounds()
            if kind == "unknown":
                source[0, 0, 3] = 0
            elif kind == "nonfinite":
                source[0, 0, 0] = np.nan
            else:
                source[0, :2, 2] = [-4.1, -3.9]
            projection = project(source, view())
            self.assertTrue(projection.unknown[0], kind)
            self.assertFalse(occluded(projection, np.ones((16, 16)))[0], kind)

    def test_screen_rejection_and_vertical_half_scale(self):
        source = bounds()
        source[0, :2, :3] = [0, 3, 4]
        projection = project(source, view())
        self.assertTrue(projection.eligible[0])
        self.assertAlmostEqual(projection.lower[0, 1], 1 / 16, places=4)
        self.assertAlmostEqual(projection.upper[0, 1], 3 / 16, places=4)
        source[0, :2, 0] = 100
        projection = project(source, view())
        self.assertFalse(projection.eligible[0])
        self.assertFalse(projection.unknown[0])

    def test_large_rectangles_cannot_claim_occlusion(self):
        source = bounds()
        source[0, 0, :2], source[0, 1, :2] = [-10, -10], [10, 10]
        self.assertFalse(
            occluded(project(source, view(128, 128)), np.ones((128, 128)))[0]
        )

    def test_odd_pyramid_edges_repeat_the_last_source_pixel(self):
        depth = np.arange(1, 10, dtype=np.float32).reshape(3, 3) / 10
        levels = depth_pyramid(depth)
        self.assertEqual(levels[0].shape, (2, 2))
        self.assertEqual(levels[0][1, 1], depth[2, 2])
        self.assertEqual(levels[-1][0, 0], depth[0, 0])

    def test_finite_errors_select_coarse_and_unknown_errors_keep_fine(self):
        source = bounds()
        self.assertEqual(
            geometric_levels(source, view(), project(source, view()))[0], 3
        )
        source[0, 2, 1:] = np.inf
        self.assertEqual(
            geometric_levels(source, view(), project(source, view()))[0], 0
        )

    def test_inputs_are_immutable_and_invalid_depth_is_rejected(self):
        source = bounds()
        original = source.copy()
        depth = np.ones((16, 16))
        occluded(project(source, view()), depth)
        np.testing.assert_array_equal(source, original)
        np.testing.assert_array_equal(depth, 1)
        for invalid in [np.empty((0, 2)), np.array([[np.nan]]), np.array([[1.1]])]:
            with self.assertRaises(ValueError):
                depth_pyramid(invalid)
        with self.assertRaises(ValueError):
            project(source, b"short")


if __name__ == "__main__":
    unittest.main()
