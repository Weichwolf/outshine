"""Independent raster oracle and conservative visibility counterexamples."""

import unittest

import numpy as np
from current_visibility import (
    bounds,
    box_bounds,
    mesh_boxes,
    occluded,
    project,
    pyramid,
    raster,
    subdivide,
)


def scalar_raster(points, triangles, width, height):
    depth = np.zeros((height, width))
    for face in triangles:
        p = points[face]
        a, b, c = p[:, :2]
        matrix = np.column_stack((a - c, b - c))
        if np.linalg.det(matrix) >= -1e-12:
            continue
        for y in range(height):
            for x in range(width):
                u, v = np.linalg.solve(matrix, np.array((x + 0.5, y + 0.5)) - c)
                if min(u, v, 1 - u - v) >= 0:
                    depth[y, x] = max(depth[y, x], np.dot((u, v, 1 - u - v), p[:, 2]))
    return depth


class CurrentVisibility(unittest.TestCase):
    def test_raster_matches_independent_scalar_oracle_with_overlap_and_clipping(self):
        rng = np.random.default_rng(12)
        points = np.column_stack(
            (rng.uniform(-5, 20, (54, 2)), rng.uniform(0.02, 0.8, 54))
        )
        triangles = np.arange(len(points)).reshape(-1, 3)
        expected = scalar_raster(points, triangles, 16, 13)
        actual = raster(points, np.ones(len(points)), triangles, 16, 13)
        self.assertGreater(np.count_nonzero(expected), 100)
        np.testing.assert_allclose(actual, expected, atol=1e-14, rtol=0)
        np.testing.assert_array_equal(
            actual, raster(points, np.ones(len(points)), triangles[::-1], 16, 13)
        )

    def test_odd_pyramid_edges_and_holes_never_become_occluders(self):
        depth = np.full((5, 7), 0.7)
        depth[2, 3] = 0
        levels = pyramid(depth, 20)
        self.assertEqual([x.shape for x in levels], [(3, 4), (2, 2), (1, 1)])
        self.assertEqual(levels[0][1, 1], 0)
        self.assertTrue(np.all(levels[0][-1] == 0))
        self.assertTrue(np.all(levels[0][:, -1] == 0))
        self.assertEqual(levels[-1][0, 0], 0)

    def test_near_intersections_and_equal_depth_are_retained(self):
        lo = np.array(((2, 2),) * 3, dtype=float)
        hi = np.array(((5, 5),) * 3, dtype=float)
        hidden = occluded(
            lo,
            hi,
            np.array((0.3, 0.7, 0.3)),
            np.array((True, True, False)),
            np.full((8, 8), 0.7),
        )
        np.testing.assert_array_equal(hidden, (True, False, False))

    def test_new_view_hole_reveals_previously_hidden_target(self):
        depth = np.full((16, 16), 0.6)
        lo, hi = np.array(((4, 4),)), np.array(((7, 7),))
        self.assertTrue(occluded(lo, hi, np.array((0.2,)), np.array((True,)), depth)[0])
        depth[5, 5] = 0
        self.assertFalse(
            occluded(lo, hi, np.array((0.2,)), np.array((True,)), depth)[0]
        )

    def test_offscreen_bounds_use_only_the_screen_intersection(self):
        depth = np.zeros((16, 16))
        depth[:8, :8] = 0.7
        hidden = occluded(
            np.array(((-1e9, -1e9),)),
            np.array(((4, 4),)),
            np.array((0.2,)),
            np.array((True,)),
            depth,
            8,
        )
        self.assertTrue(hidden[0])

    def test_query_never_hides_a_box_with_an_unoccluded_pixel(self):
        rng = np.random.default_rng(47)
        depth = rng.uniform(0.1, 0.9, (61, 67))
        depth[rng.uniform(0, 1, depth.shape) < 0.04] = 0
        lo = rng.integers(0, 55, (2000, 2))
        hi = np.minimum(lo + rng.integers(1, 12, (2000, 2)), (66, 60))
        nearest = rng.uniform(0, 0.7, len(lo))
        for count in (1, 4, 12):
            hidden = occluded(
                lo, hi, nearest, np.ones(len(lo), dtype=bool), depth, count
            )
            self.assertGreater(np.count_nonzero(hidden), 0)
            for index in np.flatnonzero(hidden):
                x, y = lo[index]
                xx, yy = hi[index]
                self.assertGreater(
                    float(depth[y : yy + 1, x : xx + 1].min()), nearest[index]
                )

    def test_projected_bounds_contain_points_and_reject_only_fully_behind(self):
        rng = np.random.default_rng(328)
        spheres = np.column_stack(
            (
                rng.uniform(-10, 10, (100, 2)),
                rng.uniform(3, 20, 100),
                rng.uniform(0.1, 2, 100),
            )
        )
        view = np.zeros(3), np.eye(3), 100, 160, 120
        lo, hi, nearest, safe, _, _ = bounds(spheres, view)
        unit = rng.normal(size=(100, 100, 3))
        unit /= np.linalg.norm(unit, axis=2)[..., None]
        points = spheres[:, None, :3] + unit * spheres[:, None, 3:]
        p, _ = project(points.reshape(-1, 3), view)
        p = p.reshape(100, 100, 3)
        self.assertTrue(np.all(safe))
        self.assertTrue(np.all(p[:, :, :2] >= lo[:, None, :]))
        self.assertTrue(np.all(p[:, :, :2] <= hi[:, None, :]))
        self.assertTrue(np.all(p[:, :, 2] <= nearest[:, None]))
        special = np.array(((0, 0, -2, 1), (0, 0, 0, 1)), dtype=float)
        self.assertEqual(bounds(special, view)[4].tolist(), [False, True])
        self.assertEqual(
            box_bounds((special[:, :3] - 1, special[:, :3] + 1), view)[4].tolist(),
            [False, True],
        )

    def test_subdivision_retains_sources_and_exact_triangle_union(self):
        vertices = np.array(((0, 0, 1), (2, 0, 1), (0, 2, 1), (0, 0, 2)), dtype=float)
        triangles = np.array(((0, 1, 2), (1, 2, 3), (0, 3, 1), (2, 3, 0)))
        owners = np.array((0, 0, 0, 1))
        split = subdivide(owners, 2)
        np.testing.assert_array_equal(split, (0, 0, 1, 2))
        low, high = mesh_boxes(vertices, triangles, split, 3)
        for i, face in enumerate(triangles):
            self.assertTrue(np.all(vertices[face] >= low[split[i]]))
            self.assertTrue(np.all(vertices[face] <= high[split[i]]))


class DepthBias(unittest.TestCase):
    def test_relative_depth_bias_scales_with_reverse_depth(self):
        from current_visibility import occluded

        lo, hi = np.array([[0.0, 0.0]]), np.array([[1.0, 1.0]])
        safe = np.array([True])
        depth = np.full((4, 4), 1e-5)
        behind = np.array([0.999e-5])
        self.assertFalse(occluded(lo, hi, behind, safe, depth)[0])
        self.assertTrue(
            occluded(lo, hi, behind, safe, depth, absolute_bias=0, relative_bias=1e-6)[
                0
            ]
        )
        for nearest in (1e-5, 1.01e-5):
            self.assertFalse(
                occluded(
                    lo,
                    hi,
                    np.array([nearest]),
                    safe,
                    depth,
                    absolute_bias=0,
                    relative_bias=1e-6,
                )[0]
            )


if __name__ == "__main__":
    unittest.main()
