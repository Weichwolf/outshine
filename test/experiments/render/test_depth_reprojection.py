"""Independent camera transforms and a real thin-triangle point-splat failure."""

import unittest
from fractions import Fraction

import numpy as np
from current_visibility import project, raster
from depth_reprojection import depth_errors, forward_points, transfer_matrix
from supported_occluders import (
    raster_faces,
    raster_patches,
    selected_faces,
    tile_patches,
)


def clip(focal=8, width=16, height=16, near=0.125):
    return np.array(
        [
            [2 * focal / width, 0, 0, 0],
            [0, 2 * focal / height, 0, 0],
            [0, 0, 0, near],
            [0, 0, 1, 0],
        ],
        dtype=float,
    )


def view(eye=(0, 0, 0), angle=0, width=32, height=24):
    c, s = np.cos(angle), np.sin(angle)
    basis = np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])
    return np.array(eye, float), basis, 24.0, width, height


class DepthTransfer(unittest.TestCase):
    def test_identity_preserves_points_and_clear_exactly(self):
        depth = np.array([[0, 0.25, 0.5], [0.75, 0.125, 0]])
        result, work = forward_points(depth, transfer_matrix(clip(), clip(), [0, 0, 0]))
        np.testing.assert_array_equal(depth, result)
        self.assertEqual(work["point_collisions"], 0)
        self.assertEqual(work["source_points"], 4)

    def test_camera_translation_matches_analytic_plane_shift(self):
        old = np.full((16, 16), 0.125 / 4)
        result, _ = forward_points(old, transfer_matrix(clip(), clip(), [-1, 0, 0]))
        expected = old.copy()
        expected[:, -2:] = 0
        np.testing.assert_array_equal(result, expected)
        matrix = transfer_matrix(
            clip(), clip(), np.array([1e8 - 1, 2e8, 0]) - [1e8, 2e8, 0]
        )
        np.testing.assert_array_equal(forward_points(old, matrix)[0], expected)

    def test_homogeneous_transfer_matches_independent_world_projection(self):
        previous, current = clip(13), clip(11)
        points = np.array([[1, -2, 7, 1], [-3, 4, 9, 1], [0.4, -0.3, 2, 1]])
        shift = np.array([0.3, 0.1, -0.2])
        old = points @ previous.T
        expected_points = points.copy()
        expected_points[:, :3] += shift
        expected = expected_points @ current.T
        actual = old @ transfer_matrix(previous, current, shift).T
        np.testing.assert_allclose(actual, expected, atol=1e-14, rtol=0)

    def test_behind_camera_and_empty_history_remain_clear(self):
        result, work = forward_points(
            np.full((8, 8), 0.125 / 4), transfer_matrix(clip(), clip(), [0, 0, -8])
        )
        self.assertFalse(result.any())
        self.assertEqual(work["projected_points"], 0)
        self.assertEqual(
            forward_points(np.zeros((3, 5)), np.eye(4))[1]["source_points"], 0
        )

    def test_batch_sizes_preserve_candidate_words(self):
        depth = np.linspace(0, 1, 221).reshape(13, 17)
        transform = transfer_matrix(clip(), clip(), [0.1, -0.1, 0.2])
        a = forward_points(depth, transform, 1)[0]
        b = forward_points(depth, transform, 65536)[0]
        np.testing.assert_allclose(a, b, atol=2e-16, rtol=0)

    def test_invalid_depth_and_transforms_fail(self):
        for value in [
            np.array([[np.nan]]),
            np.array([[np.inf]]),
            np.array([[-1.0]]),
            np.array([[1.01]]),
            np.empty((0, 0)),
            np.zeros(2),
        ]:
            with self.assertRaises(ValueError):
                forward_points(value, np.eye(4))
        with self.assertRaises(ValueError):
            transfer_matrix(np.zeros((4, 4)), clip(), [0, 0, 0])
        with self.assertRaises(ValueError):
            forward_points(np.zeros((2, 2)), np.eye(4), 0)

    def test_depth_point_does_not_cover_its_target_pixel(self):
        old_view = np.zeros(3), np.eye(3), 8, 8, 8
        new_view = np.array([0.1, 0, 0]), np.eye(3), 8, 8, 8
        p = np.array([[-0.01, -0.01, 0], [0.01, -0.01, 0], [0, 0.01, 0]]) + [
            0.25,
            -0.25,
            4,
        ]
        triangle = np.array([[0, 1, 2]])
        a, z = project(p, old_view)
        old = raster(a, z, triangle, 8, 8)
        a, z = project(p, new_view)
        current = raster(a, z, triangle, 8, 8)
        self.assertEqual(np.count_nonzero(old), 1)
        self.assertEqual(np.count_nonzero(current), 0)
        matrix = transfer_matrix(clip(8, 8, 8, 0.1), clip(8, 8, 8, 0.1), [-0.1, 0, 0])
        point, _ = forward_points(old, matrix)
        self.assertEqual(
            depth_errors(point, current)["candidate_over_current_clear"], 1
        )
        quads, owners, _ = tile_patches(p, triangle, [0], old_view, block=1)
        self.assertEqual(len(quads), 0)
        self.assertEqual(len(owners), 0)


class GeometricSupport(unittest.TestCase):
    def setUp(self):
        self.points = np.array([[-6, -5, 6], [6, -5, 6], [6, 5, 6], [-6, 5, 6]], float)
        self.faces = np.array([[0, 1, 2], [0, 2, 3]])
        self.quads, self.owners, self.work = tile_patches(
            self.points, self.faces, [0, 1], view(), block=2
        )

    def test_support_is_inside_original_triangle_planes_and_edges(self):
        self.assertGreater(len(self.quads), 10)
        for quad, owner in zip(self.quads, self.owners, strict=True):
            a, b, c = self.points[self.faces[owner]]
            np.testing.assert_array_equal(quad[:, 2], np.full(4, 6.0))
            for point in quad:
                for first, last in [(a, b), (b, c), (c, a)]:
                    edge = [Fraction(float(v)) for v in last - first]
                    delta = [
                        Fraction(float(q)) - Fraction(float(v))
                        for q, v in zip(point, first)
                    ]
                    self.assertGreater(edge[0] * delta[1] - edge[1] * delta[0], 0)

    def test_rotation_translation_disocclusion_never_invent_covered_centres(self):
        for eye, angle in [
            ((0, 0, 0), 0),
            ((1, 0, 0), 0.2),
            ((-2, 0.5, 0), -0.3),
            ((0, 0, 5.95), 0),
        ]:
            current = view(eye, angle)
            full, _ = raster_faces(self.points, self.faces, np.arange(2), current)
            candidate, _ = raster_patches(
                self.quads, self.owners, current, [True, True]
            )
            self.assertFalse(((candidate > 0) & (full == 0)).any())
            self.assertLessEqual(float((candidate - full).max()), 1e-15)

    def test_changed_dynamic_or_unknown_sources_release_all_support(self):
        result, work = raster_patches(self.quads, self.owners, view(), [False, False])
        self.assertFalse(result.any())
        self.assertEqual(work["input_triangles"], 0)
        some, work = raster_patches(self.quads, self.owners, view(), [False, True])
        self.assertGreater(np.count_nonzero(some), 0)
        self.assertEqual(work["input_triangles"], int((self.owners == 1).sum()) * 2)

    def test_bounded_support_and_invalid_owner_contract(self):
        quads, owners, work = tile_patches(
            self.points, self.faces, [0, 1], view(), block=2, max_tiles=1
        )
        self.assertEqual(len(quads), 1)
        self.assertTrue(work["limit_reached"])
        with self.assertRaises(ValueError):
            raster_patches(quads, np.array([-1]), view(), [True, True])
        with self.assertRaises(ValueError):
            selected_faces(self.points, self.faces, view(), [True, True], -1)
        with self.assertRaises(ValueError):
            selected_faces(self.points, self.faces, view(), [True], 1)

    def test_unknown_near_plane_support_remains_empty(self):
        points = self.points.copy()
        points[0, 2] = 0.05
        quads, _, _ = tile_patches(points, self.faces, [0, 1], view(), block=1)
        self.assertEqual(len(quads), 0)


if __name__ == "__main__":
    unittest.main()
