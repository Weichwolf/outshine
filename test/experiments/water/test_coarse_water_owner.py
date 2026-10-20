"""Source-only meshing and an independent affine/hat overlay error control."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from adaptive_water_boundary import refine_boundary
from compare_coarse_water_owner import parent_cut_error, solve
from outshine_ref.contracts import Mesh
from physical_boundary import canonical_cut
from river_surface import surface_mesh
from water_field_difference import compare_fields


def square(values, alternate=False):
    xy = np.array([[0.0, 0.0], [1, 0], [1, 1], [0, 1]])
    faces = np.array(
        [[0, 1, 3], [1, 2, 3]] if alternate else [[0, 1, 2], [0, 2, 3]], dtype=np.uint32
    )
    return Mesh(np.c_[xy, values], faces)


class CoarseWaterOwner(unittest.TestCase):
    def test_source_only_keeps_hole_and_fits_budget_without_render_refinement(self):
        body = shapely.Polygon(
            [(0, 0), (100, 0), (100, 80), (0, 80)],
            holes=[[(20, 20), (40, 20), (40, 40), (20, 40)]],
        )
        with patch(
            "river_surface.shapely.segmentize",
            side_effect=AssertionError("premature render refinement"),
        ):
            mesh, report = surface_mesh(body, None, vertex_limit=8, face_limit=8)
        self.assertEqual(len(mesh.vertices), 8)
        self.assertEqual(report["holes"], 1)
        self.assertEqual(report["missing_or_excess_area_m2"], 0)
        np.testing.assert_array_equal(
            np.unique(mesh.vertices[:, :2], axis=0),
            np.unique(shapely.get_coordinates(body), axis=0),
        )
        with self.assertRaises(ValueError):
            surface_mesh(body, 20, vertex_limit=8, face_limit=8)

    def test_affine_field_error_integral_is_known_independently(self):
        first = square([499, 500, 502, 501])
        second = square([500] * 4, True)
        result = compare_fields(first, second).report
        self.assertAlmostEqual(result["maximum_enu_up_difference_m"], 2.0, places=12)
        self.assertAlmostEqual(
            result["integrated_enu_up_rms_m"], np.sqrt(2 / 3), places=12
        )
        self.assertAlmostEqual(result["mean_enu_up_difference_m"], 0.5, places=12)
        self.assertAlmostEqual(result["overlay_area_m2"], 1.0, places=12)

    def test_same_plane_survives_an_independent_tessellation(self):
        values = [500, 501, 503, 502]
        result = compare_fields(square(values), square(values, True)).report
        self.assertEqual(result["maximum_enu_up_difference_m"], 0)
        self.assertEqual(result["integrated_enu_up_rms_m"], 0)

    def test_overlay_finds_interior_peak_that_corner_samples_miss(self):
        vertices = np.array(
            [[0, 0, 500], [1, 0, 500], [1, 1, 500], [0, 1, 500], [0.5, 0.5, 501]]
        )
        faces = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]], dtype=np.uint32)
        result = compare_fields(Mesh(vertices, faces), square([500] * 4)).report
        self.assertEqual(result["maximum_enu_up_difference_m"], 1)
        self.assertAlmostEqual(
            result["integrated_enu_up_rms_m"], np.sqrt(1 / 6), places=12
        )
        self.assertEqual(result["worst"]["xy_m"], [0.5, 0.5])

    def test_missing_area_overlap_and_budget_are_unproved(self):
        mesh = square([500] * 4)
        with self.assertRaisesRegex(ValueError, "complete XY"):
            compare_fields(mesh, Mesh(mesh.vertices, mesh.triangles[:1]))
        with self.assertRaisesRegex(ValueError, "overlap"):
            compare_fields(
                mesh, Mesh(mesh.vertices, np.repeat(mesh.triangles[:1], 2, axis=0))
            )
        with self.assertRaisesRegex(ValueError, "budget"):
            compare_fields(mesh, mesh, pair_limit=1)

    def test_source_midpoints_are_bounded_and_preserve_ring_vertices(self):
        body = shapely.box(0, 0, 2, 2)
        result, report = refine_boundary(body, lambda xy: xy[:, 0] ** 2, 0.05)
        self.assertTrue(result.equals(body))
        self.assertGreater(report["split_edges"], 0)
        for point in shapely.get_coordinates(body):
            self.assertTrue(
                np.any(np.all(shapely.get_coordinates(result) == point, axis=1))
            )
        with self.assertRaisesRegex(ValueError, "budget"):
            refine_boundary(body, lambda xy: xy[:, 0] ** 2, 0.001, rounds=1)

    def test_midpoint_indicator_does_not_certify_unsampled_source_values(self):
        body = shapely.box(0, 0, 1, 1)
        result, report = refine_boundary(
            body, lambda xy: np.sin(4 * np.pi * xy[:, 0]), 1e-6
        )
        self.assertEqual(report["split_edges"], 0)
        self.assertTrue(result.equals(body))
        self.assertAlmostEqual(float(np.sin(4 * np.pi * 0.125)), 1.0)

    def test_work_budget_can_resolve_local_detail_beyond_fixed_depth(self):
        body = shapely.box(0, 0, 1, 1)
        sample = lambda xy: np.sqrt(xy[:, 0])
        with self.assertRaisesRegex(ValueError, "budget"):
            refine_boundary(body, sample, 0.01, rounds=8)
        result, report = refine_boundary(body, sample, 0.01, rounds=None)
        self.assertTrue(result.equals(body))
        self.assertGreater(report["passes"], 8)
        self.assertLessEqual(report["output_boundary_vertices"], 4096)
        self.assertLessEqual(report["height_point_queries"], 16384)
        with self.assertRaises(ValueError):
            refine_boundary(body, sample, 0.01, vertex_limit=8.5)

    def test_draw_cut_keeps_authoritative_parent_planes(self):
        mesh = square([500, 501, 503, 502])
        cut = canonical_cut(mesh, 0.3)
        self.assertLessEqual(
            parent_cut_error(mesh, cut)["maximum_point_up_error_m"], 1e-9
        )
        from dataclasses import replace

        moved = cut.vertices.copy()
        moved[:, 2] += 0.01
        with self.assertRaises(ValueError):
            parent_cut_error(mesh, replace(cut, vertices=moved))

    def test_explicit_physical_contact_is_hard_before_fine_geometry(self):
        zero, _ = surface_mesh(shapely.box(-40, -30, 40, 30), None)
        vertices = zero.vertices.copy()
        vertices[:, 2] = 500
        mesh = Mesh(vertices, zero.triangles)
        n = len(mesh.triangles)
        rows = np.r_[
            np.c_[np.arange(n), np.tile([1, 0, 0, -0.01, 0.01], (n, 1))],
            np.c_[np.arange(n), np.tile([0, 1, 0, -0.01, 0.01], (n, 1))],
        ]
        result = solve(
            mesh, [9.6, 47.23], rows, [480, 520], contact_points=[[0, 0, 500]]
        )
        self.assertLessEqual(result.report["fixed_point_up_error_m"], 1e-9)
        with self.assertRaises(ValueError):
            solve(
                mesh,
                [9.6, 47.23],
                rows,
                [480, 520],
                contact_points=[[0, 0, 500], [0, 0, 501]],
            )


if __name__ == "__main__":
    unittest.main()
