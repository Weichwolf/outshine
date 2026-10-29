"""Actual collapsed contact, legal local topology and complete affine height proofs."""

import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_blend_audit import ground_subdivision
from stair_module_topology import check_surface
from stair_terrain_diagonals import repair_diagonals


def fixture():
    path = (
        Path(__file__).parents[2]
        / "outshine/data/transport/terrain-diagonal-contact.json"
    )
    saved = json.loads(path.read_text())
    return np.array(saved["xy_m"]), np.array(saved["faces"]), saved


def boundary(faces):
    edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    keys, count = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
    return {tuple(k) for k in keys[count == 1]}


class TerrainDiagonals(unittest.TestCase):
    def test_actual_contact_repairs_only_two_facets_without_moving_any_point(self):
        xy, faces, _ = fixture()
        original_xy, original_faces = xy.copy(), faces.copy()
        with self.assertRaisesRegex(ValueError, "collapses or inverts"):
            check_surface(xy, faces)
        fixed, work = repair_diagonals(xy, faces, np.ones(len(faces), int))
        check_surface(xy, fixed)
        np.testing.assert_array_equal(xy, original_xy)
        np.testing.assert_array_equal(faces, original_faces)
        self.assertEqual(boundary(fixed), boundary(faces))
        self.assertEqual(np.count_nonzero(np.any(fixed != faces, axis=1)), 2)
        self.assertEqual(work["swaps"], 1)
        before = shapely.union_all(shapely.polygons(xy[faces]))
        after = shapely.union_all(shapely.polygons(xy[fixed]))
        self.assertTrue(before.equals(after))
        unchanged, note = repair_diagonals(xy, fixed, np.ones(len(faces), int))
        self.assertIs(unchanged, fixed)
        self.assertEqual(note, {})

    def test_actual_concave_and_tiny_negative_alternatives_cannot_add_domain(self):
        xy, faces, _ = fixture()
        for neighbour in (2, 3):
            with self.assertRaisesRegex(ValueError, "no D/Q-positive"):
                repair_diagonals(xy, faces[[0, neighbour]], np.ones(2, int))

    def test_real_classes_and_protected_ground_block_the_only_legal_diagonal(self):
        xy, faces, _ = fixture()
        for classes in (np.array([0, 1, 0, 0]), np.full(4, 2)):
            with self.assertRaisesRegex(ValueError, "no D/Q-positive"):
                repair_diagonals(xy, faces, classes)

    def test_boundary_facet_does_not_acquire_a_missing_neighbour(self):
        xy, faces, _ = fixture()
        with self.assertRaisesRegex(ValueError, "no D/Q-positive"):
            repair_diagonals(xy, faces[:1], np.ones(1, int))

    def test_merged_float32_vertices_cannot_be_repaired_by_reindexing(self):
        xy = np.array([[512, 0], [512 + 1e-6, 0], [512, 1]], float)
        with self.assertRaisesRegex(ValueError, "merged Float32"):
            repair_diagonals(xy, np.array([[0, 1, 2]]), np.zeros(1, int))

    def test_face_order_and_cyclic_vertex_order_preserve_the_actual_domain(self):
        xy, faces, _ = fixture()
        for variant in (faces[::-1], np.roll(faces, 1, axis=1)):
            fixed, _ = repair_diagonals(xy, variant, np.ones(4, int))
            check_surface(xy, fixed)
            self.assertEqual(boundary(fixed), boundary(variant))

    def test_candidate_and_swap_budgets_refuse_without_mutating_input(self):
        xy, faces, _ = fixture()
        before = faces.copy()
        with self.assertRaisesRegex(ValueError, "candidate budget"):
            repair_diagonals(xy, faces, np.array([0, 1, 0, 0]), max_tests=1)
        both_xy = np.vstack([xy, xy + [32, 0]])
        both_faces = np.vstack([faces, faces + len(xy)])
        with self.assertRaisesRegex(ValueError, "swap budget"):
            repair_diagonals(both_xy, both_faces, np.ones(8, int), max_swaps=1)
        np.testing.assert_array_equal(faces, before)

    def test_invalid_numeric_and_unbounded_parameters_refuse(self):
        xy, faces, _ = fixture()
        for bad in (xy.astype("f4"), xy * np.nan):
            with self.assertRaises(ValueError):
                repair_diagonals(bad, faces, np.ones(4, int))
        for named in ({"max_tests": 193}, {"max_swaps": 0}, {"max_swaps": True}):
            with self.assertRaises(ValueError):
                repair_diagonals(xy, faces, np.ones(4, int), **named)
        with self.assertRaises(ValueError):
            repair_diagonals(xy, faces, np.ones(4, bool))


class PiecewiseGroundContact(unittest.TestCase):
    def square(self, heights):
        xyz = np.c_[np.array([[0, 0], [1, 0], [1, 1], [0, 1]]), heights]
        return (
            Mesh(xyz, np.array([[0, 1, 3], [1, 2, 3]])),
            Mesh(xyz, np.array([[0, 1, 2], [0, 2, 3]])),
        )

    def test_original_planar_height_and_whole_coverage_cross_the_old_diagonal(self):
        contact, source = self.square([0, 1, 3, 2])
        result = ground_subdivision(contact, source, 3.0)
        self.assertEqual(result["piecewise_overlay_children"], 2)
        self.assertEqual(result["piecewise_overlay_parts"], 4)
        self.assertEqual(result["maximum_parent_height_error_m"], 0)

    def test_real_height_variation_is_checked_over_complete_intersections(self):
        xy, faces, saved = fixture()
        fixed, _ = repair_diagonals(xy, faces[:2], np.ones(2, int))
        vertices = np.c_[xy[:4], saved["quad_heights_m"]]
        result = ground_subdivision(Mesh(vertices, fixed), Mesh(vertices, faces[:2]), 1)
        self.assertLess(result["maximum_parent_height_error_m"], 1e-7)
        self.assertGreater(result["maximum_parent_height_error_m"], 1e-9)

    def test_matching_source_vertices_do_not_hide_an_interior_height_error(self):
        contact, source = self.square([0, 0, 1, 0])
        np.testing.assert_array_equal(contact.vertices, source.vertices)
        with self.assertRaisesRegex(ValueError, "original height bound"):
            ground_subdivision(contact, source, 1.0)

    def test_missing_parent_region_is_not_declared_free_or_covered(self):
        contact, source = self.square([0, 1, 3, 2])
        missing = Mesh(source.vertices, source.triangles[:1])
        with self.assertRaisesRegex(ValueError, "complete original coverage"):
            ground_subdivision(contact, missing, 3.0)


if __name__ == "__main__":
    unittest.main()
