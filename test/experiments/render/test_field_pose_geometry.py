"""Independent rational projections, including interior points and tilted planes."""

import math
import unittest
from fractions import Fraction as F
from itertools import product

import numpy as np
from field_pose_geometry import geometric_candidate, projection_bound
from field_view_projection import CapturePlane, displacement_bound


def exact_error(plane, p, eye, basis, matrix, raster, normal):
    def vec(a):
        return tuple(F(float(x)) for x in a)

    def dot(a, b):
        return sum(x * y for x, y in zip(a, b, strict=True))

    p, c, d, e, n = map(vec, (p, plane.centre, plane.toward_eye, eye, normal))
    h = dot(tuple(x - y for x, y in zip(p, c, strict=True)), n) / dot(d, n)
    q = tuple(x - h * y for x, y in zip(p, d, strict=True))
    clips = []
    for position in (p, q):
        rel = tuple(x - y for x, y in zip(position, e, strict=True))
        camera = (*[dot(rel, vec(axis)) for axis in basis], F(1))
        clips.append(tuple(dot(vec(row), camera) for row in matrix))
    return tuple(
        abs(F(pixels, 2) * (clips[0][i] / clips[0][3] - clips[1][i] / clips[1][3]))
        for i, pixels in enumerate(raster)
    )


def perspective(focal=900, raster=(1280, 720), *, jitter=(0, 0)):
    return np.array(
        [
            [2 * focal / raster[0], 0, jitter[0], 0],
            [0, 2 * focal / raster[1], jitter[1], 0],
            [0, 0, 0, 0.1],
            [0, 0, 1, 0],
        ],
        dtype=np.float32,
    )


class PoseGeometryTests(unittest.TestCase):
    def setUp(self):
        self.plane = CapturePlane(
            (-120, -35, -80), (180, 95, 110), (17, 18, 19), (0, 0, 1)
        )
        self.eye = (20, -5, 64000)
        self.basis = ((1, 0, 0), (0, 1, 0), (0, 0, -1))
        self.raster = (1280, 720)

    def test_exact_corners_and_interior_under_arbitrary_finite_projection(self):
        rng = np.random.default_rng(1821)
        for ortho in (False, True):
            for tilted in (False, True):
                normal = (0.2, -0.3, 1) if tilted else (0, 0, 1)
                matrix = perspective(jitter=(0.01, -0.02))
                if ortho:
                    matrix = np.array(
                        [
                            [0.001, 0, 0.0001, 0.1],
                            [0, 0.002, 0, -0.2],
                            [0, 0, -1e-6, 1],
                            [0, 0, 0, 1],
                        ],
                        dtype=np.float32,
                    )
                result = projection_bound(
                    self.plane, self.eye, self.basis, matrix, self.raster, normal=normal
                )
                self.assertTrue(result["depth_inside"])
                points = [
                    *product(*zip(self.plane.lower, self.plane.upper, strict=True)),
                    *rng.uniform(self.plane.lower, self.plane.upper, (25, 3)),
                ]
                for p in points:
                    actual = exact_error(
                        self.plane, p, self.eye, self.basis, matrix, self.raster, normal
                    )
                    for a, b in zip(actual, result["axis_upper_px"], strict=True):
                        self.assertLessEqual(a, F(b))
                    self.assertLessEqual(sum(actual), F(result["screen_l1_upper_px"]))

    def test_existing_perspective_identity_remains_an_independent_opponent(self):
        matrix = perspective(focal=800, raster=(1024, 1024))
        result = projection_bound(
            self.plane, self.eye, self.basis, matrix, (1024, 1024)
        )
        old = displacement_bound(self.plane, self.eye, self.basis, 800)
        self.assertAlmostEqual(
            result["screen_l1_upper_px"], old["screen_l1_upper_px"], places=12
        )

    def test_distance_is_not_a_fixed_domain_and_motion_changes_the_bound(self):
        results = [
            projection_bound(
                self.plane, (20, -5, distance), self.basis, perspective(), self.raster
            )
            for distance in (500, 2000, 32000, 240000)
        ]
        errors = [r["screen_l1_upper_px"] for r in results]
        self.assertEqual(errors, sorted(errors, reverse=True))
        self.assertFalse(geometric_candidate(results[0], 0.25))
        self.assertTrue(geometric_candidate(results[-1], 0.25))
        lateral = projection_bound(
            self.plane, (64000, 0, 64000), self.basis, perspective(), self.raster
        )
        self.assertGreater(
            lateral["screen_l1_upper_px"], results[2]["screen_l1_upper_px"]
        )

    def test_geometric_near_crossing_cannot_inherit_small_screen_error(self):
        plane = CapturePlane((-1, -1, 0), (1, 1, 0), (0, 0, 0), (0, 0, 1))
        result = projection_bound(
            plane, (0, 0, 0.05), self.basis, perspective(), self.raster
        )
        self.assertLess(result["screen_l1_upper_px"], 1e-8)
        self.assertFalse(result["depth_inside"])
        self.assertFalse(geometric_candidate(result, 0.25))

    def test_source_and_support_projective_eye_crossings_keep_originals(self):
        for plane, eye in (
            (self.plane, (0, 0, 0)),
            (CapturePlane((-1, -1, 1), (1, 1, 2), (0, 0, -1), (0, 0, 1)), (0, 0, 0)),
        ):
            result = projection_bound(plane, eye, np.eye(3), perspective(), self.raster)
            self.assertIsNone(result["screen_l1_upper_px"])
            self.assertFalse(geometric_candidate(result, 0.25))

    def test_invalid_or_unknown_operands_do_not_become_zero_error(self):
        args = (self.plane, self.eye, self.basis, perspective(), self.raster)
        for normal in ((0, 0, 0), (1, 0, 0), (0, math.nan, 1)):
            with self.assertRaises(ValueError):
                projection_bound(*args, normal=normal)
        for raster in ((0, 720), (1280.0, 720), (1280,), (2**24 + 1, 720)):
            with self.assertRaises(ValueError):
                projection_bound(*args[:-1], raster)
        for matrix in (np.zeros((3, 4)), np.full((4, 4), math.nan)):
            with self.assertRaises(ValueError):
                projection_bound(*args[:3], matrix, self.raster)
        result = projection_bound(*args)
        for limit in (-1, math.inf, math.nan):
            with self.assertRaises(ValueError):
                geometric_candidate(result, limit)

    def test_rotated_capture_camera_and_dense_projection_enclose_rational_points(self):
        direction = np.array([0.5, 2**-0.5, 0.5])
        plane = CapturePlane(
            self.plane.lower, self.plane.upper, self.plane.centre, direction
        )
        eye = np.array(plane.centre) + direction * 32000 + [500, -1000, 200]
        forward = np.array(plane.centre) - eye
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, [0, 1, 0])
        right /= np.linalg.norm(right)
        basis = (right, np.cross(right, forward), forward)
        matrix = np.array(
            [
                [1.4, 0.01, 0.02, 0.1],
                [-0.02, 2.5, -0.01, -0.05],
                [0, 0, 0, 0.1],
                [1e-6, -1e-6, 1, 0.01],
            ],
            dtype=np.float32,
        )
        normal = direction + [0.1, 0.2, -0.1]
        result = projection_bound(plane, eye, basis, matrix, self.raster, normal=normal)
        rng = np.random.default_rng(18210)
        for p in rng.uniform(plane.lower, plane.upper, (40, 3)):
            actual = exact_error(plane, p, eye, basis, matrix, self.raster, normal)
            self.assertLessEqual(sum(actual), F(result["screen_l1_upper_px"]))

    def test_reversing_support_normal_keeps_the_same_physical_mapping(self):
        args = (self.plane, self.eye, self.basis, perspective(), self.raster)
        positive = projection_bound(*args, normal=(0.2, -0.1, 1))
        negative = projection_bound(*args, normal=(-0.2, 0.1, -1))
        self.assertEqual(positive, negative)

    def test_same_lens_with_double_raster_doubles_geometric_pixel_bound(self):
        args = (self.plane, self.eye, self.basis, perspective())
        first = projection_bound(*args, self.raster)
        doubled = projection_bound(*args, tuple(2 * v for v in self.raster))
        self.assertEqual(
            doubled["axis_upper_px"], tuple(2 * v for v in first["axis_upper_px"])
        )


if __name__ == "__main__":
    unittest.main()
