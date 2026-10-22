"""Cached box bounds retain ties and pruning under divergent ray packets."""

import unittest

import numpy as np
from primitive_bvh import Hierarchy, build, trace
from roof_prisms import Prisms, intersect_planes
from test_roof_prisms import box


def cubes(centres):
    centres = np.asarray(centres, float)
    equations = box()
    planes = np.array(
        [
            equations + np.column_stack((np.zeros((6, 3)), equations[:, :3] @ centre))
            for centre in centres
        ]
    )
    count = len(centres)
    return Prisms(
        planes.reshape(-1, 4),
        np.arange(0, (count + 1) * 6, 6, dtype=np.uint32),
        np.column_stack((np.arange(count), np.zeros(count, np.int32))),
        np.stack((centres - 1, centres + 1), axis=1),
        np.zeros(count * 6, bool),
        np.empty(0, np.uint32),
    )


class IntervalTraversalTest(unittest.TestCase):
    def test_inclusive_cached_bound_keeps_lower_id_at_equal_depth(self):
        solids = cubes([[0, 0, 0], [0, 0, 0]])
        # Visit primitive 1 first; primitive 0's cached entry then equals depth.
        hierarchy = Hierarchy(
            np.repeat(solids.bounds[:1], 3, axis=0),
            np.array([[1, 2], [-1, 1], [-1, 0]], np.int32),
        )
        depth, owner, _, work = trace(solids, hierarchy, [[-3, 0, 0]], [[1, 0, 0]])
        np.testing.assert_array_equal(depth, [2])
        np.testing.assert_array_equal(owner, [0])
        self.assertEqual(work["primitive_ray_tests"], 2)

    def test_pending_far_interval_is_pruned_after_nearer_hit(self):
        solids = cubes([[0, 0, 0], [5, 0, 0]])
        depth, owner, _, work = trace(
            solids, build(solids.bounds), [[-3, 0, 0]], [[1, 0, 0]]
        )
        np.testing.assert_array_equal(depth, [2])
        np.testing.assert_array_equal(owner, [0])
        self.assertEqual(work["node_ray_tests"], 3)
        self.assertEqual(work["primitive_ray_tests"], 1)
        self.assertEqual(work["cached_interval_ray_tests"], 3)
        self.assertEqual(work["max_pending_interval_bytes"], 32)
        self.assertEqual(work["interval_stack_gather_bytes"], 48)

    def test_opposite_and_parallel_rays_keep_their_own_nearest_owner(self):
        solids = cubes([[0, 0, 0], [5, 0, 0]])
        origins = np.array([[-3, 0, 0], [10, 0, 0], [-3, 1, 0], [-3, 2, 0], [0, 0, 0]])
        directions = np.array([[1, 0, 0], [-1, 0, 0]] + [[1, 0, 0]] * 3)
        depth, owner, _, _ = trace(solids, build(solids.bounds), origins, directions)
        np.testing.assert_array_equal(depth, [2, 4, 2, np.inf, 4])
        np.testing.assert_array_equal(owner, [0, 1, 0, -1, 1])

    def test_incoherent_packets_match_exhaustive_primitive_search(self):
        rng = np.random.default_rng(1101)
        solids = cubes(rng.integers(-30, 31, (47, 3)))
        origins = rng.integers(-100, 101, (768, 3)).astype(float)
        directions = solids.bounds.mean(1)[np.arange(len(origins)) % 47] - origins
        depths, faces = zip(
            *[
                intersect_planes(solids.planes[begin:end], origins, directions)
                for begin, end in zip(
                    solids.offsets[:-1], solids.offsets[1:], strict=True
                )
            ],
            strict=True,
        )
        depths = np.array(depths)
        owner = depths.argmin(0)
        depth = depths[owner, np.arange(len(origins))]
        plane = np.array(faces)[owner, np.arange(len(origins))] + solids.offsets[owner]
        finite = np.isfinite(depth)
        owner[~finite], plane[~finite] = -1, -1
        actual = trace(solids, build(solids.bounds), origins, directions)
        for observed, expected in zip(actual[:3], (depth, owner, plane), strict=True):
            np.testing.assert_array_equal(observed, expected)

    def test_empty_packet_and_root_misses_do_not_schedule_intervals(self):
        solids = cubes([[0, 0, 0]])
        hierarchy = build(solids.bounds)
        empty = trace(solids, hierarchy, np.empty((0, 3)), np.empty((0, 3)))
        self.assertEqual(len(empty[0]), 0)
        self.assertTrue(all(value == 0 for value in empty[3].values()))
        origins = np.tile([-3, 4, 0], (17, 1))
        directions = np.tile([1, 0, 0], (17, 1))
        depth, _, _, work = trace(solids, hierarchy, origins, directions)
        self.assertTrue(np.isinf(depth).all())
        self.assertEqual(work["node_ray_tests"], 17)
        self.assertEqual(work["cached_interval_ray_tests"], 0)
        self.assertEqual(work["max_pending_interval_bytes"], 0)


if __name__ == "__main__":
    unittest.main()
