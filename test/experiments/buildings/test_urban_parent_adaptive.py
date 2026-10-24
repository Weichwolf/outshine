"""Sampling geometry, subpixel support and nonuniform moment interpolation."""

import unittest

import numpy as np
from compare_urban_parents import camera
from test_urban_parent_phase import homogeneous_parent
from triangle_image_reference import image_rays
from urban_parent_adaptive import adaptive_axes, rays_at
from urban_parent_transfer import ShadowTransfer


class ParentAdaptiveTest(unittest.TestCase):
    def test_dense_grid_does_not_duplicate_refined_support_samples(self):
        parent = homogeneous_parent()
        view = camera(512, 5000, 0)
        box = np.array([[590, 340], [690, 380]])
        profile = {
            "kind": "sun",
            "direction": [0.45, -0.5, 0.7],
            "intensity": 3,
            "ambient": 0.1,
        }
        axes, work = adaptive_axes(parent, view, box, profile, [1, 1])
        self.assertGreater(work["support_pixels"], 0)
        np.testing.assert_array_equal(axes[0], (np.arange(100) + 0.5) / 100)
        np.testing.assert_array_equal(axes[1], (np.arange(40) + 0.5) / 40)

    def test_projection_matches_existing_camera_for_uniform_axes(self):
        view = camera(512, 240000, 8000)
        box = np.array([[638, 358], [642, 362]])
        axes = ((np.arange(7) + 0.5) / 7, (np.arange(3) + 0.5) / 3)
        a, b = rays_at(view, box, axes)
        c, d = image_rays(view, box, 7, 3, 1)
        d /= np.linalg.norm(d, axis=1)[:, None]
        np.testing.assert_array_equal(a, c)
        np.testing.assert_allclose(b, d, rtol=0, atol=3e-16)

    def test_nonuniform_nodes_reconstruct_affine_conditional_visibility(self):
        axes = (np.array([0.1, 0.2, 0.9]), np.array([0.1, 0.8, 0.9]))
        x, y = np.meshgrid(*axes)
        moments = np.zeros((3, 3, 2, 2))
        moments[:, :, 0, 0] = 1
        moments[:, :, 0, 1] = 0.2 + 0.3 * x + 0.2 * y
        lookup = ShadowTransfer(moments, (4, 3), 4, axes=axes)
        lookup.prepare(0, 12)
        values = lookup(np.arange(12) * 16, np.zeros(12, int))
        expected = (
            0.2
            + 0.3 * (np.arange(12) % 4 + 0.5) / 4
            + 0.2 * (np.arange(12) // 4 + 0.5) / 3
        )
        np.testing.assert_allclose(values, expected, rtol=0, atol=2e-15)

    def test_subpixel_parent_gets_projected_support_anchors_without_colour_fit(self):
        parent = homogeneous_parent()
        view = camera(512, 240000, 0, 10000)
        box = np.array([[636, 357], [644, 363]])
        profile = {
            "kind": "sun",
            "direction": [0.45, -0.5, 0.7],
            "intensity": 3,
            "ambient": 0.1,
        }
        axes, work = adaptive_axes(parent, view, box, profile, [4, 4])
        # A ray through the projected parent centre intersects the small mass even
        # when integer pixel-centre probes miss it.
        origins, directions = rays_at(view, box, axes)
        z = parent.heights @ parent.probabilities
        points = (
            origins + ((z - origins[:, 2]) / directions[:, 2])[:, None] * directions
        )
        np.testing.assert_allclose(
            np.min(np.linalg.norm(points[:, :2] - 256, axis=1)), 0, rtol=0, atol=1e-8
        )
        changed, _ = adaptive_axes(
            parent, view, box, profile | {"intensity": 500, "ambient": 100}, [4, 4]
        )
        for a, b in zip(axes, changed, strict=True):
            self.assertTrue(np.all(np.diff(a) > 0))
            self.assertTrue(np.all((a > 0) & (a < 1)))
            np.testing.assert_array_equal(a, b)
        self.assertEqual(work["support_ray_plane_tests"], 8 * 6 * 5)


if __name__ == "__main__":
    unittest.main()
