"""Physical endpads connect whole sources without inventing source topology."""

import copy
import unittest

import numpy as np
import shapely
from elevated_rail_hardware import chains, conflict
from elevated_rail_joints import apply


def source(key, points, nodes):
    return {
        "source_id": key,
        "xy": np.array(points, float),
        "original_mvt_xy": nodes,
        "width_m": 3.8,
    }


class RaisedHardwareChecks(unittest.TestCase):
    def setUp(self):
        self.rows = [
            source("first", [[0, 0], [5, 0], [10, 0]], [[0, 0], [5, 0], [10, 0]]),
            source("second", [[10, 0], [15, 1], [20, 2]], [[10, 0], [15, 1], [20, 2]]),
        ]

    def test_constructed_joint_keeps_every_whole_source_segment_and_originals(self):
        apply(self.rows)
        before = copy.deepcopy(self.rows)
        products, omitted = chains(self.rows)
        self.assertEqual(omitted, [])
        self.assertEqual(len(products), 1)
        np.testing.assert_array_equal(
            products[0]["xy"], [[0, 0], [5, 0], [10, 0], [15, 1], [20, 2]]
        )
        self.assertEqual(
            products[0]["segment_sources"], ["first", "first", "second", "second"]
        )
        for a, b in zip(self.rows, before, strict=True):
            np.testing.assert_array_equal(a["xy"], b["xy"])
            self.assertEqual(a["original_mvt_xy"], b["original_mvt_xy"])
            self.assertEqual(a["junction_pads_xy_m"], b["junction_pads_xy_m"])

    def test_same_position_without_supplied_endpoint_identity_is_not_a_joint(self):
        self.rows[1]["original_mvt_xy"][0] = [100, 100]
        apply(self.rows)
        products, omitted = chains(self.rows)
        self.assertEqual(len(products), 2)
        self.assertEqual(omitted, [])
        self.assertEqual([len(p["members"]) for p in products], [1, 1])

    def test_missing_constructed_pad_duplicate_source_and_capacity_reject(self):
        with self.assertRaisesRegex(ValueError, "physical joint pad"):
            chains(self.rows)
        for rows in ([self.rows[0], self.rows[0]], self.rows * 5, []):
            with self.assertRaisesRegex(ValueError, "distinct whole"):
                chains(rows)

    def test_reversed_input_keeps_its_direction_mapping_and_all_segments(self):
        self.rows[1]["xy"] = self.rows[1]["xy"][::-1].copy()
        self.rows[1]["original_mvt_xy"] = self.rows[1]["original_mvt_xy"][::-1]
        apply(self.rows)
        products, omitted = chains(self.rows)
        self.assertEqual(omitted, [])
        self.assertEqual(
            products[0]["members"],
            [
                {"source_id": "first", "reversed": False},
                {"source_id": "second", "reversed": True},
            ],
        )
        np.testing.assert_array_equal(products[0]["xy"][-1], [20, 2])

    def test_positive_same_kind_volume_overlap_is_distinct_from_boundary_contact(self):
        previous = [
            [
                shapely.box(0, 0, 1, 1),
                shapely.box(3, 0, 4, 1),
                shapely.box(10, 0, 11, 1),
            ]
        ]
        self.assertTrue(
            conflict([shapely.box(0.5, 0, 2, 1), shapely.box(6, 0, 7, 1)], previous)
        )
        self.assertFalse(
            conflict([shapely.box(1, 0, 2, 1), shapely.box(6, 0, 7, 1)], previous)
        )
        self.assertFalse(
            conflict([shapely.box(10, 0, 11, 1), shapely.box(6, 0, 7, 1)], previous)
        )


if __name__ == "__main__":
    unittest.main()
