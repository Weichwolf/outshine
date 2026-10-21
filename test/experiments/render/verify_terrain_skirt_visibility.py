"""Independent source-triangle checks for height-independent skirt planes."""

import unittest

import numpy as np
from terrain_occluders import skirts
from terrain_skirt_visibility import visible_sides


class SkirtPlanes(unittest.TestCase):
    def setUp(self):
        self.corners = np.array(
            [[0.0, 1.0, 0.0], [1.0, 1.0, 0.0], [1.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
        )
        self.up = np.array([0.0, 0.0, 1.0])

    def test_eye_inside_retains_no_outward_side_and_corner_retains_two(self):
        np.testing.assert_array_equal(
            visible_sides(self.corners, self.up, [0.5, 0.5, 20]), False
        )
        np.testing.assert_array_equal(
            visible_sides(self.corners, self.up, [2, 2, -3]), [True, True, False, False]
        )

    def test_edge_plane_and_uncertainty_band_fail_open(self):
        for x in (1.0, 1.0 - 1e-8):
            self.assertTrue(visible_sides(self.corners, self.up, [x, 0.5, 0], 1e-7)[1])
        self.assertFalse(visible_sides(self.corners, self.up, [0.99, 0.5, 0], 1e-7)[1])

    def test_missing_or_degenerate_planes_fail_open(self):
        np.testing.assert_array_equal(
            visible_sides(self.corners, self.up, [np.nan, 0, 0]), True
        )
        np.testing.assert_array_equal(
            visible_sides(self.corners, np.zeros(3), [2, 2, 0]), True
        )
        corners = self.corners.copy()
        corners[1] = corners[0]
        self.assertTrue(visible_sides(corners, self.up, [2, 2, 0])[0])

    def test_finite_extreme_or_underflowed_arithmetic_fails_open(self):
        for scale in (1e200, 1e-200):
            np.testing.assert_array_equal(
                visible_sides(self.corners * scale, self.up * scale, [2, 2, 0]), True
            )

    def test_invalid_shape_or_uncertainty_is_rejected(self):
        for guard in (-1.0, np.inf, np.nan):
            with self.assertRaises(ValueError):
                visible_sides(self.corners, self.up, [2, 2, 0], guard)
        with self.assertRaises(ValueError):
            visible_sides(self.corners[:3], self.up, [2, 2, 0])

    def test_random_heights_curvature_affine_transforms_and_reflections(self):
        rng = np.random.default_rng(12)
        for _ in range(1000):
            f = np.r_[0.0, np.sort(rng.uniform(0.001, 0.999, 31)), 1.0]
            x, y = np.meshgrid(f, f)
            height = rng.uniform(-5000, 5000, (33, 33)) - 200 * (x * x + y * y)
            local = np.stack((x, 1 - y, height), axis=-1).reshape(-1, 3)
            points, faces, _ = skirts(local, rng.uniform(0.01, 1000))
            faces = faces.reshape(4, 64, 3)[[0, 3, 1, 2]].reshape(-1, 3)
            rotation, _ = np.linalg.qr(rng.normal(size=(3, 3)))
            stretch = np.triu(rng.normal(size=(3, 3)))
            np.fill_diagonal(stretch, rng.uniform(0.25, 4, 3) * rng.choice([-1, 1], 3))
            transform = stretch @ rotation
            offset, eye = rng.normal(size=(2, 3)) * 10
            vertices = points @ transform + offset
            corners = self.corners @ transform + offset
            up = self.up @ transform
            keep = visible_sides(corners, up, eye, 1e-8)
            triangles = vertices[faces]
            normals = np.cross(
                triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
            )
            scores = np.einsum("ij,ij->i", normals, eye - triangles[:, 0])
            self.assertFalse(np.any((scores > 0) & ~np.repeat(keep, 64)))


if __name__ == "__main__":
    unittest.main()
