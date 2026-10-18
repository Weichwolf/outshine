"""Whole wider frozen profiles must survive narrow new-source support masks."""

import unittest

import numpy as np
import shapely
from stair_module_roots import profile_masks
from stair_module_trace import compare


class CachedProfileMasks(unittest.TestCase):
    def setUp(self):
        self.line = np.array([[-2.75, 0, 1], [0, 0, 1], [2.75, 0, 1]])
        self.nodes = [{"xy": [0, 0], "incident": [0]}]
        self.mask = shapely.Point(0, 0).buffer(1.8 / 2 + 0.75)
        self.vertices = np.array(
            [[-3, -1, 1], [3, -1, 1], [3, 1, 1], [-3, 1, 1]], float
        )
        self.faces = np.array([[0, 1, 2], [0, 2, 3]])

    def test_clipped_old_reference_is_rejected_and_full_mask_passes(self):
        group = [(self.vertices, self.faces, self.mask)]
        with self.assertRaisesRegex(ValueError, "uncovered interval"):
            compare(
                [group, [(self.vertices, self.faces, None)]], self.line[:, :2], False
            )
        full = profile_masks(self.nodes, [self.mask], self.line, [])[0]
        self.assertTrue(full.covers(shapely.LineString(self.line[:, :2])))
        self.assertLess(self.mask.difference(full).area, 1e-12)
        for cast in (False, True):
            proof = compare(
                [
                    [(self.vertices, self.faces, full)],
                    [(self.vertices, self.faces, None)],
                ],
                self.line[:, :2],
                cast,
            )
            self.assertEqual(proof["maximum_height_disagreement_m"], 0)

    def test_height_disagreement_still_fails_after_support_completion(self):
        full = profile_masks(self.nodes, [self.mask], self.line, [])[0]
        for cast in (False, True):
            with self.assertRaisesRegex(ValueError, "heights disagree"):
                compare(
                    [
                        [(self.vertices, self.faces, full)],
                        [(self.vertices + [0, 0, 0.001], self.faces, None)],
                    ],
                    self.line[:, :2],
                    cast,
                )

    def test_already_complete_support_is_returned_unchanged(self):
        full = shapely.box(-4, -4, 4, 4)
        result = profile_masks(self.nodes, [full], self.line, [])
        self.assertIs(result[0], full)
        self.assertEqual(result[0].wkb, full.wkb)

    def test_each_frozen_profile_at_shared_or_distinct_nodes_is_complete(self):
        crossing = self.line[:, [1, 0, 2]]
        remote = self.line + [10, 0, 0]
        nodes = self.nodes + [{"xy": [10, 0], "incident": [1]}]
        masks = [self.mask, shapely.Point(10, 0).buffer(1.65)]
        result = profile_masks(
            nodes, masks, self.line, [{"profile": crossing}, {"profile": remote}]
        )
        for index, line in ((0, self.line), (0, crossing), (1, remote)):
            self.assertTrue(result[index].covers(shapely.LineString(line[:, :2])))
        self.assertIs(masks[0], self.mask)
        self.assertFalse(masks[0].covers(shapely.LineString(self.line[:, :2])))

    def test_missing_or_ambiguous_original_node_cannot_supply_a_mask(self):
        for nodes in ([], self.nodes * 2, [{"xy": [0.001, 0], "incident": [0]}]):
            with self.assertRaisesRegex(ValueError, "one saved node"):
                profile_masks(nodes, [self.mask] * len(nodes), self.line, [])


if __name__ == "__main__":
    unittest.main()
