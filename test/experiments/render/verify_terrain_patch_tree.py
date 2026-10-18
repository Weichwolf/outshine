"""Compare the tree selector with every admissible spatial partition."""

import itertools
import unittest

import numpy as np
from terrain_patch_tree import solve


class TerrainPatchTree(unittest.TestCase):
    def test_random_costs_match_all_seventeen_partitions_and_cover_each_leaf_once(self):
        rng = np.random.default_rng(515)
        for _ in range(1000):
            costs = [rng.integers(1, 2049, size=n) for n in (16, 4, 1)]
            candidates = [int(costs[2][0])]
            for use_parent in itertools.product((False, True), repeat=4):
                total = 0
                for node, merged in enumerate(use_parent):
                    row, col = divmod(node, 2)
                    leaves = costs[0].reshape(4, 4)[2 * row : 2 * row + 2, 2 * col : 2 * col + 2]
                    total += int(costs[1][node] if merged else leaves.sum())
                candidates.append(total)
            total, selected = solve(costs)
            self.assertEqual(total, min(candidates))
            self.assertEqual(total, sum(costs[level][node] for level, node in selected))
            coverage = np.zeros((4, 4), dtype=int)
            for level, node in selected:
                row, col = divmod(node, 4 >> level)
                side = 1 << level
                coverage[row * side : (row + 1) * side, col * side : (col + 1) * side] += 1
            np.testing.assert_array_equal(coverage, np.ones((4, 4), dtype=int))

    def test_mixed_geometry_preserves_rough_children_and_merges_smooth_neighbours(self):
        cost, selected = solve([np.full(16, 128), np.array([64, 1024, 64, 1024]), np.array([2000])])
        self.assertEqual(cost, 1152)
        self.assertEqual(sum(level == 1 for level, _ in selected), 2)
        self.assertEqual(sum(level == 0 for level, _ in selected), 8)

    def test_partial_visibility_costs_do_not_pay_for_an_entire_merged_parent(self):
        leaves = np.zeros((4, 4), dtype=int)
        leaves[:2, :2] = 32
        cost, selected = solve([leaves.ravel(), np.full(4, 128), np.array([256])])
        self.assertEqual(cost, 128)
        self.assertEqual(selected, [(0, 0), (0, 1), (0, 4), (0, 5)])
        self.assertEqual(solve([np.zeros(16), np.zeros(4), np.zeros(1)]), (0, []))

    def test_incomplete_or_negative_costs_are_rejected(self):
        for costs in ([np.ones(16)], [np.ones(15), np.ones(4), np.ones(1)], [np.full(16, -1), np.ones(4), np.ones(1)]):
            with self.assertRaises(ValueError):
                solve(costs)


if __name__ == "__main__":
    unittest.main()
