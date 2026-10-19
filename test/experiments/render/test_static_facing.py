"""Exact independent determinants check quantized culling, seams and fallbacks."""

import itertools
import struct
import unittest
from fractions import Fraction as F

import numpy as np
from facing_images import clip_near, render
from facing_intervals import length, point
from static_facing import bake, exact_facing, query

ENABLED = {
    "static": True,
    "one_sided": True,
    "trusted_winding": True,
    "model": np.eye(4),
}


def determinant(triangle, camera):
    p = [[F(float(x)) for x in row] for row in triangle]
    a, b, c = (
        [p[1][j] - p[0][j] for j in range(3)],
        [p[2][j] - p[0][j] for j in range(3)],
        [F(float(camera[j])) - p[0][j] for j in range(3)],
    )
    return sum(
        a[j] * (b[(j + 1) % 3] * c[(j + 2) % 3] - b[(j + 2) % 3] * c[(j + 1) % 3])
        for j in range(3)
    )


def retained(mask, count):
    return np.array([bool(mask & (1 << i)) for i in range(count)])


class StaticFacing(unittest.TestCase):
    def setUp(self):
        self.triangles = np.tile([[-1.0, -1, 0], [1, -1, 0], [0, 1, 0]], (128, 1, 1))
        self.record = bake(self.triangles, [0, 0, 0, 2])

    def assert_safe(self, triangles, record, camera):
        reference = np.array([determinant(t, camera) >= 0 for t in triangles])
        np.testing.assert_array_equal(exact_facing(triangles, camera) >= 0, reference)
        cone, mask = query(record, camera, **ENABLED)
        self.assertFalse((reference & ~retained(mask, len(triangles))).any())
        self.assertTrue(cone or not reference.any())
        return cone, mask

    def test_128_bit_masks_and_eight_byte_cone_are_effective(self):
        self.assertEqual(len(self.record["payload"]), 8)
        self.assertEqual(self.record["reason"], "cone")
        self.assertEqual(
            self.assert_safe(self.triangles, self.record, [0, 0, 100]),
            (True, (1 << 128) - 1),
        )
        self.assertEqual(
            self.assert_safe(self.triangles, self.record, [0, 0, -100]), (False, 0)
        )

    def test_unbounded_faces_retain_far_oblique_visible_triangle(self):
        triangle = np.array([[[0.0, 8, 0], [0, 8, 1], [1, 9 - 2**-10, 0]]])
        camera = [2**20, 2**20 * (1 - 2**-12), 0]
        self.assertGreater(determinant(triangle[0], camera), 0)
        # A finite +X frustum with r<=20 would wrongly reject this triangle.
        self.assertLess(-8 + 20 * 2**-10, 0)
        self.assert_safe(triangle, bake(triangle, [0, 0, 0, 10]), camera)

    def test_inside_boundary_tangent_and_unknown_states_keep_everything(self):
        for camera in (
            [0, 0, 0],
            [0, 0, -2],
            [100, 100, 0],
            [np.nan, 0, 0],
            [1e308, 1e308, 0],
        ):
            with np.errstate(over="ignore", invalid="ignore"):
                self.assertEqual(
                    query(self.record, camera, **ENABLED), (True, (1 << 128) - 1)
                )
        for field in ("static", "one_sided", "trusted_winding"):
            for value in (None, False):
                self.assertEqual(
                    query(self.record, [0, 0, -100], **dict(ENABLED, **{field: value})),
                    (True, (1 << 128) - 1),
                )
        for model in (
            None,
            np.diag([-1, 1, 1, 1]),
            np.diag([0, 1, 1, 1]),
            np.diag([1e-9, 1, 1, 1]),
            np.full((4, 4), np.nan),
        ):
            self.assertEqual(
                query(self.record, [0, 0, -100], **dict(ENABLED, model=model)),
                (True, (1 << 128) - 1),
            )

    def test_original_non_enclosing_sphere_is_not_silently_enlarged(self):
        sphere = [0, 0, 0, 1]
        record = bake(self.triangles, sphere)
        self.assertEqual(record["reason"], "sphere-unproved")
        np.testing.assert_array_equal(record["sphere"], sphere)
        self.assertEqual(query(record, [0, 0, -100], **ENABLED), (True, (1 << 128) - 1))

    def test_degenerate_geometry_and_opposed_normals_keep_cone(self):
        triangles = np.concatenate(
            (self.triangles[:1], self.triangles[:1, ::-1], np.zeros((1, 3, 3)))
        )
        record = bake(triangles, [0, 0, 0, 2])
        for camera in itertools.product((-100.0, 0.0, 100.0), repeat=3):
            self.assert_safe(triangles, record, camera)

    def test_quantized_apex_is_behind_every_original_plane(self):
        rng = np.random.default_rng(811)
        triangles = self.triangles[:32].copy()
        triangles[:, :, 2] = rng.uniform(-0.1, 0.1, (32, 3))
        record = bake(triangles, [0, 0, 0, 2])
        self.assertEqual(record["reason"], "cone")
        x, y, z, _cutoff, distance = struct.unpack("<3bBf", record["payload"])
        apex = [-F(a) * F(distance) for a in (x, y, z)]
        for triangle in triangles:
            self.assertLessEqual(determinant(triangle, apex), 0)
        for camera in rng.uniform(-100, 100, (96, 3)):
            self.assert_safe(triangles, record, camera)

    def test_face_seams_full_rotation_near_far_and_translated_geometry(self):
        rng = np.random.default_rng(12)
        triangles = rng.uniform(-1, 1, (19, 3, 3))
        offset = np.array([4e6, -3e6, 2e6])
        triangles = (triangles + offset).astype(np.float32).astype(float)
        record = bake(triangles, [*offset, 3])
        for radius in (0, 3, np.nextafter(3.0, np.inf), 10, 1e8):
            for direction in itertools.product((-1, 0, 1), repeat=3):
                camera = offset + radius * np.array(direction)
                self.assert_safe(triangles, record, camera)
        for yaw in np.linspace(0, 2 * np.pi, 73):
            self.assert_safe(
                triangles, record, offset + [30 * np.cos(yaw), 30 * np.sin(yaw), -5]
            )

    def test_length_outward_encloses_exact_squared_length(self):
        rng = np.random.default_rng(2015)
        vectors = rng.uniform(-1, 1, (256, 3)) * np.exp2(
            rng.integers(-500, 500, (256, 1))
        )
        low, high = length(point(vectors))
        for vector, a, b in zip(vectors, low, high, strict=True):
            square = sum(F(float(x)) ** 2 for x in vector)
            self.assertLessEqual(F(float(a)) ** 2, square)
            self.assertGreaterEqual(F(float(b)) ** 2, square)

    def test_near_crossing_is_clipped_and_rasterized_instead_of_dropped(self):
        triangle = np.array(
            [[[-0.01, -0.01, 0.05], [0, 0.03, 0.2], [0.03, -0.01, 0.2]]]
        )
        clipped = clip_near(triangle)
        self.assertEqual(len(clipped), 2)
        self.assertTrue((clipped[:, :, 2] >= 0.1).all())
        depth = render(
            triangle.reshape(-1, 3),
            np.array([[0, 1, 2]]),
            np.zeros(3),
            np.diag([1.0, -1.0, 1.0]),
        )
        self.assertGreater(np.count_nonzero(depth), 100)


if __name__ == "__main__":
    unittest.main()
