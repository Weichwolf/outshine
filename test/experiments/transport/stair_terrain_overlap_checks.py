"""Whole-neighbour exact cover, real contact slivers and conservative failure cases."""

import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from stair_module_topology import check_surface
from stair_terrain_overlap import remove_covered_duplicates


def square_case():
    xy = np.array(
        [[0, 0], [2, 0], [2, 2], [0, 2], [0.2, 0.4], [1.8, 0.4], [1, 1.8]], float
    )
    return xy, np.array([[0, 1, 2], [0, 2, 3], [4, 5, 6], [5, 6, 4]])


def actual_contact():
    path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/transport/terrain-duplicate-contact.json"
    )
    fixture = json.loads(path.read_text())
    return fixture, np.array(fixture["xy_m"]), np.array(fixture["faces"])


class CoveredTerrainDuplicates(unittest.TestCase):
    def test_original_contact_is_covered_jointly_and_passes_unchanged_DQ(self):
        fixture, xy, faces = actual_contact()
        polygons = shapely.polygons(xy[faces])
        self.assertFalse(shapely.coverage_is_valid(polygons))
        self.assertFalse(any(shapely.covers(p, polygons[0]) for p in polygons[2:]))
        result, work = remove_covered_duplicates(xy, faces)
        np.testing.assert_array_equal(result, faces[2:])
        self.assertEqual(work["removed_facets"], fixture["expected_removed_facets"])
        self.assertEqual(len(result), fixture["expected_retained_faces"])
        check_surface(xy, result)

    def test_multi_triangle_cover_does_not_require_one_owning_facet(self):
        xy, faces = square_case()
        result, work = remove_covered_duplicates(xy, faces)
        np.testing.assert_array_equal(result, faces[:2])
        self.assertEqual(work["removed_facets"], 2)
        self.assertTrue(
            shapely.union_all(shapely.polygons(xy[result])).equals(
                shapely.box(0, 0, 2, 2)
            )
        )

    def test_ordinary_valid_mesh_is_the_same_object_and_keeps_all_bytes(self):
        xy, faces = square_case()
        faces = faces[:2]
        before = xy.tobytes(), faces.tobytes()
        result, work = remove_covered_duplicates(xy, faces)
        self.assertIs(result, faces)
        self.assertEqual(work, {})
        self.assertEqual(before, (xy.tobytes(), faces.tobytes()))

    def test_one_ulp_of_uncovered_boundary_is_not_zero(self):
        xy, faces = square_case()
        xy[-1, 1] = np.nextafter(2.0, np.inf)
        before = xy.copy(), faces.copy()
        result, work = remove_covered_duplicates(xy, faces)
        self.assertIs(result, faces)
        self.assertEqual(work["removed_facets"], 0)
        np.testing.assert_array_equal(xy, before[0])
        np.testing.assert_array_equal(faces, before[1])
        with self.assertRaises(ValueError):
            check_surface(xy, result)

    def test_small_real_island_cannot_be_removed_by_an_area_threshold(self):
        xy, faces = square_case()
        xy[-3:] = [[1, 2 + 2**-30], [1 + 2**-24, 2 + 2**-30], [1, 2 + 2**-29]]
        self.assertGreater(shapely.Polygon(xy[-3:]).area, 0)
        self.assertLess(shapely.Polygon(xy[-3:]).area, 1e-12)
        result, work = remove_covered_duplicates(xy, faces)
        self.assertIs(result, faces)
        self.assertEqual(work["removed_facets"], 0)

    def test_duplicate_groups_cannot_justify_each_other(self):
        xy = np.array([[0, 0], [1, 0], [0, 1]] * 2, float)
        faces = np.array([[0, 1, 2], [1, 2, 0], [3, 4, 5], [4, 5, 3]])
        result, work = remove_covered_duplicates(xy, faces)
        self.assertIs(result, faces)
        self.assertEqual(work["removed_facets"], 0)
        self.assertEqual(work["triangle_tests"], 0)

    def test_unrelated_overlaps_without_duplicate_faces_remain_rejected(self):
        xy, faces = square_case()
        result, work = remove_covered_duplicates(xy, faces[:3])
        self.assertEqual(work, {})
        self.assertEqual(len(result), 3)
        with self.assertRaises(ValueError):
            check_surface(xy, result)

    def test_face_order_and_winding_do_not_change_the_covered_domain(self):
        xy, faces = square_case()
        for rows in (faces[::-1], faces[:, ::-1], faces[[2, 0, 3, 1]]):
            result, work = remove_covered_duplicates(xy, rows)
            self.assertEqual(work["removed_facets"], 2)
            self.assertTrue(
                shapely.union_all(shapely.polygons(xy[result])).equals(
                    shapely.box(0, 0, 2, 2)
                )
            )

    def test_capacity_failures_keep_the_original_inputs(self):
        xy, faces = square_case()
        before = xy.copy(), faces.copy()
        for option in ("max_groups", "max_neighbours", "max_clips"):
            with (
                self.subTest(option=option),
                self.assertRaisesRegex(ValueError, "budget exhausted"),
            ):
                remove_covered_duplicates(xy, faces, **{option: 0})
        np.testing.assert_array_equal(xy, before[0])
        np.testing.assert_array_equal(faces, before[1])

    def test_invalid_limits_and_numeric_inputs_are_not_cover_proofs(self):
        xy, faces = square_case()
        for wrong in (True, -1, 65, 0.5):
            with self.subTest(limit=wrong), self.assertRaises(ValueError):
                remove_covered_duplicates(xy, faces, max_groups=wrong)
        for bad_xy, bad_faces in (
            (xy.astype(int), faces),
            (xy * np.nan, faces),
            (xy, faces - 1),
            (xy, faces.astype(float)),
            (xy[:, :1], faces),
        ):
            with self.assertRaises(ValueError):
                remove_covered_duplicates(bad_xy, bad_faces)


if __name__ == "__main__":
    unittest.main()
