import unittest

import numpy as np
from native_visibility_cost import (
    Snapshot,
    compare,
    extend_pyramid,
    groups,
    project,
    query,
)


def scene():
    vertices = np.array(
        [
            [-0.25, -0.25, 0.2],
            [0.25, -0.25, 0.2],
            [0.25, 0.25, 0.2],
            [-0.25, 0.25, 0.2],
            [-0.25, -0.25, 0.8],
            [0.25, -0.25, 0.8],
            [0.25, 0.25, 0.8],
            [-0.25, 0.25, 0.8],
        ]
    )
    return Snapshot(
        vertices,
        np.array([0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7]),
        np.array([[0, 0, 0, 6], [1, 1, 6, 6]]),
        np.repeat(np.eye(4)[None], 2, axis=0),
        np.eye(4),
        np.zeros(3),
        np.zeros((6, 4)),
        np.array([8, 8]),
        [
            np.full((4, 4), 0.6),
            np.full((2, 2), 0.6),
            np.full((1, 1), 0.6),
            np.full((1, 1), 0.6),
        ],
    )


class NativeVisibilityCost(unittest.TestCase):
    def test_split_preserves_every_triangle_and_parent_transform(self):
        snapshot = scene()
        snapshot.models[1, 0, 3] = 7
        lower, upper, models, weights = groups(snapshot, 1)
        np.testing.assert_array_equal(weights, [1, 1, 1, 1])
        np.testing.assert_array_equal(models[:, 0, 3], [0, 0, 7, 7])
        for group, first in enumerate([0, 3, 6, 9]):
            points = snapshot.vertices[snapshot.indices[first : first + 3]]
            self.assertTrue((points >= lower[group]).all())
            self.assertTrue((points <= upper[group]).all())
        report = compare(snapshot, [0, 1])
        self.assertEqual(
            [r["selected_original_triangles"] for r in report["comparisons"]], [4, 4]
        )

    def test_known_reversed_depth_rejects_only_the_far_quad(self):
        snapshot = scene()
        lower, upper, models, weights = groups(snapshot, 0)
        result = query(snapshot, project(snapshot, lower, upper, models), weights, 4)
        self.assertEqual(result["rejected_triangles"], 2)
        self.assertEqual(result["remaining_triangles"], 2)
        self.assertLessEqual(result["pyramid_scalar_reads"], 2 * 16)

    def test_clear_depth_inside_the_rectangle_prevents_rejection(self):
        snapshot = scene()
        snapshot.levels[0][1, 1] = 0
        lower, upper, models, weights = groups(snapshot, 0)
        result = query(snapshot, project(snapshot, lower, upper, models), weights, 4)
        self.assertEqual(result["rejected_triangles"], 0)

    def test_nonpositive_projection_denominator_fails_open(self):
        snapshot = scene()
        snapshot.clip[3, 3] = -1
        lower, upper, models, weights = groups(snapshot, 0)
        result = query(snapshot, project(snapshot, lower, upper, models), weights, 4)
        self.assertEqual(result["rejected_triangles"], 0)

    def test_odd_pyramid_keeps_the_last_edge_and_clear_depth(self):
        depth = np.array([[0.9, 0.8, 0.7], [0.6, 0.5, 0.4], [0.3, 0.2, 0.0]])
        levels = extend_pyramid([depth])
        np.testing.assert_array_equal(levels[1], [[0.5, 0.4], [0.2, 0.0]])
        self.assertEqual(levels[-1][0, 0], 0)


if __name__ == "__main__":
    unittest.main()
