"""Candidate-only fields, foreground stops and ordered unresolved visibility."""

import unittest

import numpy as np
from reconstructed_field_query import trace, validate
from trilinear_support import CORNERS, LEAF, SOLID, UNKNOWN


def field(tags, values):
    return {
        "root_low": np.zeros(3),
        "root_side": np.array(2.0),
        "tags": np.asarray(tags, np.uint32),
        "values": np.asarray(values, np.float32).reshape(-1, 8),
    }


class ReconstructedQueryTest(unittest.TestCase):
    def test_clipped_cube_is_consumed_without_source_queries(self):
        arrays = field([SOLID], [])
        validate(arrays)
        depth, normal, unknown, _ = trace(arrays, [[1, 1, 3]], [[0, 0, -1]])
        np.testing.assert_array_equal(depth, [1])
        np.testing.assert_array_equal(normal, [[0, 0, 1]])
        self.assertTrue(np.isinf(unknown[0]))

    def test_local_need_is_not_an_empty_cell(self):
        arrays = field([UNKNOWN], [])
        validate(arrays)
        depth, _, unknown, _ = trace(arrays, [[1, 1, 3]], [[0, 0, -1]])
        self.assertTrue(np.isinf(depth[0]))
        self.assertEqual(unknown[0], 1)

    def test_unresolved_region_behind_a_known_first_hit_does_not_block_it(self):
        tags = [1] + [0] * 8
        tags[1 + 6], tags[1 + 7] = UNKNOWN, LEAF
        arrays = field(tags, [CORNERS[:, 2] - 0.5])
        validate(arrays)
        depth, _, unknown, _ = trace(arrays, [[1.5, 1.5, 3]], [[0, 0, -1]])
        np.testing.assert_allclose(depth, [1.5], atol=1e-12)
        self.assertGreater(unknown[0], depth[0])

    def test_retained_foreground_stops_before_unresolved_field_work(self):
        arrays = field([UNKNOWN], [])
        depth, _, unknown, _ = trace(arrays, [[1, 1, 3]], [[0, 0, -1]], np.array([0.5]))
        self.assertTrue(np.isinf(depth[0]))
        self.assertTrue(np.isinf(unknown[0]))

    def test_invalid_child_links_fail_before_traversal(self):
        for tags in [[1], [1, 1, 0, 0, 0, 0, 0, 0, 0]]:
            with self.assertRaises(ValueError):
                validate(field(tags, []))

    def test_zero_only_leaf_is_a_need_without_a_solid_source_certificate(self):
        arrays = field([LEAF], [np.zeros(8)])
        depth, _, unknown, _ = trace(arrays, [[1, 1, 3]], [[0, 0, -1]])
        self.assertTrue(np.isinf(depth[0]))
        self.assertEqual(unknown[0], 1)

    def test_surface_entry_exactly_at_a_neighbor_boundary_is_preserved(self):
        tags = [1] + [0] * 8
        tags[1], tags[3] = LEAF, LEAF | 1
        arrays = field(tags, [1 - CORNERS[:, 1], -CORNERS[:, 1]])
        validate(arrays)
        depth, normal, unknown, _ = trace(arrays, [[0.25, -1, 0.25]], [[0, 1, 0]])
        np.testing.assert_allclose(depth, [2], atol=1e-12)
        np.testing.assert_allclose(normal, [[0, -1, 0]], atol=1e-12)
        self.assertTrue(np.isinf(unknown[0]))

    def test_box_caps_do_not_close_a_gap_between_different_depth_neighbors(self):
        tags = [1] + [0] * 16
        tags[1], tags[3], tags[5], tags[7] = SOLID, SOLID, 9, LEAF
        values = [1.02 - (1 + CORNERS[:, 0])]
        for child, corner in enumerate(CORNERS):
            tags[9 + child] = LEAF | (child + 1)
            values.append(1.02 - (1 + corner[0] * 0.5 + CORNERS[:, 0] * 0.5))
        arrays = field(tags, values)
        validate(arrays)
        depth, _, unknown, _ = trace(arrays, [[1.01, -1, 0.25]], [[0, 1, 0]])
        self.assertTrue(np.isinf(depth[0]))
        self.assertTrue(np.isinf(unknown[0]))
