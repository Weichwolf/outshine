"""Source-aware spatial conflicts and unchanged construction semantics."""

import unittest

import numpy as np
import shapely
from stair_support_audit import Reference
from stair_support_checks import crossing, rectangle
from stair_support_plan import context, plan
from stair_support_traffic import TrafficIndex


class SourceMasks(unittest.TestCase):
    def test_own_faces_are_ignored_but_shared_and_foreign_faces_still_conflict(self):
        shape = shapely.box(0, 0, 2, 2)
        near = shapely.box(1, 1, 1.2, 1.2)
        for mask, expected in ((1, False), (2, True), (3, True)):
            index = TrafficIndex([shape], [mask])
            self.assertEqual(index.for_source(0).conflicts(near), expected)

    def test_actual_twenty_centimetre_distance_has_no_round_buffer_gap(self):
        index = TrafficIndex([shapely.box(0, 0, 1, 1)], [2])
        query = index.for_source(0)
        self.assertTrue(query.conflicts(shapely.box(1.2, 0, 2, 1)))
        self.assertFalse(query.conflicts(shapely.box(1.2001, 0, 2, 1)))

    def test_masks_are_owned_and_unassigned_faces_cannot_disappear(self):
        shape = shapely.box(0, 0, 2, 2)
        masks = np.array([2])
        index = TrafficIndex([shape], masks)
        masks[:] = 1
        self.assertTrue(index.for_source(0).conflicts(shape))
        with self.assertRaisesRegex(ValueError, "integer"):
            TrafficIndex([shape], [2.5])
        with self.assertRaisesRegex(ValueError, "positive"):
            TrafficIndex([shape], [0])

    def test_shape_owner_and_count_bounds_remain_explicit(self):
        shape = shapely.Point(0, 0)
        for shapes, masks in (([shape], []), ([shape] * 65537, np.ones(65537, int))):
            with self.assertRaisesRegex(ValueError, "bounded matching"):
                TrafficIndex(shapes, masks)
        index = TrafficIndex([], [])
        self.assertFalse(index.for_source(0).conflicts(shape))
        for owner in (-1, 63, True):
            with self.assertRaisesRegex(ValueError, "owner bit"):
                index.for_source(owner)


class SharedConstruction(unittest.TestCase):
    def test_both_preparations_preserve_complete_finite_outputs(self):
        rows, road, lower = crossing()
        for method in ("fixed", "shifted"):
            old, _before = plan(
                rows, road, lower, method=method, preparation="per_source"
            )
            new, _after = plan(rows, road, lower, method=method, preparation="shared")
            self.assertEqual(len(old), len(new))
            for a, b in zip(old, new, strict=True):
                for first, second in (
                    (a.body, b.body),
                    (a.lower_contact, b.lower_contact),
                    (a.upper_contact, b.upper_contact),
                ):
                    np.testing.assert_array_equal(first.vertices, second.vertices)
                    np.testing.assert_array_equal(first.triangles, second.triangles)

    def test_shared_contact_faces_are_not_duplicated(self):
        upper = rectangle(-2, -2, 2, 2, 3)
        road = {
            "body_vertices": upper.vertices,
            "top_faces": upper.triangles,
            "bottom_faces": upper.triangles,
            "face_sources": np.array([3, 3]),
        }
        builders, _low, _up, _excluded, work = context(
            road, rectangle(-3, -3, 3, 3, 0), {0, 1}
        )
        product, note = builders[0].build(
            shapely.box(-0.4, -0.2, 0.4, 0.2), minimum_height_m=0.1
        )
        self.assertIsNotNone(product, note)
        self.assertEqual(work["construction_upper_faces"], 2)

    def test_overlapping_different_decks_are_not_resolved_by_height_selection(self):
        lower = rectangle(-3, -3, 3, 3, 0)
        first, second = rectangle(-2, -2, 2, 2, 3), rectangle(-2, -2, 2, 2, 5)
        faces = np.vstack([first.triangles, second.triangles + 4])
        road = {
            "body_vertices": np.vstack([first.vertices, second.vertices]),
            "top_faces": faces,
            "bottom_faces": faces,
            "face_sources": np.array([1, 1, 2, 2]),
        }
        builders, _low, _up, _excluded, _work = context(road, lower, {0, 1})
        product, note = builders[0].build(
            shapely.box(-0.4, -0.2, 0.4, 0.2), minimum_height_m=0.1
        )
        self.assertIsNone(product)
        self.assertIn("unambiguous", note["reason"])

    def test_shared_builder_cannot_approve_another_sources_deck(self):
        lower = rectangle(-10, -10, 10, 10, 0)
        first, second = rectangle(-8, -2, -4, 2, 3), rectangle(4, -2, 8, 2, 5)
        faces = np.vstack([first.triangles, second.triangles + 4])
        road = {
            "body_vertices": np.vstack([first.vertices, second.vertices]),
            "top_faces": faces,
            "bottom_faces": faces,
            "face_sources": np.array([1, 1, 2, 2]),
        }
        builders, _low, uppers, _excluded, _work = context(road, lower, {0, 1})
        product, note = builders[0].build(
            shapely.box(5, -0.2, 5.8, 0.2), minimum_height_m=0.1
        )
        self.assertIsNotNone(product, note)
        with self.assertRaisesRegex(ValueError, "patch absent"):
            uppers[0].check(product.upper_contact)
        Reference(second).check(product.upper_contact)

    def test_no_bridge_station_returns_an_explicit_empty_plan(self):
        rows, road, lower = crossing()
        rows = [dict(row, original={"bridge": False}) for row in rows]
        bodies, report = plan(rows, road, lower, method="shifted")
        self.assertFalse(bodies)
        self.assertFalse(report["stations"])
        self.assertEqual(report["preparation_work"]["construction_builders"], 0)


if __name__ == "__main__":
    unittest.main()
