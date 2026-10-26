"""Independent inset-box expectations for bounded reference query modes."""

import unittest

import numpy as np
from column_field_query import trace, walk_cells


def inset_box():
    return {
        "header": {"pitch_m": 16.0, "footprint_fraction": 0.75},
        "basis": np.eye(3),
        "low": np.zeros(2, np.int64),
        "shape": np.ones(2, np.int64),
        "lookup": np.array([[0]], np.int32),
        "columns": np.array([[0, 0]], np.int32),
        "offsets": np.array([0, 1], np.uint32),
        "spans": np.array([[0.0, 6.0]]),
        "height_bounds": np.array([0.0, 6.0]),
    }


class ColumnIntervalTests(unittest.TestCase):
    def test_inset_box_depths_normals_and_parallel_gap_work(self):
        origins = np.array([[8, -10, 3], [1, -10, 3], [8, 8, 50], [8, 8, -2]])
        directions = np.array([[0, 1, 0], [0, 1, 0], [0, 0, -1], [0, 0, 1]])
        depth, normal, owner, work = trace(inset_box(), origins, directions)
        np.testing.assert_array_equal(depth, [12, np.inf, 44, 2])
        np.testing.assert_array_equal(owner, [0, -1, 0, 0])
        np.testing.assert_array_equal(
            normal, [[0, -1, 0], [0, 0, 0], [0, 0, 1], [0, 0, -1]]
        )
        self.assertEqual(work["cell_ray_visits"], 3)
        self.assertEqual(work["interval_ray_tests"], 3)

    def test_close_shadow_blocker_and_explicit_interior_occlusion(self):
        depth, _, _, _ = trace(inset_box(), [[8, 1.99, 3]], [[0, 1, 0]], minimum=0)
        self.assertAlmostEqual(depth[0], 0.01)
        origins = np.array([[8, 8, 3], [1, 8, 3], [8, 8, 6.001], [8, 8, -1]])
        directions = np.tile([0, 0, 1], (4, 1))
        with self.assertRaisesRegex(ValueError, "inside occupied"):
            trace(inset_box(), origins[:1], directions[:1], minimum=0)
        depth, _, _, work = trace(
            inset_box(), origins, directions, minimum=0, inside_is_hit=True
        )
        np.testing.assert_array_equal(depth, [0, np.inf, np.inf, 1])
        self.assertEqual(work["near_limit_hits"], 1)

    def test_finite_per_ray_intervals_and_walk_budget(self):
        origins = np.tile([8, -10, 3], (2, 1))
        directions = np.tile([0, 1, 0], (2, 1))
        depth, _, _, _ = trace(
            inset_box(), origins, directions, ray_bounds=[[0, 11.99], [0, 12]]
        )
        np.testing.assert_array_equal(depth, [np.inf, 12])
        work = {"steps": 0, "cell_ray_visits": 0}
        self.assertEqual(
            list(walk_cells(inset_box(), origins, directions, work, maximum=9)), []
        )
        self.assertEqual(work["cell_ray_visits"], 0)
        for intervals in [np.zeros((2, 3)), [[0, np.nan], [0, 12]]]:
            with self.assertRaises(ValueError):
                trace(inset_box(), origins, directions, ray_bounds=intervals)


if __name__ == "__main__":
    unittest.main()
