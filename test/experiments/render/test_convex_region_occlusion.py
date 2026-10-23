"""Independent rational segment checks and counterexamples for #1141."""

import unittest
from fractions import Fraction

import numpy as np
from convex_region_occlusion import corners, covered


def exact_segment(start, end, centre, normals, half):
    def local(point):
        return [
            sum(
                Fraction(float(n)) * (Fraction(float(p)) - Fraction(float(c)))
                for n, p, c in zip(row, point, centre)
            )
            for row in normals
        ]

    a, b = local(start), local(end)
    lower, upper = Fraction(0), Fraction(1)
    for left, right, extent in zip(a, b, half):
        extent = Fraction(float(extent))
        if left == right:
            if not -extent < left < extent:
                return False
            continue
        first, last = sorted(
            ((-extent - left) / (right - left), (extent - left) / (right - left))
        )
        lower, upper = max(lower, first), min(upper, last)
    return lower < upper


class ConvexRegionTests(unittest.TestCase):
    def setUp(self):
        self.eye = np.array([[-1.0, -1, -1], [1, 1, 1]])
        self.target = np.array([[-1.0, -1, 20], [1, 1, 22]])
        self.centre = np.array([0.0, 0, 10])
        self.normals = np.eye(3)
        self.half = np.array([4.0, 4, 1])

    def check(self, observer=None, target=None, centre=None, half=None, **kwargs):
        return covered(
            self.eye if observer is None else observer,
            self.target if target is None else target,
            self.centre if centre is None else centre,
            self.normals,
            self.half if half is None else half,
            **kwargs,
        )

    def test_whole_region_and_interior_points(self):
        self.assertTrue(self.check(opaque=True))
        rng = np.random.default_rng(1141)
        eyes = rng.uniform(self.eye[0], self.eye[1], (128, 3))
        targets = rng.uniform(self.target[0], self.target[1], (128, 3))
        for eye, point in zip(eyes, targets):
            self.assertTrue(
                exact_segment(eye, point, self.centre, self.normals, self.half)
            )

    def test_point_visibility_does_not_prove_region(self):
        thin = np.array([0.6, 0.6, 1])
        self.assertTrue(self.check(observer=np.zeros((2, 3)), half=thin, opaque=True))
        self.assertFalse(self.check(half=thin, opaque=True))

    def test_opacity_inside_touch_and_foreground(self):
        self.assertFalse(self.check())
        self.assertFalse(
            self.check(observer=np.array([[-1.0, -1, 9], [1, 1, 11]]), opaque=True)
        )
        self.assertFalse(
            self.check(target=np.array([[0.0, 0, 9], [1, 1, 9]]), opaque=True)
        )
        self.assertFalse(
            self.check(target=np.array([[0.0, 0, 5], [1, 1, 6]]), opaque=True)
        )
        parallel = np.array([[4.0, 0, 20], [4, 0, 22]])
        self.assertFalse(
            self.check(
                observer=np.array([[4.0, 0, 0], [4, 0, 0]]),
                target=parallel,
                opaque=True,
            )
        )

    def test_multiple_blockers_do_not_prove_the_gap(self):
        eye = np.zeros((2, 3))
        target = np.array([[-10.0, -0.1, 20], [10, 0.1, 20]])
        blockers = [np.array([x, 0.0, 10]) for x in (-5, 5)]
        half = np.array([0.3, 1, 0.1])
        for point in corners(target):
            self.assertTrue(
                any(
                    exact_segment(eye[0], point, c, self.normals, half)
                    for c in blockers
                )
            )
        self.assertFalse(
            any(
                self.check(
                    observer=eye, target=target, centre=c, half=half, opaque=True
                )
                for c in blockers
            )
        )
        self.assertFalse(
            any(
                exact_segment(eye[0], np.array([0.0, 0, 20]), c, self.normals, half)
                for c in blockers
            )
        )

    def test_forward_guard_rejects_numeric_grazing(self):
        observer = np.array([[0.0, 0, 0], [0, 0, 0]])
        target = np.array([[1.0, 0, 20], [1, 0, 20]])
        # At z=9 the exact segment lies at x=.45. One ULP is not a useful margin.
        self.assertFalse(
            self.check(
                observer=observer,
                target=target,
                half=np.array([np.nextafter(0.45, 1), 4, 1]),
                opaque=True,
            )
        )

    def test_random_accepted_cases_against_exact_corner_segments(self):
        rng = np.random.default_rng(72)
        accepted = 0
        for _ in range(64):
            half = rng.uniform([0.5, 0.5, 0.1], [6, 6, 2])
            observer = self.eye * rng.uniform(0, 3)
            target = self.target + rng.uniform(-4, 4, 3)
            if not self.check(observer=observer, target=target, half=half, opaque=True):
                continue
            accepted += 1
            for eye in corners(observer):
                for point in corners(target):
                    self.assertTrue(
                        exact_segment(eye, point, self.centre, self.normals, half)
                    )
        self.assertGreater(accepted, 16)

    def test_rotated_core_and_ecef_translation(self):
        angle = 0.37
        c, s = np.cos(angle), np.sin(angle)
        self.normals = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])
        shift = np.array([4e6, 1e6, 5e6])
        observer, target, centre = (
            self.eye + shift,
            self.target + shift,
            self.centre + shift,
        )
        self.assertTrue(
            self.check(observer=observer, target=target, centre=centre, opaque=True)
        )
        for eye in corners(observer):
            for point in corners(target):
                self.assertTrue(
                    exact_segment(eye, point, centre, self.normals, self.half)
                )

    def test_invalid_contracts(self):
        with self.assertRaises(ValueError):
            self.check(half=np.zeros(3), opaque=True)
        with self.assertRaises(ValueError):
            self.check(observer=self.eye.astype(np.float32), opaque=True)
        with self.assertRaises(ValueError):
            self.check(target=np.full((2, 3), np.nan), opaque=True)
        with self.assertRaises(ValueError):
            self.check(target=self.target[::-1], opaque=True)


if __name__ == "__main__":
    unittest.main()
