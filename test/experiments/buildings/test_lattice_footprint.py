"""Exact enumeration/GEOS controls, boundary ownership and huge-prefix work."""

import unittest
from fractions import Fraction as F

import numpy as np
import shapely
from lattice_footprint import (
    clip_rectangle,
    convex_hull,
    count_centres,
    floor_sum,
    strict_interior,
    swept_square,
)


class LatticeFootprintTest(unittest.TestCase):
    def test_signed_floor_sums_match_direct_integer_enumeration(self):
        for count in range(10):
            for denominator in range(1, 9):
                for slope in range(-9, 10):
                    for offset in [-19, -3, 0, 4, 21]:
                        actual = floor_sum(count, denominator, slope, offset, {})
                        expected = sum(
                            (slope * i + offset) // denominator for i in range(count)
                        )
                        self.assertEqual(actual, expected)

    def test_closed_rectangles_and_degenerate_boundaries(self):
        cases = [
            ([(-2, -3), (3, -3), (3, 4), (-2, 4)], 48),
            (
                [
                    (F(1, 3), F(-7, 3)),
                    (F(25, 3), F(-7, 3)),
                    (F(25, 3), F(31, 3)),
                    (F(1, 3), F(31, 3)),
                ],
                104,
            ),
            ([(0, 0)], 1),
            ([(F(1, 2), 0)], 0),
            ([(0, 0), (0, 5)], 6),
            ([(0, 0), (5, 5)], 6),
        ]
        for points, expected in cases:
            with self.subTest(points=points):
                self.assertEqual(count_centres(convex_hull(points), {}), expected)
        square = convex_hull([(-2, -2), (2, -2), (2, 2), (-2, 2)])
        self.assertEqual(count_centres(clip_rectangle(square, [1, -1], [1, 1]), {}), 3)
        self.assertEqual(count_centres(clip_rectangle(square, [2, 2], [3, 3]), {}), 1)
        self.assertEqual(count_centres(strict_interior(square), {}), 9)
        tiny = swept_square((F(1, 2**80), F(1, 2**80)), (2, 2), F(3, 8))
        self.assertEqual(
            count_centres(clip_rectangle(tiny, np.array([0, 0]), np.array([2, 2])), {}),
            3,
        )

    def test_random_convex_footprints_match_independent_geos_point_queries(self):
        rng = np.random.default_rng(1113)
        x, y = np.meshgrid(np.arange(-12, 13), np.arange(-12, 13))
        xy = np.c_[x.ravel(), y.ravel()]
        points = shapely.points(xy)
        rectangle = ((xy >= [-3, -2]) & (xy <= [5, 7])).all(axis=1)
        for _ in range(250):
            vertices = rng.integers(-40, 41, (8, 2)) / 4
            reference = shapely.convex_hull(shapely.multipoints(vertices))
            hull = convex_hull(vertices)
            self.assertEqual(
                count_centres(hull, {}), int(shapely.covers(reference, points).sum())
            )
            self.assertEqual(
                count_centres(strict_interior(hull), {}),
                int(shapely.contains(reference, points).sum()),
            )
            expected = shapely.covers(reference, points) & rectangle
            self.assertEqual(
                count_centres(clip_rectangle(hull, [-3, -2], [5, 7]), {}),
                int(expected.sum()),
            )

    def test_clipping_preserves_known_integer_boundary_point(self):
        vertices = [
            (-3.5, 0),
            (9.75, 0),
            (1, -4),
            (0, -4),
            (0.25, 2.5),
            (-6.25, -3.75),
            (-8.5, -5),
            (-3.25, -9),
        ]
        clipped = clip_rectangle(convex_hull(vertices), [-3, -2], [5, 7])
        self.assertEqual(count_centres(clipped, {}), 38)

    def test_trillion_cell_prefix_retains_gap_with_bounded_integer_work(self):
        size = 10**12
        work = {}
        triangle = convex_hull([(0, 0), (size, 0), (size, size)])
        self.assertEqual(count_centres(triangle, work), (size + 1) * (size + 2) // 2)
        self.assertLess(work["euclidean_steps"], 20)
        gap = swept_square((F(1, 2), 0), (F(1, 2), size), F(3, 8))
        self.assertEqual(count_centres(gap, {}), 0)
        occupied = swept_square((0, 0), (0, size), F(3, 8))
        self.assertEqual(count_centres(occupied, {}), size + 1)


if __name__ == "__main__":
    unittest.main()
