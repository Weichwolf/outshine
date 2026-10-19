"""Sample-cell counting and explicit near/reuse rejection before voxel fitting."""

import unittest

import numpy as np
from far_voxel_preflight import voxel_preflight


class VoxelPreflightTests(unittest.TestCase):
    def test_distinct_source_hits_are_counted_without_inventing_coverage(self):
        positions = np.array([[0.0, 0.0, 0.0], [100.0, 100.0, 100.0]])
        hits = np.array([[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [100.0, 100.0, 100.0]])
        row = voxel_preflight(
            positions,
            np.array([50.0, -1000.0, 50.0]),
            hits,
            extent_multiple=8,
            focal_px=1044.0,
            width=1280,
            height=720,
        )
        self.assertTrue(row["eligible"])
        self.assertEqual(row["sampled_nonempty_cells"], 2)
        self.assertEqual(row["sampled_source_hits"], 3)
        self.assertLessEqual(row["max_voxel_point_pair_px_bound"], 1)
        self.assertEqual(row["optimistic_position_normal_colour_bytes"], 28)

    def test_near_reuse_region_rejects_and_empty_hits_stay_empty(self):
        positions = np.array([[0.0, 0.0, 0.0], [1.0, 1.0, 1.0]])
        row = voxel_preflight(
            positions,
            np.array([0.0, -4.0, 0.0]),
            positions,
            extent_multiple=4,
            focal_px=1044.0,
            width=1280,
            height=720,
        )
        self.assertFalse(row["eligible"])
        positions *= 100
        row = voxel_preflight(
            positions,
            np.array([50.0, -1000.0, 50.0]),
            np.empty((0, 3)),
            extent_multiple=8,
            focal_px=1044.0,
            width=1280,
            height=720,
        )
        self.assertEqual(row["sampled_nonempty_cells"], 0)


if __name__ == "__main__":
    unittest.main()
