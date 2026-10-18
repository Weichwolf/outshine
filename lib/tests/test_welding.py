"""Numeric displacement, bounded pair materialization and owned immutable results."""

import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.geometry import weld_vertices


class Welding(unittest.TestCase):
    def test_first_vertex_labels_match_the_prior_reference_in_two_and_three_dimensions(
        self,
    ):
        for dimensions in (2, 3):
            points = np.zeros((4, dimensions))
            points[:, 0] = [0, 0.5e-8, 3e-8, 3e-8]
            before = points.copy()
            expected, labels, drift = points[[0, 2]], [0, 0, 1, 1], 0.5e-8
            actual, result, note = weld_vertices(points, 1e-8)
            np.testing.assert_array_equal(actual, expected)
            np.testing.assert_array_equal(result, labels)
            np.testing.assert_array_equal(points, before)
            self.assertEqual(note["maximum_displacement_m"], drift)
            self.assertEqual(note["candidate_pairs"], 2)

    def test_empty_pair_result_still_owns_immutable_vertices_and_labels(self):
        points = np.array([[0, 0], [1, 0]], float)
        result, labels, note = weld_vertices(points, 1e-8, max_pairs=0)
        self.assertFalse(np.shares_memory(points, result))
        self.assertFalse(result.flags.writeable)
        self.assertFalse(labels.flags.writeable)
        self.assertEqual(note["owned_output_bytes"], result.nbytes + labels.nbytes)
        points[:] = 10
        np.testing.assert_array_equal(result, [[0, 0], [1, 0]])

    def test_long_transitive_chain_is_rejected_instead_of_exceeding_the_bound(self):
        with self.assertRaisesRegex(ValueError, "displacement"):
            weld_vertices(np.array([[0, 0], [0.75e-8, 0], [1.5e-8, 0]]), 1e-8)

    def test_real_northern_pinch_pair_is_separate_at_ten_nm_and_merges_at_one_hundred(
        self,
    ):
        points = np.array(
            [
                [142.15280829206728, 701.8784145833091],
                [142.15280830601947, 701.8784145628729],
            ]
        )
        first, _, _ = weld_vertices(points, 1e-8)
        result, _, note = weld_vertices(points, 1e-7)
        self.assertEqual(len(first), 2)
        self.assertEqual(len(result), 1)
        self.assertGreater(note["maximum_displacement_m"], 2e-8)
        self.assertLess(note["maximum_displacement_m"], 3e-8)

    def test_pair_cap_precedes_materialization(self):
        from scipy.spatial import cKDTree

        tree = cKDTree(np.zeros((4, 2)))

        class CountOnly:
            def count_neighbors(self, _other, _radius):
                return tree.count_neighbors(tree, 1e-8)

            def query_pairs(self, *_args, **_kwargs):
                raise AssertionError("over-budget pair array materialized")

        with (
            patch("scipy.spatial.cKDTree", return_value=CountOnly()),
            self.assertRaisesRegex(ValueError, "pair budget"),
        ):
            weld_vertices(np.zeros((4, 2)), 1e-8, max_pairs=5)

    def test_invalid_domains_and_capacities_fail_explicitly(self):
        for points in ([], [[0]], [[np.nan, 0]], [[1e10, 0]]):
            with self.subTest(points=points), self.assertRaises(ValueError):
                weld_vertices(points, 1e-8)
        for tolerance in (0, True, np.nan, np.inf, 2, "0.1", 1j):
            with self.subTest(tolerance=tolerance), self.assertRaises(ValueError):
                weld_vertices([[0, 0]], tolerance)
        for values in (
            {"max_vertices": 0},
            {"max_vertices": True},
            {"max_pairs": -1},
            {"max_pairs": 1048577},
        ):
            with self.subTest(values=values), self.assertRaises(ValueError):
                weld_vertices([[0, 0]], 1e-8, **values)


if __name__ == "__main__":
    unittest.main()
