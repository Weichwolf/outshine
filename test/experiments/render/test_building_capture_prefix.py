"""Semantic controls for the original native first-callback traversal."""

import unittest

import numpy as np
from building_capture_prefix import (
    NODE,
    capture_tree,
    first_source,
    native_tree,
    ray_entry,
)


class CapturePrefix(unittest.TestCase):
    def test_native_unique_centroid_ties_and_equal_entry_visit_right_first(self):
        bounds = np.tile([[0.0, 0, 0], [1.0, 1, 1]], (4, 1, 1))
        nodes, order = capture_tree(bounds)
        np.testing.assert_array_equal(order, [0, 1, 2, 3])
        source, tests = first_source(
            nodes, order, [True] * 4, [-1, 0.5, 0.5], [[1, 0, 0]]
        )
        self.assertEqual((source[0], tests[0]), (3, 5))

    def test_unprojectable_leaves_do_not_become_an_expensive_callback(self):
        nodes, order = capture_tree(np.tile([[0.0, 0, 0], [1.0, 1, 1]], (4, 1, 1)))
        source, tests = first_source(
            nodes, order, [True, True, False, False], [-1, 0.5, 0.5], [[1, 0, 0]]
        )
        self.assertEqual((source[0], tests[0]), (1, 7))

    def test_axis_span_tie_uses_first_axis_and_partitions_by_local_index(self):
        centres = np.array([[1, 1, 0], [-1, 1, 0], [1, -1, 0], [-1, -1, 0]])
        bounds = np.stack((centres - 0.1, centres + 0.1), axis=1)
        _, order = capture_tree(bounds)
        np.testing.assert_array_equal(order, [3, 1, 2, 0])

    def test_original_first_callback_need_not_be_the_nearest_source_box(self):
        # Left subtree's loose union enters at 0, but its only intersecting leaf
        # is at 100. The right subtree has a leaf at 1. Native sorted DFS must
        # call the left leaf first; a global nearest-box sort is a wrong oracle.
        boxes = np.array(
            [
                [[100.0, 0, 0], [101, 1, 1]],
                [[0, 4, 0], [1, 5, 1]],
                [[1, 0, 0], [2, 1, 1]],
                [[2, 10, 0], [3, 11, 1]],
            ]
        )
        nodes = np.zeros(7, dtype=NODE)
        for at, members, links in (
            (0, [0, 1, 2, 3], [0, 4, 1, 4]),
            (1, [0, 1], [0, 2, 2, 3]),
            (2, [0], [0, 1, 0, 0]),
            (3, [1], [1, 1, 0, 0]),
            (4, [2, 3], [2, 2, 5, 6]),
            (5, [2], [2, 1, 0, 0]),
            (6, [3], [3, 1, 0, 0]),
        ):
            nodes[at]["bounds"] = (boxes[members, 0].min(0), boxes[members, 1].max(0))
            nodes[at]["links"] = links
        source, tests = first_source(
            nodes, np.arange(4), [True] * 4, [0, 0.5, 0.5], [[1, 0, 0]]
        )
        self.assertEqual((source[0], tests[0]), (0, 5))
        hit, along = ray_entry(boxes, np.tile([0, 0.5, 0.5], (4, 1)), [[1, 0, 0]] * 4)
        self.assertEqual(int(np.argmin(np.where(hit, along, np.inf))), 2)

    def test_parallel_tangent_and_root_miss(self):
        box = np.array([[0.0, 0, 0], [1.0, 1, 1]])
        hit, along = ray_entry(box, [[-1, 1, 1], [-1, 2, 1]], [[1, 0, 0], [1, 0, 0]])
        np.testing.assert_array_equal(hit, [True, False])
        self.assertEqual(along[0], 1)
        nodes, order = capture_tree(box[None])
        source, tests = first_source(nodes, order, [True], [-1, 2, 1], [[1, 0, 0]])
        self.assertEqual((source[0], tests[0]), (-1, 1))

    def test_infinite_entry_is_not_a_native_nullopt_but_children_require_finite(self):
        tiny = np.nextafter(0.0, 1.0)
        box = np.array([[1.0, 0, 0], [2.0, 1, 1]])
        nodes, order = capture_tree(box[None])
        source, _ = first_source(nodes, order, [True], [0, 0.5, 0.5], [[tiny, 0, 0]])
        self.assertEqual(source[0], 0)
        nodes, order = capture_tree(np.tile(box, (2, 1, 1)))
        source, tests = first_source(
            nodes, order, [True] * 2, [0, 0.5, 0.5], [[tiny, 0, 0]]
        )
        self.assertEqual((source[0], tests[0]), (-1, 3))

    def test_invalid_or_unbounded_replay_input_is_not_a_miss(self):
        nodes, order = capture_tree(np.array([[[0.0, 0, 0], [1.0, 1, 1]]]))
        with self.assertRaises(ValueError):
            first_source(nodes, order, [True], [0, 0, 0], [[0, 0, 0]])
        with self.assertRaises(ValueError):
            first_source(nodes, order, [True], [0, 0, 0], np.ones((65537, 3)))
        with self.assertRaises(ValueError):
            capture_tree(np.array([[[np.nan, 0, 0], [1.0, 1, 1]]]))

    def test_native_bytes_are_checked_without_replacing_corrupt_input(self):
        boxes = np.array([[[0.0, 0, 0], [1, 1, 1]], [[2.0, 0, 0], [3, 1, 1]]])
        before = boxes.copy()
        nodes, order = capture_tree(boxes)
        snapshot = {
            "count": 2,
            "bounds_hex": boxes.astype("<f8").tobytes().hex(),
            "nodes_hex": nodes.tobytes().hex(),
            "order_hex": order.tobytes().hex(),
        }
        actual, actual_order, _ = native_tree(snapshot)
        self.assertFalse(actual.flags.writeable)
        self.assertFalse(actual_order.flags.writeable)
        np.testing.assert_array_equal(boxes, before)
        snapshot["order_hex"] = order[::-1].tobytes().hex()
        with self.assertRaises(ValueError):
            native_tree(snapshot)

    def test_batch_layout_preserves_each_ray_source_and_work(self):
        boxes = np.array([[[0.0, 0, 0], [1, 1, 1]], [[2.0, 0, 0], [3, 1, 1]]])
        nodes, order = capture_tree(boxes)
        rays = np.array([[1, 0, 0], [1, 0.25, 0], [-1, 0, 0], [0, 1, 0.0]])
        answer, work = first_source(nodes, order, [True] * 2, [-1, 0.5, 0.5], rays)
        singles = [
            first_source(nodes, order, [True] * 2, [-1, 0.5, 0.5], r[None])
            for r in rays
        ]
        np.testing.assert_array_equal(answer, [a[0] for a, _ in singles])
        np.testing.assert_array_equal(work, [w[0] for _, w in singles])


if __name__ == "__main__":
    unittest.main()
