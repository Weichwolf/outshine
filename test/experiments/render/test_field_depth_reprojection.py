"""Independent rays and original-visibility counterexamples for cell reprojection."""

import unittest

import numpy as np
from field_composition import compose_points
from field_depth_reprojection import reproject


def perspective(eye=(0, 0, -2)):
    view = np.eye(4)
    view[:3, 3] = -np.asarray(eye)
    projection = np.array([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 0.1], [0, 0, 1, 0]])
    return projection @ view


def brute_cells(rgba, depth, capture, world, pixels, raster):
    """Cast a field-space ray against EVERY explicit cell plane, no grid walk."""
    inverse_world, inverse_capture = np.linalg.inv(world), np.linalg.inv(capture)
    result = np.zeros((len(pixels), 4), dtype=rgba.dtype)
    zs = np.zeros(len(pixels))
    for at, pixel in enumerate(pixels):
        xy = (pixel / raster * 2 - 1) * [1, -1]
        points = np.array([inverse_world @ [*xy, d, 1] for d in (0.2, 0.8)])
        points = points[:, :3] / points[:, 3:]
        origin, ray = points[0], points[1] - points[0]
        for y, x in np.argwhere(depth > 0):
            uv = np.array([[x, y], [x + 1, y], [x, y + 1]])
            ndc = (uv / depth.shape[::-1] * 2 - 1) * [1, -1]
            corners = (
                np.c_[ndc, np.full(3, depth[y, x]), np.ones(3)] @ inverse_capture.T
            )
            corners = corners[:, :3] / corners[:, 3:]
            normal = np.cross(corners[1] - corners[0], corners[2] - corners[0])
            denominator = normal @ ray
            if denominator == 0:
                continue
            position = origin + ray * (normal @ (corners[0] - origin) / denominator)
            c, w = capture @ [*position, 1], world @ [*position, 1]
            if c[3] <= 0 or w[3] <= 0:
                continue
            hit = (c[:2] / c[3] * [1, -1] + 1) * depth.shape[::-1] / 2
            if (
                (hit >= [x, y]).all()
                and (hit < [x + 1, y + 1]).all()
                and zs[at] < w[2] / w[3] <= 1
            ):
                zs[at], result[at] = w[2] / w[3], rgba[y, x]
    return result, zs


class ReprojectionTests(unittest.TestCase):
    def test_orthographic_capture_to_perspective_with_analytic_ground(self):
        capture = np.eye(4)
        capture[2, 2], capture[2, 3] = -0.1, 1
        depth = np.full((8, 8), 0.6, np.float32)
        rgba = np.zeros((8, 8, 4), np.float32)
        rgba[:] = [1, 0, 0, 1]
        yy, xx = np.mgrid[:32, :40]
        pixels = np.c_[xx.ravel(), yy.ravel()] + 0.3
        world = perspective()
        result = reproject(rgba, depth, capture, world, pixels, (40, 32))
        stored_plane_z = (1 - float(depth[0, 0])) / 0.1
        ndc = (pixels / [40, 32] * 2 - 1) * [1, -1]
        hit = (abs(ndc * (stored_plane_z + 2)) < 1).all(1)
        expected = np.zeros_like(result.rgba)
        expected[hit] = [1, 0, 0, 1]
        np.testing.assert_array_equal(result.rgba, expected)
        np.testing.assert_allclose(result.reverse_z[hit], 0.1 / (stored_plane_z + 2))
        self.assertTrue(result.evaluated.all())
        ground = np.broadcast_to(np.array([0, 1, 0, 1], np.float32), (32, 40, 4)).copy()
        combined = compose_points(result.rgba.reshape(32, 40, 4), ground)
        truth = ground.reshape(-1, 4).copy()
        truth[hit] = [1, 0, 0, 1]
        np.testing.assert_array_equal(combined.reshape(-1, 4), truth)

    def test_different_raster_preserves_constant_plane_and_coupled_colour(self):
        y, x = np.mgrid[:4, :4]
        rgba = np.stack(
            (x / 4, y / 4, np.zeros_like(x), np.ones_like(x)), axis=2
        ).astype(np.float32)
        depth = np.full((4, 4), 0.6, np.float32)
        yy, xx = np.mgrid[:8, :8]
        pixels = np.c_[xx.ravel(), yy.ravel()] + 0.5
        old = rgba.copy(), depth.copy()
        result = reproject(rgba, depth, np.eye(4), np.eye(4), pixels, (8, 8))
        np.testing.assert_array_equal(
            result.rgba.reshape(8, 8, 4), rgba.repeat(2, 0).repeat(2, 1)
        )
        np.testing.assert_array_equal(result.reverse_z, float(depth[0, 0]))
        self.assertTrue(result.evaluated.all())
        np.testing.assert_array_equal(rgba, old[0])
        np.testing.assert_array_equal(depth, old[1])

    def test_grid_walk_matches_all_explicit_cell_planes_at_new_perspective(self):
        rng = np.random.default_rng(1797)
        capture = np.eye(4)
        capture[2, 2], capture[2, 3] = -0.1, 1
        for _ in range(4):
            depth = rng.uniform(0.2, 0.8, (4, 5)).astype(np.float32)
            rgba = rng.uniform(0, 4, (4, 5, 4)).astype(np.float32)
            rgba[:, :, 3] = 1
            empty = rng.random(depth.shape) < 0.2
            rgba[empty], depth[empty] = 0, 0
            world = perspective(rng.uniform([-0.4, -0.4, -3], [0.4, 0.4, -1]))
            pixels = rng.uniform([0, 0], [20, 16], (96, 2))
            result = reproject(rgba, depth, capture, world, pixels, (20, 16))
            truth, truth_z = brute_cells(rgba, depth, capture, world, pixels, (20, 16))
            self.assertTrue(result.evaluated.all())
            np.testing.assert_array_equal(result.rgba, truth)
            np.testing.assert_allclose(
                result.reverse_z, truth_z, rtol=1e-12, atol=1e-15
            )

    def test_a_reprojected_miss_does_not_prove_an_original_miss(self):
        # Original front red square at z=2 fully hides a blue square at z=4.
        capture = np.eye(4)
        capture[2, 2], capture[2, 3] = -0.1, 1
        depth = np.zeros((8, 8), np.float32)
        depth[2:6, 2:6] = 0.8
        rgba = np.zeros((8, 8, 4), np.float32)
        rgba[depth > 0] = [1, 0, 0, 1]
        world = perspective((1, 0, 0))
        result = reproject(rgba, depth, capture, world, [[4, 5]], (10, 10))
        # New ray x=1-0.2*z misses the front (x=.6) and hits the blue back (x=.2).
        self.assertGreater(1 - 0.2 * 2, 0.5)
        self.assertLess(abs(1 - 0.2 * 4), 0.5)
        self.assertTrue(result.evaluated[0])
        self.assertEqual(result.reverse_z[0], 0)
        np.testing.assert_array_equal(result.rgba[0], [0, 0, 0, 0])

    def test_work_exhaustion_is_local_unknown_not_clear(self):
        depth = np.full((4, 4), 0.5, np.float32)
        rgba = np.ones((4, 4, 4), np.float32)
        result = reproject(
            rgba, depth, np.eye(4), np.eye(4), [[1, 1], [2, 2]], (4, 4), cells_most=1
        )
        self.assertFalse(result.evaluated.any())
        self.assertEqual(result.work["failed_samples"], 2)

    def test_empty_model_and_invalid_inputs(self):
        rgba, depth = np.zeros((2, 2, 4), np.float32), np.zeros((2, 2), np.float32)
        self.assertTrue(
            reproject(
                rgba, depth, np.eye(4), np.eye(4), [[0.5, 0.5]], (2, 2)
            ).evaluated.all()
        )
        for changes in ({"cells_most": 0}, {"samples_most": 1}):
            with self.assertRaises(ValueError):
                reproject(
                    rgba,
                    depth,
                    np.eye(4),
                    np.eye(4),
                    [[0.5, 0.5], [1.5, 1.5]],
                    (2, 2),
                    **changes,
                )
        rgba[0, 0, 3] = 0.5
        with self.assertRaises(ValueError):
            reproject(rgba, depth, np.eye(4), np.eye(4), [[0.5, 0.5]], (2, 2))
        rgba[:] = 0
        for capture, world in (
            (np.zeros((4, 4)), np.eye(4)),
            (np.eye(4), np.zeros((4, 4))),
        ):
            with self.assertRaises(ValueError):
                reproject(rgba, depth, capture, world, [[0.5, 0.5]], (2, 2))


if __name__ == "__main__":
    unittest.main()
