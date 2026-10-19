"""Analytic overlap/holes/budgets and complete-pair counterchecks for #760."""

import unittest

import numpy as np
from river_membership import NO_SOURCE, face_membership
from shapely.geometry import Polygon, box


class RiverMembershipTests(unittest.TestCase):
    def setUp(self):
        self.vertices = np.array([[0.0, 0.0], [2.0, 0.0], [0.0, 2.0]])
        self.faces = np.array([[0, 1, 2]], dtype=np.uint32)

    def test_positive_partial_overlap_keeps_all_sources_and_representative(self):
        sources = [box(0, 0, 1, 2), box(1, 0, 2, 2), box(2, 0, 3, 2)]
        result = face_membership(self.vertices, self.faces, sources)
        np.testing.assert_array_equal(result.offsets, [0, 2])
        np.testing.assert_array_equal(result.members, [0, 1])
        np.testing.assert_array_equal(result.member_area_m2, [1.5, 0.5])
        np.testing.assert_array_equal(result.representative, [0])
        self.assertEqual(result.uncovered_area_m2[0], 0)

    def test_duplicate_original_records_remain_distinct_members(self):
        source = box(-1, -1, 3, 3)
        result = face_membership(self.vertices, self.faces, [source, source])
        np.testing.assert_array_equal(result.members, [0, 1])
        np.testing.assert_array_equal(result.representative, [0])
        np.testing.assert_array_equal(result.member_area_m2, [2, 2])
        self.assertEqual(result.uncovered_area_m2[0], 0)

    def test_provenance_does_not_imply_full_coverage(self):
        result = face_membership(self.vertices, self.faces, [box(0, 0, 1, 0.5)])
        np.testing.assert_array_equal(result.members, [0])
        self.assertEqual(result.member_area_m2[0], 0.5)
        self.assertEqual(result.uncovered_area_m2[0], 1.5)

    def test_hole_and_edge_contact_do_not_invent_ancestry(self):
        source = Polygon(
            [(-1, -1), (3, -1), (3, 3), (-1, 3)],
            [[(-0.1, -0.1), (2.1, -0.1), (2.1, 2.1), (-0.1, 2.1)]],
        )
        result = face_membership(self.vertices, self.faces, [source, box(2, 0, 3, 1)])
        np.testing.assert_array_equal(result.offsets, [0, 0])
        self.assertEqual(result.representative[0], NO_SOURCE)
        self.assertEqual(result.status[0], 1)
        self.assertEqual(result.uncovered_area_m2[0], 2)

    def test_small_positive_sliver_is_retained_without_area_threshold(self):
        width = 2.0**-40
        result = face_membership(self.vertices, self.faces, [box(0, 0, width, 1)])
        np.testing.assert_array_equal(result.members, [0])
        self.assertEqual(result.member_area_m2[0], width)

    def test_reference_budget_rejects_whole_face_but_keeps_independent_face(self):
        vertices = np.r_[self.vertices, self.vertices + [10, 0]]
        faces = np.array([[0, 1, 2], [3, 4, 5]], dtype=np.uint32)
        sources = [box(-1, -1, 3, 3), box(-1, -1, 3, 3), box(9, -1, 13, 3)]
        for budget in ("member_limit", "intersection_limit"):
            result = face_membership(vertices, faces, sources, **{budget: 1})
            np.testing.assert_array_equal(result.offsets, [0, 0, 1])
            np.testing.assert_array_equal(result.members, [2])
            self.assertNotEqual(result.status[0], 0)
            self.assertEqual(result.status[1], 0)
            self.assertTrue(np.isnan(result.uncovered_area_m2[0]))

    def test_indexed_matches_complete_pairs_with_overlaps_and_empty_space(self):
        vertices = np.r_[self.vertices, self.vertices + [4, 0], self.vertices + [8, 0]]
        faces = np.arange(9, dtype=np.uint32).reshape(-1, 3)
        sources = [box(-1, -1, 1, 3), box(0, 0, 3, 3), box(4, 0, 5, 2)]
        indexed = face_membership(vertices, faces, sources)
        brute = face_membership(vertices, faces, sources, indexed=False)
        for name in (
            "offsets",
            "members",
            "representative",
            "member_area_m2",
            "uncovered_area_m2",
            "status",
        ):
            np.testing.assert_array_equal(getattr(indexed, name), getattr(brute, name))
        self.assertLess(
            indexed.work["requested_intersections"],
            brute.work["requested_intersections"],
        )

    def test_degenerate_face_is_local_and_invalid_indices_fail_preflight(self):
        result = face_membership(
            self.vertices, np.array([[0, 0, 1], [0, 1, 2]]), [box(-1, -1, 3, 3)]
        )
        np.testing.assert_array_equal(result.status, [4, 0])
        with self.assertRaises(ValueError):
            face_membership(self.vertices, np.array([[0, 1, 5]]), [])
        with self.assertRaisesRegex(ValueError, "coordinate budget"):
            face_membership(
                self.vertices,
                self.faces,
                [box(-1, -1, 3, 3)],
                source_coordinate_limit=3,
            )

    def test_exhausted_budget_stops_queries_and_empty_table_creates_no_members(self):
        faces = np.repeat(self.faces, 3, axis=0)
        for budget in ("member_limit", "intersection_limit"):
            result = face_membership(
                self.vertices, faces, [box(-1, -1, 3, 3)], **{budget: 1}
            )
            np.testing.assert_array_equal(result.offsets, [0, 1, 1, 1])
            self.assertEqual(result.work["enumerated_candidates"], 1)
            self.assertEqual(result.work["requested_intersections"], 1)
        empty = face_membership(
            self.vertices, self.faces, [], intersection_limit=0, member_limit=0
        )
        np.testing.assert_array_equal(empty.offsets, [0, 0])
        self.assertEqual(empty.representative[0], NO_SOURCE)


if __name__ == "__main__":
    unittest.main()
