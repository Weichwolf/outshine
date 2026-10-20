"""Stored-input intervention, provenance and geometry/contact controls (#935)."""

import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from water_sliver_edge import contract_edge, exact_area
from water_sliver_replay import geometry_report, load_fixture, original_mesh


class WaterSliver(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.arrays, cls.case = load_fixture(
            Path(__file__).parent / "fixtures/native-geos-3141"
        )
        cls.mesh = original_mesh(cls.arrays)

    def contraction(self, **kwargs):
        return contract_edge(
            self.mesh,
            self.arrays["rows"],
            self.arrays["primary_rows"],
            [713, 714],
            maximum_shift_m=1e-12,
            **kwargs,
        )

    def test_exact_binary64_area_detects_positive_one_ulp_face(self):
        triangle = self.mesh.vertices[self.mesh.triangles[257]]
        self.assertEqual(
            abs(triangle[0, 0] - triangle[1, 0]), abs(np.spacing(triangle[0, 0]))
        )
        self.assertGreater(exact_area(triangle), Fraction(0))
        self.assertAlmostEqual(
            float(exact_area(triangle)), 2.359409320975948e-13, delta=1e-28
        )

    def test_surviving_faces_and_all_rows_retain_exact_original_indices(self):
        result = self.contraction()
        np.testing.assert_array_equal(result.removed_faces, [257])
        np.testing.assert_array_equal(
            result.mesh.vertices, self.mesh.vertices[result.vertices]
        )
        np.testing.assert_array_equal(
            result.rows[:, 1:], self.arrays["rows"][result.row_indices, 1:]
        )
        np.testing.assert_array_equal(
            result.rows[result.primary, 0],
            np.repeat(np.arange(len(result.faces))[:, None], 2, axis=1),
        )
        self.assertTrue(result.report["positive_remaining_exact_areas"])
        self.assertEqual(result.report["changed_original_faces"], [257, 259, 653, 895])
        np.testing.assert_array_equal(self.mesh.vertices, self.arrays["target_enu_m"])

    def test_island_is_unchanged_and_microscopic_hole_loss_is_explicit(self):
        result = self.contraction()
        report, offsets, members = geometry_report(self.mesh, result, self.arrays)
        self.assertEqual((report["holes_before"], report["holes_after"]), (2, 1))
        self.assertGreater(report["hole_areas_before_exact_m2"][0], 0)
        self.assertTrue(report["largest_island_ring_identical"])
        self.assertTrue(report["all_original_source_ids_retained"])
        self.assertEqual(report["removed_face_source_members"], [[0, 1, 3]])
        self.assertEqual(len(members), 1616)
        for j, i in enumerate(result.faces):
            np.testing.assert_array_equal(
                members[offsets[j] : offsets[j + 1]],
                self.arrays["source_members"][
                    slice(*self.arrays["source_offsets"][i : i + 2])
                ],
            )

    def test_hard_contacts_at_either_endpoint_prevent_contraction(self):
        for index in (713, 714):
            with self.assertRaisesRegex(ValueError, "protected vertex contact"):
                self.contraction(protected=[index])

    def test_shift_bound_and_actual_edge_are_required(self):
        for pair, bound in (([713, 714], 0), ([0, 1493], 1e10)):
            with self.assertRaises(ValueError):
                contract_edge(
                    self.mesh,
                    self.arrays["rows"],
                    self.arrays["primary_rows"],
                    pair,
                    maximum_shift_m=bound,
                )

    def test_small_positive_unrelated_face_is_not_deleted(self):
        mesh = Mesh(
            [
                [0, 0, 0],
                [1e-15, 0, 0],
                [0, 1, 0],
                [10, 0, 0],
                [10 + 1e-7, 0, 0],
                [10, 1, 0],
            ],
            [[0, 1, 2], [3, 4, 5]],
        )
        rows = np.array(
            [
                [0, 1, 0, 0, -1, 1],
                [1, 1, 0, 0, -1, 1],
                [0, 0, 1, 0, -1, 1],
                [1, 0, 1, 0, -1, 1],
            ],
            dtype=float,
        )
        result = contract_edge(
            mesh, rows, [[0, 2], [1, 3]], [0, 1], maximum_shift_m=1e-12
        )
        np.testing.assert_array_equal(result.faces, [1])
        self.assertLess(
            float(exact_area(result.mesh.vertices[result.mesh.triangles[0]])), 1e-6
        )
        self.assertGreater(
            float(exact_area(result.mesh.vertices[result.mesh.triangles[0]])), 0
        )

    def test_swept_bound_covers_tangential_motion_of_a_tall_triangle(self):
        mesh = Mesh(
            [[1e-9, 100, 0], [0, 100, 0], [0, 99, 0], [0, 0, 0], [1, 0, 0]],
            [[0, 1, 2], [1, 3, 4]],
        )
        rows = np.array([[0, 1, 0, 0, -1, 1], [1, 1, 0, 0, -1, 1]], dtype=float)
        result = contract_edge(
            mesh, rows, [[0, 0], [1, 1]], [0, 1], maximum_shift_m=1e-8
        )
        old = shapely.union_all(shapely.polygons(mesh.vertices[mesh.triangles, :2]))
        new = shapely.union_all(
            shapely.polygons(result.mesh.vertices[result.mesh.triangles, :2])
        )
        self.assertGreater(old.symmetric_difference(new).area, 1e-8)
        self.assertLessEqual(
            old.symmetric_difference(new).area,
            result.report["swept_symmetric_difference_bound_m2"],
        )


if __name__ == "__main__":
    unittest.main()
