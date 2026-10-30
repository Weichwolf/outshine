"""Whole-box support, exact rational enclosure, unknown projection and budgets."""

import unittest
from fractions import Fraction
from unittest.mock import patch

import numpy as np
from test_triangle_columns import CAPTURE, POINTS, WORLD, rectangles
from triangle_columns import prepare, query, world_rays
from triangle_query_support import potential_samples, screen_bounds


def source_bounds(positions):
    return np.array([positions.min(0), positions.max(0)])


class TriangleSupportTests(unittest.TestCase):
    def test_outward_box_contains_exact_rational_projected_source_points(self):
        rng = np.random.default_rng(47)
        positions = rng.uniform([-2, -3, 0.1], [2, 3, 4], (71, 3)).astype(np.float32)
        world = np.array(
            [
                [1.3, 0.2, 0.1, -0.3],
                [0.1, 0.8, -0.2, 0.4],
                [0, 0, 0.3, 0.1],
                [0.2, -0.1, 0.4, 2],
            ]
        )
        low, high = screen_bounds(
            source_bounds(positions), world, np.array([1280, 720])
        )
        for point in positions:
            p = [Fraction(float(x)) for x in (*point, 1)]
            clip = [
                sum(Fraction(float(v)) * x for v, x in zip(row, p)) for row in world
            ]
            exact = [(clip[0] / clip[3] + 1) * 640, (1 - clip[1] / clip[3]) * 360]
            for axis in range(2):
                self.assertLessEqual(Fraction(float(low[axis])), exact[axis])
                self.assertGreaterEqual(Fraction(float(high[axis])), exact[axis])

    def test_eye_plane_touch_crossing_behind_and_overflow_keep_all_samples(self):
        positions = rectangles()["positions"]
        for w in ([0, 0, 1, -1.5], [0, 0, 1, -1], [0, 0, -1, 0]):
            world = WORLD.copy()
            world[3] = w
            self.assertIsNone(
                screen_bounds(source_bounds(positions), world, np.array([2, 2]))
            )
            keep, known = potential_samples(
                source_bounds(positions), world, POINTS, np.array([2, 2])
            )
            self.assertTrue(keep.all())
            self.assertFalse(known)
        world = np.full((4, 4), 1e308)
        self.assertIsNone(
            screen_bounds(source_bounds(positions), world, np.array([2, 2]))
        )

    def test_box_edges_and_immediate_float_neighbours_are_not_rejected(self):
        positions = rectangles()["positions"]
        points = np.array([[0.5, 0.5], [1.5, 1.5]])
        points = np.r_[
            points, np.nextafter(points, -np.inf), np.nextafter(points, np.inf)
        ]
        keep, known = potential_samples(
            source_bounds(positions), CAPTURE, points, np.array([2, 2])
        )
        self.assertTrue(known)
        self.assertTrue(keep.all())
        keep, _ = potential_samples(
            source_bounds(positions),
            CAPTURE,
            np.array([[-1, 1], [3, 1]]),
            np.array([2, 2]),
        )
        self.assertFalse(keep.any())

    def test_original_hits_and_silhouettes_survive_pose_changes(self):
        rng = np.random.default_rng(47)
        source = rectangles()
        for shift in (-0.7, 0.0, 0.7):
            world = WORLD.copy()
            world[0, 3] += shift
            world[3] = [0.03, -0.02, 0.04, 1]
            clip = (
                np.c_[source["positions"], np.ones(len(source["positions"]))] @ world.T
            )
            edges = clip[:, :2] / clip[:, 3:] * [1, -1] + 1
            points = np.r_[
                edges,
                np.nextafter(edges, -np.inf),
                np.nextafter(edges, np.inf),
                rng.uniform(-2, 4, (80, 2)),
            ]
            for size in (4, 16, 64):
                field = prepare(source, CAPTURE, size)
                bounded = query(field, world, points, [2, 2])
                original = query(field, world, points, [2, 2], whole_support=False)
                for a, b in zip(bounded[:3], original[:3], strict=True):
                    np.testing.assert_array_equal(a, b)

    def test_unknown_projection_uses_original_query_and_same_outputs(self):
        field = prepare(rectangles(), CAPTURE, 16)
        world = WORLD.copy()
        world[3] = [0, 0, 1, -1.5]
        a = query(field, world, POINTS, [2, 2])
        b = query(field, world, POINTS, [2, 2], whole_support=False)
        for actual, expected in zip(a[:3], b[:3], strict=True):
            np.testing.assert_array_equal(actual, expected)
        self.assertFalse(a[3]["support_bounds_available"])
        self.assertEqual(a[3]["ray_rows"], len(POINTS))

    def test_selection_precedes_ray_expansion_and_budget_keeps_inside_unknown(self):
        field = prepare(rectangles(), CAPTURE, 16)
        points = np.r_[POINTS, [[-100, -100]]]
        with patch("triangle_columns.world_rays", wraps=world_rays) as rays:
            result = query(field, WORLD, points, [2, 2], time_limit_s=0)
        self.assertFalse(result[2][:-1].any())
        self.assertTrue(result[2][-1])
        self.assertEqual(result[0][-1], 0)
        self.assertEqual(result[1][-1], -1)
        self.assertEqual(len(rays.call_args.args[1]), result[3]["ray_rows"])
        self.assertLess(result[3]["ray_rows"], len(points))


if __name__ == "__main__":
    unittest.main()
