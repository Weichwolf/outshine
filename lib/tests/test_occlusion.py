"""Opaque planar patch certificates, movement cells and independent exact ray tests."""

import itertools
import unittest
from fractions import Fraction

import numpy as np
from outshine_ref.geometry.occlusion import classify_boxes, convex_patch_umbra

WALL = np.array([[-1, 0, -1], [1, 0, -1], [1, 0, 1], [-1, 0, 1]], dtype=float)


def corners(box):
    return list(itertools.product(*zip(box[0], box[1], strict=True)))


def observer(radius=0):
    return np.array([[-radius, -2 - radius, -radius], [radius, -2 + radius, radius]])


def scalar_triangle_hit(eye, target, triangle):
    """Exact rational Moller-Trumbore segment test, independent of umbra planes."""

    def point(row):
        return tuple(Fraction.from_float(float(v)) for v in row)

    def sub(a, b):
        return tuple(x - y for x, y in zip(a, b, strict=True))

    def dot(a, b):
        return sum(x * y for x, y in zip(a, b, strict=True))

    def cross(a, b):
        return (
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0],
        )

    origin, target = point(eye), point(target)
    a, b, c = map(point, triangle)
    direction, e1, e2 = sub(target, origin), sub(b, a), sub(c, a)
    h = cross(direction, e2)
    determinant = dot(e1, h)
    if determinant == 0:
        return False
    s = sub(origin, a)
    u = dot(s, h) / determinant
    q = cross(s, e1)
    v, t = dot(direction, q) / determinant, dot(e2, q) / determinant
    return 0 <= u <= 1 and 0 <= v and u + v <= 1 and 0 < t < 1


class OpaquePatchOcclusion(unittest.TestCase):
    def test_known_hidden_visible_and_front_boxes(self):
        umbra = convex_patch_umbra(WALL, observer(), certified_opaque=True)
        boxes = np.array(
            [
                [[-0.1, 3, -0.1], [0.1, 3.2, 0.1]],
                [[9, 3, 0], [10, 4, 1]],
                [[0, -1, 0], [0.1, -0.5, 0.1]],
            ]
        )
        hidden, outside = classify_boxes(boxes, umbra)
        np.testing.assert_array_equal(hidden, [True, False, False])
        np.testing.assert_array_equal(outside, [False, True, True])

    def test_single_view_occlusion_is_not_a_movement_cell_certificate(self):
        box = np.array([[[2, 3, 0], [2, 3, 0]]])
        fixed = convex_patch_umbra(WALL, observer(), certified_opaque=True)
        moving = convex_patch_umbra(WALL, observer(1), certified_opaque=True)
        self.assertTrue(classify_boxes(box, fixed)[0][0])
        self.assertFalse(classify_boxes(box, moving)[0][0])
        self.assertFalse(scalar_triangle_hit([1, -1, 0], box[0, 0], WALL[[0, 1, 2]]))
        self.assertFalse(scalar_triangle_hit([1, -1, 0], box[0, 0], WALL[[0, 2, 3]]))

    def test_uncertified_bounds_or_surface_error_do_not_hide_anything(self):
        unknown = convex_patch_umbra(None, None)
        hidden, outside = classify_boxes(np.array([[[0, 3, 0], [0, 4, 0]]]), unknown)
        self.assertFalse(hidden.any() or outside.any())

    def test_backfaces_require_explicit_two_sided_opacity(self):
        box = np.array([[[0, 3, 0], [0, 4, 0]]])
        one = convex_patch_umbra(WALL[::-1], observer(), certified_opaque=True)
        two = convex_patch_umbra(
            WALL[::-1], observer(), certified_opaque=True, two_sided=True
        )
        self.assertFalse(classify_boxes(box, one)[0][0])
        self.assertTrue(classify_boxes(box, two)[0][0])

    def test_view_cell_crossing_patch_and_boundary_ties_remain_visible(self):
        crossing = convex_patch_umbra(
            WALL, [[-1, -1, -1], [1, 1, 1]], certified_opaque=True, two_sided=True
        )
        box = np.array([[[2.5, 3, 0], [2.5, 3, 0]]])
        fixed = convex_patch_umbra(WALL, observer(), certified_opaque=True)
        self.assertFalse(classify_boxes(box, crossing)[0][0])
        self.assertFalse(classify_boxes(box, fixed)[0][0])

    def test_cached_certificate_owns_region_and_rejects_reuse_outside(self):
        polygon, region = WALL.copy(), observer(0.5)
        umbra = convex_patch_umbra(polygon, region, certified_opaque=True)
        polygon[:] = 99
        region[:] = 99
        box = np.array([[[0, 3, 0], [0, 4, 0]]])
        self.assertTrue(classify_boxes(box, umbra, observer=[0, -2, 0])[0][0])
        self.assertFalse(classify_boxes(box, umbra, observer=[1, -2, 0])[0][0])
        self.assertFalse(
            umbra.low.flags.writeable
            or umbra.high.flags.writeable
            or umbra.observer_box.flags.writeable
        )

    def test_exact_segment_oracle_for_translated_moving_boxes(self):
        rng = np.random.default_rng(1200)
        offset = np.array([9_000_000, -8_000_000, 7_000_000])
        polygon, region = WALL + offset, observer(0.25) + offset
        umbra = convex_patch_umbra(polygon, region, certified_opaque=True)
        centres = rng.uniform([-3, 1, -3], [3, 12, 3], (40, 3)) + offset
        boxes = np.stack((centres - 0.05, centres + 0.05), axis=1)
        hidden, _ = classify_boxes(boxes, umbra)
        self.assertGreater(hidden.sum(), 3)
        eyes = corners(region) + [region.mean(axis=0)]
        for box in boxes[hidden]:
            for eye, target in itertools.product(eyes, corners(box)):
                self.assertTrue(
                    scalar_triangle_hit(eye, target, polygon[[0, 1, 2]])
                    or scalar_triangle_hit(eye, target, polygon[[0, 2, 3]])
                )

    def test_concave_or_nonplanar_contours_are_never_used_as_convex_blockers(self):
        concave = [[-1, 0, -1], [1, 0, -1], [0, 0, 0], [1, 0, 1], [-1, 0, 1]]
        warped = WALL.copy()
        warped[2, 1] = 0.01
        for polygon in (concave, warped, WALL[[0, 1, 1, 3]]):
            with self.assertRaises(ValueError):
                convex_patch_umbra(polygon, observer(), certified_opaque=True)

    def test_rotated_triangle_matches_exact_segment_oracle(self):
        transform = np.array([[0.6, -0.8, 0], [0.64, 0.48, -0.6], [0.48, 0.36, 0.8]])
        polygon = np.array([[-3, 0, -3], [3, 0, -3], [0, 0, 3]]) @ transform.T
        eye = np.array([0, -2, 0]) @ transform.T
        region = np.array([eye - 0.1, eye + 0.1])
        umbra = convex_patch_umbra(polygon, region, certified_opaque=True)
        rng = np.random.default_rng(7)
        centres = rng.uniform([-2, 4, -2], [2, 12, 2], (15, 3)) @ transform.T
        boxes = np.stack((centres - 0.01, centres + 0.01), axis=1)
        hidden, _ = classify_boxes(boxes, umbra)
        self.assertGreater(hidden.sum(), 3)
        for box in boxes[hidden]:
            for observer_point, target in itertools.product(
                corners(region) + [eye], corners(box)
            ):
                self.assertTrue(scalar_triangle_hit(observer_point, target, polygon))

    def test_batch_cap_and_invalid_bounds_are_checked(self):
        umbra = convex_patch_umbra(WALL, observer(), certified_opaque=True)
        for boxes in (
            np.zeros((513, 2, 3)),
            np.full((1, 2, 3), np.nan),
            np.array([[[1, 1, 1], [0, 0, 0]]]),
        ):
            with self.assertRaises(ValueError):
                classify_boxes(boxes, umbra)


if __name__ == "__main__":
    unittest.main()
