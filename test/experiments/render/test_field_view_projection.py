"""Exact rational counterchecks of the finite-coordinate displacement bound."""

import math
import unittest
from fractions import Fraction
from itertools import product

import numpy as np
from field_view_projection import CapturePlane, displacement_bound


def exact_axis_error(plane, point, eye, basis, focal):
    def vec(v):
        return tuple(Fraction(float(x)) for x in v)

    def dot(a, b):
        return sum(x * y for x, y in zip(a, b, strict=True))

    p, c, d, e = map(vec, (point, plane.centre, plane.toward_eye, eye))
    height = dot(tuple(x - y for x, y in zip(p, c, strict=True)), d) / dot(d, d)
    q = tuple(x - height * y for x, y in zip(p, d, strict=True))
    first = tuple(x - y for x, y in zip(p, e, strict=True))
    second = tuple(x - y for x, y in zip(q, e, strict=True))
    camera = tuple(map(vec, basis))
    a, b = dot(first, camera[2]), dot(second, camera[2])
    return tuple(
        abs(
            Fraction(float(focal))
            * (dot(first, camera[i]) / a - dot(second, camera[i]) / b)
        )
        for i in (0, 1)
    )


class ProjectionTests(unittest.TestCase):
    def test_bound_contains_exact_corner_and_interior_displacements(self):
        rng = np.random.default_rng(1663)
        direction = np.array([0.5, math.sqrt(0.5), 0.5])
        plane = CapturePlane((-120, -35, -80), (180, 95, 110), (17, 18, 19), direction)
        for distance in (500, 64000, 240000):
            eye = np.array(plane.centre) + direction * distance + [20, -5, 9]
            forward = np.array(plane.centre) - eye
            forward /= np.linalg.norm(forward)
            right = np.cross(forward, [0, 1, 0])
            right /= np.linalg.norm(right)
            basis = (right, np.cross(right, forward), forward)
            result = displacement_bound(plane, eye, basis, 772.5483399593904)
            points = [
                *product(*zip(plane.lower, plane.upper, strict=True)),
                *rng.uniform(plane.lower, plane.upper, (32, 3)),
            ]
            for point in points:
                actual = exact_axis_error(plane, point, eye, basis, 772.5483399593904)
                for value, bound in zip(actual, result["axis_upper_px"], strict=True):
                    self.assertLessEqual(value, Fraction(bound))
                self.assertLessEqual(
                    sum(actual), Fraction(result["screen_l1_upper_px"])
                )

    def test_coplanar_source_has_only_a_rounding_enclosure(self):
        plane = CapturePlane((-10, -10, 0), (10, 10, 0), (0, 0, 0), (0, 0, 1))
        result = displacement_bound(
            plane, (0, 0, 1000), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 800
        )
        self.assertLess(result["screen_l1_upper_px"], 1e-12)

    def test_continuous_eye_region_bounds_interior_poses_exactly(self):
        plane = CapturePlane((-100, -20, -150), (100, 80, 170), (0, 0, 0), (0, 0, 1))
        centre, basis = (0, 0, 64000), ((1, 0, 0), (0, 1, 0), (0, 0, -1))
        offsets = (
            ((0, 0, 1), -4005, 4001),
            ((1, 0, 0), -595, 595),
            ((0, 1, 0), -595, 595),
        )
        bound = displacement_bound(plane, centre, basis, 800, eye_offsets=offsets)
        rng = np.random.default_rng(16631)
        for _ in range(32):
            eye = np.array(centre, float)
            for axis, low, high in offsets:
                eye += np.asarray(axis) * rng.uniform(low, high)
            point = rng.uniform(plane.lower, plane.upper)
            actual = exact_axis_error(plane, point, eye, basis, 800)
            self.assertLessEqual(sum(actual), Fraction(bound["screen_l1_upper_px"]))

    def test_region_crossing_eye_does_not_inherit_valid_centre(self):
        plane = CapturePlane((-1, -1, -1), (1, 1, 1), (0, 0, 0), (0, 0, 1))
        result = displacement_bound(
            plane, (0, 0, 10), np.eye(3), 800, eye_offsets=(((0, 0, 1), -20, 20),)
        )
        self.assertIsNone(result["screen_l1_upper_px"])

    def test_source_crossing_eye_retains_an_unknown_bound(self):
        plane = CapturePlane((-1, -1, -1), (1, 1, 1), (0, 0, 0), (0, 0, 1))
        result = displacement_bound(plane, (0, 0, 0), np.eye(3), 800)
        self.assertIsNone(result["screen_l1_upper_px"])

    def test_projection_plane_behind_eye_is_rejected_even_for_front_source(self):
        plane = CapturePlane((-1, -1, 1), (1, 1, 2), (0, 0, -1), (0, 0, 1))
        result = displacement_bound(plane, (0, 0, 0), np.eye(3), 800)
        self.assertIsNone(result["screen_l1_upper_px"])

    def test_invalid_geometry_and_camera_are_not_zero_error(self):
        for bounds, direction in (((1, 0, 0), (0, 0, 1)), ((0, 0, 0), (0, 0, 0))):
            with self.assertRaises(ValueError):
                CapturePlane(bounds, (0, 0, 0), (0, 0, 0), direction)
        plane = CapturePlane((-1, -1, -1), (1, 1, 1), (0, 0, 0), (0, 0, 1))
        for focal in (0, -1, math.inf, math.nan):
            with self.assertRaises(ValueError):
                displacement_bound(plane, (0, 0, 10), np.eye(3), focal)
        with self.assertRaises(ValueError):
            displacement_bound(plane, (0, 0, 10), np.ones((3, 3)), 800)

    def test_capture_descriptor_owns_immutable_values(self):
        lower = [-1, -1, -1]
        plane = CapturePlane(lower, (1, 1, 1), (0, 0, 0), (0, 0, 1))
        lower[0] = -100
        self.assertEqual(plane.lower, (-1, -1, -1))


if __name__ == "__main__":
    unittest.main()
