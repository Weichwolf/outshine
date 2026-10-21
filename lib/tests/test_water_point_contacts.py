"""Real physical point constraints, independent plane residuals and bounded rejection."""

import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.geodetic import query_facet


class WaterPointContactTests(unittest.TestCase):
    def inputs(self):
        mesh = Mesh(
            np.array([[0, 0, 500], [10, 0, 500], [0, 10, 500], [10, 10, 500.0]]),
            np.array([[0, 1, 2], [1, 3, 2]]),
        )
        rows = np.array(
            [
                [0, 1, 0, 0, -0.1, 0],
                [1, 1, 0, 0, -0.1, 0],
                [0, 0, 1, 0, -0.005, 0.005],
                [1, 0, 1, 0, -0.005, 0.005],
            ]
        )
        return mesh, rows, np.array([[0, 2], [1, 3]])

    def test_interior_and_shared_edge_contacts_preserve_geometry_and_height(self):
        mesh, rows, primary = self.inputs()
        points = np.array([[2, 2, 500], [5, 5, 499.99]])
        frozen = points.copy()
        for solver, method in (
            ("clarabel", None),
            ("clarabel", primary),
            ("osqp", primary),
        ):
            fit = fit_physical_surface(
                mesh,
                [0, 0],
                rows,
                up_bounds_m=[490, 510],
                contact_points=points,
                primary_rows=method,
                solver=solver,
                iterations=50000,
            )
            self.assertIsNotNone(fit.mesh, fit.report)
            self.assertTrue(fit.report["finite_qp_optimal"])
            self.assertEqual(fit.report["equality_contacts"], 2)
            for point, owners in zip(points, ([0], [0, 1]), strict=True):
                for face in fit.mesh.triangles[owners]:
                    v = fit.mesh.vertices[face]
                    normal = np.cross(v[1] - v[0], v[2] - v[0])
                    self.assertLess(abs(normal @ (point - v[0])) / abs(normal[2]), 1e-9)
            np.testing.assert_array_equal(
                fit.mesh.vertices[:, :2], mesh.vertices[:, :2]
            )
            np.testing.assert_array_equal(fit.mesh.triangles, mesh.triangles)
        np.testing.assert_array_equal(points, frozen)

    def test_conflicting_point_contacts_are_not_relaxed(self):
        mesh, rows, primary = self.inputs()
        for solver in ("clarabel", "osqp"):
            fit = fit_physical_surface(
                mesh,
                [0, 0],
                rows,
                up_bounds_m=[490, 510],
                primary_rows=primary,
                contact_points=[[2, 2, 500], [2, 2, 501]],
                solver=solver,
                iterations=50000,
            )
            self.assertIsNone(fit.mesh)

    def test_vertex_pin_and_point_contact_share_same_physical_field(self):
        mesh, rows, primary = self.inputs()
        fit = fit_physical_surface(
            mesh,
            [0, 0],
            rows,
            up_bounds_m=[490, 510],
            primary_rows=primary,
            fixed={0: 500.0},
            contact_points=[[0, 0, 501.0]],
        )
        self.assertIsNone(fit.mesh)

    def test_contact_outside_height_bounds_does_not_reach_solver(self):
        mesh, rows, primary = self.inputs()
        with patch(
            "outshine_ref.water._program.clarabel.DefaultSolver",
            side_effect=AssertionError,
        ):
            fit = fit_physical_surface(
                mesh,
                [0, 0],
                rows,
                up_bounds_m=[490, 510],
                primary_rows=primary,
                contact_points=[[2, 2, 1e308]],
            )
        self.assertIsNone(fit.mesh)
        self.assertEqual(fit.report["reason"], "point_contact_height_outside_bounds")

    def test_outside_contacts_are_not_snapped_to_the_surface(self):
        mesh, rows, primary = self.inputs()
        with patch(
            "outshine_ref.water._program.clarabel.DefaultSolver",
            side_effect=AssertionError,
        ):
            fit = fit_physical_surface(
                mesh,
                [0, 0],
                rows,
                up_bounds_m=[490, 510],
                primary_rows=primary,
                contact_points=[[10 + 1e-10, 0, 500]],
            )
        self.assertIsNone(fit.mesh)
        self.assertEqual(
            fit.report["unresolved_point_contacts"],
            [{"index": 0, "reason": "outside_surface"}],
        )

    def test_ambiguous_overlapping_sheets_do_not_choose_a_height(self):
        mesh, rows, primary = self.inputs()
        overlap = Mesh(
            np.r_[mesh.vertices[:3], mesh.vertices[:3] + [0, 0, 1]],
            np.array([[0, 1, 2], [3, 4, 5]]),
        )
        fit = fit_physical_surface(
            overlap,
            [0, 0],
            rows,
            up_bounds_m=[490, 510],
            primary_rows=primary,
            contact_points=[[2, 2, 500]],
        )
        self.assertIsNone(fit.mesh)
        self.assertEqual(
            fit.report["unresolved_point_contacts"][0]["reason"],
            "ambiguous_projected_contact",
        )

    def test_large_contact_input_is_rejected_before_mapping_or_copy(self):
        mesh, rows, primary = self.inputs()
        with patch(
            "outshine_ref.water._contacts.SurfaceSampler", side_effect=AssertionError
        ):
            fit = fit_physical_surface(
                mesh,
                [0, 0],
                rows,
                up_bounds_m=[490, 510],
                primary_rows=primary,
                contact_points=np.zeros((4097, 3)),
            )
        self.assertIsNone(fit.mesh)
        self.assertEqual(fit.report["reason"], "point_contact_budget_exhausted")

    def test_malformed_or_nonfinite_coordinates_are_rejected(self):
        mesh, rows, primary = self.inputs()
        for points in (
            [[1, 2]],
            [[1, 2, np.inf]],
            np.zeros((2, 4)),
            [[np.ones(99999), 1, 2]],
        ):
            with self.assertRaises(ValueError):
                fit_physical_surface(
                    mesh,
                    [0, 0],
                    rows,
                    up_bounds_m=[490, 510],
                    primary_rows=primary,
                    contact_points=points,
                )

    def test_geodetic_query_hits_the_explicit_equatorial_physical_contact(self):
        from outshine_ref.water.geodetic import WGS84_A, local_points

        mesh, rows, primary = self.inputs()
        geographic = [[0.00002, 0.00002]]
        point = local_points([0, 0], geographic, 500)
        fit = fit_physical_surface(
            mesh,
            [0, 0],
            rows,
            up_bounds_m=[490, 510],
            primary_rows=primary,
            contact_points=point,
        )
        self.assertIsNotNone(fit.mesh)
        value = query_facet(
            [0, 0],
            fit.mesh.vertices[fit.mesh.triangles[0]],
            geographic,
            metric_height_m=500,
        )
        self.assertAlmostEqual(
            value["plane_height_m"][0], 500, delta=64 * np.spacing(WGS84_A)
        )


if __name__ == "__main__":
    unittest.main()
