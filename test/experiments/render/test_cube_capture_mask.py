"""Independent coverage, slab-ray and capture-boundary checks."""

import itertools
import math
import unittest
from fractions import Fraction as F

import numpy as np
from cube_capture_mask import (
    capture_rectangle,
    occupied_tiles,
    selected_pixels,
    word_masks,
)


def exact_ray_box(box, eye, direction):
    low, high = F(0), None
    for axis in range(3):
        a, b = (F(float(box[i, axis])) - F(float(eye[axis])) for i in (0, 1))
        d = F(float(direction[axis]))
        if not d:
            if a > 0 or b < 0:
                return False
            continue
        first, last = sorted((a / d, b / d))
        low = max(low, first)
        high = last if high is None else min(high, last)
        if high < low:
            return False
    return True


def float_ray_box(box, eye, direction):
    """Scalar native RayEntry arithmetic, independent of the projection code."""
    low, high = 0.0, math.inf
    for axis in range(3):
        if direction[axis] == 0:
            if not box[0, axis] <= eye[axis] <= box[1, axis]:
                return False
            continue
        first = float(box[0, axis] - eye[axis]) / float(direction[axis])
        last = float(box[1, axis] - eye[axis]) / float(direction[axis])
        low, high = max(low, min(first, last)), min(high, max(first, last))
        if low > high:
            return False
    return True


class CubeCaptureMask(unittest.TestCase):
    def test_words_equal_independent_dense_union_including_tail_tiles(self):
        size = 73
        rectangles = [(0, 0, 2, 1), (1, 0, 72, 65), (64, 63, 73, 73), (0, 72, 73, 73)]
        dense = np.zeros((size, size), bool)
        for left, top, right, bottom in rectangles:
            dense[top:bottom, left:right] = True
        tiles = occupied_tiles(rectangles, size)
        masks, work = word_masks(tiles, rectangles, size)
        actual = set(selected_pixels(masks))
        self.assertEqual(actual, set(map(tuple, np.argwhere(dense))))
        self.assertEqual(work["selected_rays"], int(dense.sum()))
        self.assertEqual(work["baseline_rays"], size**2)
        for rows in masks.values():
            self.assertFalse(rows.flags.writeable)

    def test_duplicates_and_input_order_do_not_change_any_ray(self):
        rectangles = [(1, 2, 34, 65), (5, 7, 71, 9), (64, 64, 73, 73)]
        tiles = occupied_tiles(rectangles, 73)
        a, _ = word_masks(tiles, rectangles, 73)
        b, _ = word_masks(tiles, rectangles[::-1] * 2, 73)
        for tile in tiles:
            np.testing.assert_array_equal(a[tile], b[tile])

    def test_no_new_tile_is_admitted_and_budget_failure_restores_original_rays(self):
        masks, work = word_masks({(1, 1)}, [(0, 0, 73, 73)], 73, pair_budget=0)
        self.assertTrue(work["budget_exhausted"])
        self.assertEqual(
            set(selected_pixels(masks)), set(itertools.product(range(64, 73), repeat=2))
        )
        self.assertEqual(work["selected_rays"], 81)

    def test_empty_known_demand_does_not_invent_a_query(self):
        masks, work = word_masks({(0, 0)}, [], 64)
        self.assertEqual(list(selected_pixels(masks)), [])
        self.assertEqual(work["selected_rays"], 0)

    def test_unknown_bound_and_eye_inclusive_contact_admit_all_faces(self):
        for axis, sign in itertools.product(range(3), (-1, 1)):
            self.assertEqual(
                capture_rectangle(None, [0, 0, 0], axis, sign, 73), (0, 0, 73, 73)
            )
            box = np.array([[0.0, -1, -1], [1, 1, 1]])
            self.assertEqual(
                capture_rectangle(box, [0, 0, 0], axis, sign, 73), (0, 0, 73, 73)
            )

    def test_box_straddling_eye_plane_retains_the_face(self):
        box = np.array([[-1.0, -1, -1], [1, 1, 2]])
        self.assertEqual(capture_rectangle(box, [0, 0, -1.5], 0, 1, 64), (0, 0, 64, 64))

    def test_behind_observer_is_not_an_opacity_or_geometry_removal(self):
        box = np.array([[-1.0, -1, -4], [1, 1, -2]])
        self.assertIsNone(capture_rectangle(box, [0, 0, 0], 2, 1, 64))
        self.assertIsNotNone(capture_rectangle(box, [0, 0, 0], 2, -1, 64))

    def test_subnormal_slab_false_positive_uses_the_full_face_fallback(self):
        tiny = np.nextafter(0.0, 1.0)
        box = np.array([[tiny, -1, tiny], [tiny, 1, tiny]])
        eye, ray = np.zeros(3), np.array([0.7, -0.1, 1.0])
        self.assertTrue(float_ray_box(box, eye, ray))
        self.assertFalse(exact_ray_box(box, eye, ray))
        self.assertEqual(capture_rectangle(box, eye, 2, 1, 10), (0, 0, 10, 10))

    def test_all_exact_and_native_slab_hits_fit_on_cube_seams_and_translations(self):
        rng = np.random.default_rng(4459)
        boxes = [
            np.array([[1.0, 0.99, 0.99], [2.0, 1.01, 1.01]]),
            np.array([[-2.0, -1, -1], [-1.0, 1, 1]]),
        ]
        boxes += [
            np.stack((point, point + rng.uniform(0.01, 2, 3)))
            for point in rng.uniform(-4, 4, (8, 3))
        ]
        eyes = [np.zeros(3), np.array([4e6, -3e6, 2e6])]
        size, positive = 17, 0
        for eye, local, axis, sign in itertools.product(eyes, boxes, range(3), (-1, 1)):
            box = local + eye
            rectangle = capture_rectangle(box, eye, axis, sign, size)
            for y, x in itertools.product(range(size), repeat=2):
                ray = np.zeros(3)
                ray[axis] = sign
                ray[(axis + 1) % 3] = 2.0 * (x + 0.5) / size - 1.0
                ray[(axis + 2) % 3] = 2.0 * (y + 0.5) / size - 1.0
                if not (exact_ray_box(box, eye, ray) or float_ray_box(box, eye, ray)):
                    continue
                positive += 1
                self.assertIsNotNone(rectangle)
                left, top, right, bottom = rectangle
                self.assertTrue(left <= x < right and top <= y < bottom)
        self.assertGreater(positive, 100)

    def test_invalid_rectangles_and_coordinate_inputs_are_rejected(self):
        for rectangle in [(-1, 0, 1, 1), (0, 0, 65, 1), (0, 0, 1.0, 1)]:
            with self.assertRaises(ValueError):
                word_masks({(0, 0)}, [rectangle], 64)
        with self.assertRaises(ValueError):
            capture_rectangle([[0, 0, 0], [1, 1, math.inf]], [0, 0, 0], 0, 1, 64)
        with self.assertRaises(ValueError):
            word_masks({(1, 0)}, [], 64)


if __name__ == "__main__":
    unittest.main()
