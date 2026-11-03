"""Overlap, movement, misses and retained-geometry controls for toroidal clipmaps."""

import unittest

import numpy as np

from terrain_clipmap import layout, positions
from terrain_clipmap_cache import HeightClipmap, exposed_nodes, rectangle


class ToroidalHeightUpdates(unittest.TestCase):
    def test_exposed_bands_equal_set_difference_without_duplicate_corners(self):
        old = np.array([4, -8])
        side = 19
        previous = set(map(tuple, rectangle(old, (side, side))))
        for dx, dy in [(0, 0), (2, 0), (-2, 4), (4, -2), (-6, -8), (19, 0), (21, -30)]:
            new = old + (dx, dy)
            nodes = exposed_nodes(old, new, side)
            expected = set(map(tuple, rectangle(new, (side, side)))) - previous
            self.assertEqual(set(map(tuple, nodes)), expected)
            self.assertEqual(len(nodes), len(expected))

    def test_still_camera_does_no_sampling_and_a_snap_updates_only_exposed_bands(self):
        calls = []
        def sample(xy):
            calls.append(len(xy))
            return 10 + xy[:, 0] / 8 - xy[:, 1] / 16
        cache = HeightClipmap(sample, levels=4, half_cells=8)
        initial = cache.move([0, 0])
        self.assertEqual(initial, 4 * 19**2)
        self.assertEqual(cache.move([0.5, 0.5]), 0)
        self.assertEqual(len(calls), 1)
        self.assertEqual(cache.move([2, 2]), 4 * 19 - 4)
        for camera in [[2, 2], [-3, 2], [4, -6], [800, -1600]]:
            cache.move(camera)
            mesh = layout(camera, levels=4, half_cells=8)
            np.testing.assert_array_equal(positions(mesh, cache), positions(mesh, sample))

    def test_unknown_nodes_are_retained_and_stale_or_non_grid_queries_stay_unknown(self):
        cache = HeightClipmap(lambda xy: np.where(xy[:, 0] >= 0, xy[:, 1], np.nan),
                              levels=1, half_cells=8)
        cache.move([0, 0])
        np.testing.assert_array_equal(cache([[0, 2], [2, 4]]), [2, 4])
        self.assertTrue(np.isnan(cache([[-2, 2], [0.5, 0.5], [20, 0]])).all())
        cache.move([100, 100])
        self.assertTrue(np.isnan(cache([[0, 2]])).all())
        self.assertEqual(cache([[100, 100]])[0], 100)

    def test_failed_sample_does_not_advance_or_modify_the_previous_window(self):
        cache = HeightClipmap(lambda xy: np.ones(len(xy)), levels=2, half_cells=8)
        cache.move([0, 0])
        before = [(level.low.copy(), level.values.copy()) for level in cache.levels]
        cache.sample = lambda xy: np.full(len(xy), np.inf)
        with self.assertRaises(ValueError):
            cache.move([2, 2])
        for level, (low, values) in zip(cache.levels, before):
            np.testing.assert_array_equal(level.low, low)
            np.testing.assert_array_equal(level.values, values)

    def test_finite_but_unrepresentable_coordinates_are_refused_before_casting(self):
        cache = HeightClipmap(lambda xy: np.ones(len(xy)), levels=1, half_cells=8)
        with np.errstate(all="raise"):
            with self.assertRaises(ValueError):
                cache.move([1e100, 0])
            cache.move([0, 0])
            with self.assertRaises(ValueError):
                cache([[1e100, 0]])


if __name__ == "__main__":
    unittest.main()
